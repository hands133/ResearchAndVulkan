# Vulkan Tutorial (C++ Ver.)

## 生成 Mipmaps

### 介绍

程序可以加载 3D 模型了。本章将添加生成 mipmap 的特性。mipmaps 常用于渲染器，Vulkan 提供了创建它们的方式。

mipmaps 是预计算的，图像的向下缩放版本。每个新的图片的宽和高都是前一张图像的一半。mipmaps 是 Level of Detail（LOD）的一种形式。远离摄像机的对象将会从更小的 mip 图像中进行采样。使用更小的图像提升了渲染速度，避免了莫尔文等 artifacts。

### 创建图像

Vulkan 中每个 mip 图像存储在 `vk::Image` 不同的 mip levels。mip 层级 0 是原图像，通常将 0 层级之后的 mip 层级成为 mip 链。

mip 层级的数目通过创建 `vk::Image` 时指定。直到现在，我们总是需要设置为 1。我们需要分局图像维度计算 mip 的数目。首先添加类成员存储层级数：
```cpp
...
uint32_t m_MipLevels;
vk::Image m_TextureImage;
...
```

`m_MipLevels` 的值可以在 `createTextureImage` 加载纹理时找到：
```cpp
int texWidth, texHeight, texChannels;
stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
...
m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
```

`max` 函数选择最大的维度，`log2` 函数计算维度应该被 2 除多少次。`floor` 函数处理最大维数不是 2 的幂次的情况。最后的 +1 表示原始图像应该为一个 mip 层级。

修改 `createImage`，`createImageView` 和 `transitionImageLayout` 函数从而指定 mip 层级。给 `createImage` 函数添加 `mipLevels` 参数：
```cpp

void HelloTriangleApplication::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties, 
    vk::Image& image, vk::DeviceMemory& memory)
{
    ...
    imageInfo.setMipLevels(mipLevels);
    ...
}
```

```cpp
vk::ImageView HelloTriangleApplication::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    ...
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setSubresourceRange(vk::ImageSubresourceRange(
            aspectFlags,
            0, mipLevels, 0, 1));
    ...
}
```

```cpp

void HelloTriangleApplication::transitionImageLayout(vk::Image image, vk::Format format,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
{
    barrier.setSubresourceRange(vk::ImageSubresourceRange(
        vk::ImageAspectFlagBits::eColor,
        0,
        mipLevels,
        0,
        1));
    ...
}
```

更新所有调用的地方：
```cpp
createImage(m_SwapChainExtent.width, m_SwapChainExtent.height, 1,
    depthFormat, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eDepthStencilAttachment,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_DepthImage, m_DepthImageMemory);
...
createImage(texWidth, texHeight, m_MipLevels,
    vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eSampled,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_TextureImage, m_TextureImageMemory);
```

```cpp
m_vecSwapChainImageViews.emplace_back(createImageView(image, m_SwapChainImageFormat, vk::ImageAspectFlagBi
...
m_DepthImageView = createImageView(m_DepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
...
m_TextureImageView = createImageView(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, m_MipLevels);
```

```cpp
transitionImageLayout(m_DepthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
...
transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_MipLevels);
...
transitionImageLayout(m_DepthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
```

### 生成 mipmaps

纹理图像有多个 mip 层级，但是缓冲区只能填充 mip 0 层级，其他层级是不确定的。为了填充这些层级，我们需要从单一层级生成我们想要的数据。使用 `BlitImage` 命令，执行复制、缩放和过滤操作。我们多次调用次命令，将数据分别填充到各个层级。

`BlitImage` 被认为是一个传输操作，所以我们必须通知Vulkan，打算使用纹理图像作为传输的源和目标。在 `createTextureImage` 中添加 `vk::ImageUsageFlagBits::eTransferSrc` 到纹理图像的使用标志：
```cpp
...
createImage(texWidth, texHeight, m_MipLevels,
    vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eTransferSrc |
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eSampled,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_TextureImage, m_TextureImageMemory);
...
```

像其他图像操作一样，`BlitImage` 依赖于它所操作的图像的布局。我们可以将整个图像转换为 `vk::ImageLayout::eGeneral`，但这很可能会很慢。为了获得最佳的性能，源图像应该在 `vk::ImageLayout::eTransferSrcOptimal` 中
目标图像应该在 `vk::ImageLayout::eTransferSDstOptimal` 中。
Vulkan 允许我们独立地转换图像的每个 mip 级别。每个 blit 一次只处理两个 mip 级别，因此我们可以将每个级别转换为 blits 命令之间的最佳布局。

`transitionImageLayout` 只在整个图像上执行布局转换，所以我们需要编写更多的管道屏障命令。删除 `createTextureImage` 中现有的VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL的过渡:

```cpp
transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_MipLevels);
copyBufferToImage(stagingBuffer, m_TextureImage, texWidth, texHeight);

// transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, m_MipLevels);
```

这将使纹理图像的每个级别保持在 `vk::ImageLayout::eTransferDstOptimal`。
在 blit 命令读取完成后，每个级别将转换为 `vk::ImageLayout::eShaderReadOnlyOptimal`。

添加 `generateMipmaps` 生成 mipmap：
```cpp
void HelloTriangleApplication::generateMipmaps(vk::Image image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.setImage(image)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setSubresourceRange(vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            1));
    
    endSingleTimeCommands(commandBuffer);
}
```

这里准备做几次格式过渡，并重用 `vk::IMageMemoryBarrier`，上面代码 中的其他参数保持不变，但是 `subresourceRange.miplevel`、`oldLayout`、`newLayout`、`srcAccessMask` 和 `dstAccessMask` 每次过渡都需要改变。

```cpp
int32_t mipWidth = texWidth;
int32_t mipHeight = texHeight;

for (uint32_t i = 1; i < mipLevels; ++i)
{
    
}
```

此循环记录每一次 `blitImage` 命令，循环变量从 1 开始：

```cpp
barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
    .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
    .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
    .subresourceRange.setBaseMipLevel(i - 1);

commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eTransfer,
    vk::DependencyFlags{0},
    {}, {}, barrier);
```

首先将层级 `i - 1` 过渡到 `vk::ImageLayout::eTransferSrcOptimal`。这次过渡将等待 `i - 1` 层级填充完成，它要么来自前一个 blit 命令，要么来自 `copyBufferToImage`。当前 `blit` 命令将等待此次过渡。

```cpp
vk::ImageBlit blit{};
blit.setSrcOffsets({ { { 0, 0, 0 }, { mipWidth, mipHeight, 1 } } })
    .setSrcSubresource(vk::ImageSubresourceLayers(
        vk::ImageAspectFlagBits::eColor,
        i - 1,
        0,
        1))
    .setDstOffsets({ { { 0, 0, 0 }, {
        mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 } } })
    .setDstSubresource(vk::ImageSubresourceLayers(
        vk::ImageAspectFlagBits::eColor,
        i,
        0,
        1));
```

指定将在 blit 操作中使用的区域。源 mip 层级为 i - 1，目标 mip 层级为 i。`setSrcOffsets` 数组的两个元素决定了数据将被位元化的3D区域。`setDstOffsets` 决定数据将被位进的区域。`setDstOffsets[1]` 的 X 和 Y 维度除以2，因为每个 mip 级别的大小是前一个级别的一半。`setSrcOffsets[1]` 和 `setDstOffsets[1]` 的 Z 维度必须为 1，因为 2D 图像的深度为 1。

```cpp
commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
    image, vk::ImageLayout::eTransferDstOptimal,
    blit, vk::Filter::eLinear);
```

现在记录 blit 命令。注意，`m_TextureImage` 同时用于  `SetSrcImage` 和 `setDstImage` 参数。这是因为我们在同一张图像的不同层次之间进行了位变换。源 mip 层级刚刚从 `createTextureImage` 过渡到 `vk::ImageLayout::eTransferSrcOptimal`，目标级别仍然在 `vk::ImageLayout::eTransferDstOptimal`。

注意，如果你正在使用一个专用的传输队列（如在顶点缓冲区中建议的）:`blitImage` 必须提交到一个具有图形功能的队列。

最后一个参数允许我们指定在 blit 中使用的 `vk::Filter`。我们在这里有和制作 `vk::Sampler` 时相同的过滤选项。我们使用 `vk::Filter::eLinear` 来启用插值。

```cpp

barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
    .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{0},
    {}, {}, barrier);
```

这个屏障将 mip 中 `i - 1` 层级转换为 `vk::ImageLayout::eShaderReadOnlyOptimal`。此转换等待当前 blit 命令完成。所有采样操作都将等待此转换完成。

```cpp
    ...
    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
}
```

在循环结束时，我们将当前的 mip 维数除以 2。我们在除法之前检查每个维度，以确保维度不会变为 0。这可以处理图像不是正方形的情况，因为其中一个 mip 维度会在另一个维度之前到达 1。当这种情况发生时，对于所有剩余的级别，该维度应该保持为 1。

```cpp
barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
    .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
    .subresourceRange.setBaseMipLevel(mipLevels - 1);

commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{0},
    {}, {}, barrier);

endSingleTimeCommands(commandBuffer);
```

在结束命令缓冲区之前，我们再插入一个管道屏障。这个屏障从 `vk::ImageLayout::eTransferDstOptimal` 转换最后一个mip 级别 `vk::AccessFlagBits::eShaderRead`。这不是由循环处理的，因为最后一个 mip 级别永远不会被 blit。

最后在 `createTextureImage` 中添加函数调用 `generateMipmaps`：
```cpp
transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_MipLevels);
copyBufferToImage(stagingBuffer, m_TextureImage, texWidth, texHeight);

// transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, m_MipLevels);

generateMipmaps(m_TextureImage, texWidth, texHeight, m_MipLevels);
```

现在图像 mipmaps 填充完成。

### 线性过滤支持

使用像 `blitImage` 这样的内置函数来生成所有mip级别是非常方便的，但不幸的是，它不能保证在所有平台上都得到支持。它需要纹理图像格式来支持线性过滤器，这可以通过 `getFormatProperties` 函数进行检查。我们将为此在 `generateMipmaps` 函数中添加一个检查。首先添加额外的参数指定图像格式：
```cpp
void HelloTriangleApplication::createTextureImage(){
    ...
    generateMipmaps(m_TextureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, m_MipLevels);
}

void HelloTriangleApplication::generateMipmaps(vk::Image image, vk::Format format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    ...
}
```

函数 `generateMipmaps` 使用 `getFormatProperties` 请求纹理图像格式的特性：
```cpp
void HelloTriangleApplication::generateMipmaps(vk::Image image, vk::Format format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    vk::FormatProperties formatProperties = m_PhysicalDevice.getFormatProperties(format);
    ...
```

`vk::FormatProperties` 结构体有三个名为 `linearTilingFeatures`，`optimalTilingFeatures` 和 `bufferFeatures` 的字段，每个字段都描述了如何根据使用方式使用格式。我们创建了一个具有最佳平铺格式的纹理图像，所以我们需要检查 `optimalTilingFeatures`。对线性滤波特性的支持可以通过 `vk::FormatFeatureFlagBits::eSampledImageFilterLinear` 进行检查:
```cpp
if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
    throw std::runtime_error("texture image format does not support linear blitting!");
```

在这种情况下有两种选择。您可以实现一个函数来搜索常见的纹理图像格式，以寻找支持线性比特化的纹理图像格式，或者您可以使用 `stb_image_resize` 等库在软件中实现 mipmap 生成。然后可以将每个 mip 级别加载到图像中，与加载原始图像的方式相同。

应该注意的是，在运行时生成 mipmap 级别在实践中并不常见。通常它们是预先生成并存储在纹理文件中，以提高加载速度。在软件中实现调整大小和从文件中加载多个级别留给读者作为练习。

### 采样器

当 `vk::Image` 持有 mipmap 数据时，`vk::Sampler` 控制者数据在渲染时的读取方式。Vulkan 允许指定 `minLod`，`maxLod`，`mipLodBias` 和 `mipmapMode`（“Lod”意思是“Level of Detail”）。做纹理采样时，采样器和下面的为代码一样选择一个 mip 层级：
```cpp
lod = getLodLevelFromScreenSize(); // 物体足够近时更小，可能为负值
lod = clamp(lod + mipLodBias, minLod, maxLod);

level = clamp(floor(lod), 0, texture.mipLevels - 1); // 限制到纹理 mip 层级数

if (mipmapMode = vk::SamplerMipmapMode::eNearest) {
    color = sample(level);
} else {
    color = blend(sample(level), sample(level + 1));
}
```

如果 `samplerInfo.mipmapMode` 是 `vk::SamplerMipmapMode::eNearest`，`lod` 选择采样的 mip 层级。如果是 `vk::SamplerMipmapMode::eLinear`，`lod` 用于选择两个采样的 mip 层级，得到的采样结果会被线性混合：

采样操作会被 `lod` 影响：
```cpp
if (lod <= 0)
    color = readTexture(uv, magFilter);
else
    color = readTexture(uv, minFilter);
```

如果物体靠近相机，则使用 `magFilter` 作为过滤器。如果对象离相机较远，则使用 `minFilter`。正常情况下，`lod` 是非负的，当关闭相机时，`lod` 仅为 0。`mipLodBias` 让我们强制Vulkan 使用比它通常使用的更低的 `lod`和 `level`。

已经将 `minFilter` 和 `magFilter` 设置为使用 `vk::Filter::eLinear`。我们只需要选择 `minLod`， `maxLod`，`mipLodBias` 和 `mipmapMode` 的值。

```cpp
void createTextureSampler() {
    ...
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setMinLod(0.0f)    // Optional
    .setMaxLod(static_cast<float>(m_MipLevels))
    .setMipLodBias(0.0f);    // Optional
    ...
}
```

为了允许使用所有 mip 级别，我们设置 `setMinLod` 为 0.0f，并设置 `setMaxLod` 设置为 mip 级别的数量。我们没有理由改变负载值，所以我们将 `mipLodBias` 设置为 0.0f。