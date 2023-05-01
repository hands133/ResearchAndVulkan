# Vulkan Tutorial (C++ Ver.)

## 图像视图和采样

在本章中，我们将创建图形管线采样图像所需的另外两个资源。第一个资源是我们之前在处理交换链图像时已经看到的，但第二个是新的—它涉及到着色器如何从图像中读取纹理。

### 纹理图像视图

我们之前已经看到，使用交换链图像和帧缓冲时，图像是通过图像视图而不是直接访问的。我们还需要为纹理图像创建这样一个图像视图。

添加一个类成员来保存纹理图像的 `vk::ImageView`，并创建一个新函数`createTextureImageView`，我们将在其中创建它：
```cpp
vk::ImageView m_TextureImageView;
...

void HelloTriangleApplication::initVulkan() {
    ...
    createTextureImage();
    createTextureImageView();
    createVertexBuffer();
    ...
}
...
void HelloTriangleApplication::createTextureImageView() {}
```

这个函数的代码可以直接基于 `createImageViews`。你只需要做两个改变：`format` 和 `image`：

```cpp
vk::ImageViewCreateInfo viewInfo{};
viewInfo.setImage(m_TextureImage)
    .setViewType(vk::ImageViewType::e2D)
    .setFormat(vk::Format::eR8G8B8A8Srgb)
    .setSubresourceRange(vk::ImageSubresourceRange(
        vk::ImageAspectFlagBits::eColor,
        0, 1, 0, 1));
```

这里没有指定 `viewInfo.setComponents` 初始化，因为 `vk::ComponentSwizzle::eIdentity` 定义为 0。调用 `createImageView` 创建图像视图：
```cpp
m_TextureImageView = m_Device.createImageView(viewInfo);
if (!m_TextureImageView)
    throw std::runtime_error("failed to create texture image view!");
```

因为代码逻辑和 `createImageViews` 大量重复，抽象为 `createImageView`：
```cpp
vk::ImageView HelloTriangleApplication::createImageView(vk::Image image, vk::Format format)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(format)
        .setSubresourceRange(vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0, 1, 0, 1));
        
    vk::ImageView imageView = m_Device.createImageView(viewInfo);
    if (!imageView) throw std::runtime_error("failed to create texture image view!");

    return imageView;
}
```

`createTextureImageView` 可以被化简为：
```cpp
void HelloTriangleApplication::createTextureImageView()
{
    m_TextureImageView = createImageView(m_TextureImage, vk::Format::eR8G8B8A8Srgb);
}
```

`createImageViews` 函数可以简化为：
```cpp
void HelloTriangleApplication::createImageViews()
{
    m_vecSwapChainImageViews.clear();
    for (const auto& image : m_vecSwapChainImages)
    {
        m_vecSwapChainImageViews.emplace_back(createImageView(image, m_SwapChainImageFormat));
    }
}
```

保证程序结束时清理视图：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyImageView(m_TextureImageView);

    m_Device.destroyImage(m_TextureImage);
    m_Device.freeMemory(m_TextureImageMemory);
    ...
}
```

### 采样器（samplers）

着色器可以直接从图像中读取纹理，但当它们被用作纹理时，这并不常见。纹理通常通过采样器访问，采样器将应用过滤和转换来计算检索到的最终颜色。

这些过滤器有助于处理过采样等问题。考虑一个纹理，它映射到几何体的碎片比纹理多。

如果你通过线性插值组合 4 个最接近的纹理，那么你会得到一个更平滑的结果。当然，你的应用程序可能有像素风的美术风格要求（如《我的世界》），但在传统的图像应用程序中更倾向于插值得到的图像。当从纹理中读取颜色时，采样器对象会自动为您应用此过滤。

下采样是相反的问题，你有更多的纹理而不是片元。这将导致在以尖锐角度采样高频率模式（如棋盘纹理）时产生伪影。纹理在远处变成了一团模糊。解决这个问题的方法是各向异性滤波，它也可以由采样器自动应用。

除了这些过滤器，采样器还可以处理转换。它决定当您尝试通过其寻址模式读取图像外的文本时发生的情况。

创建一个函数 `createTextureSampler` 来设置这样一个采样器对象。稍后，我们将使用该采样器从着色器中的纹理读取颜色。
```cpp

void HelloTriangleApplication::initVulkan() {
    ...
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    ...
}

void HelloTriangleApplication::createTextureSampler()
{
    
}
```

采样器是通过 `vk::SamplerCreateInfo` 结构配置的，该结构规定了应该应用的所有过滤器和转换。

```cpp
vk::SamplerCreateInfo samplerInfo{};
samplerInfo.setMagFilter(vk::Filter::eLinear)
    .setMinFilter(vk::Filter::eLinear);
```

`setMagFilter` 和 `setMinFilter` 字段指定了如何插入放大或缩小的纹理。放大涉及上述过采样问题，而缩小涉及欠采样问题。选项是 `vk::Filter::eLinear` 和 `vk::Filter::eNearest`。

```cpp
samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat)
    setAddressModeV(vk::SamplerAddressMode::eRepeat)
    setAddressModeW(vk::SamplerAddressMode::eRepeat);
```

寻址模式可以使用 `setAddressMode` 字段指定每个轴。下面列出了可用的值。上面的图片展示了其中的大部分。请注意，轴被称为U，V 和 W，而不是 X，Y 和 Z。这是纹理空间坐标的约定。
+ `vk::SamplerAddressMode::eRepeat`：当超出图像尺寸时，重复纹理
+ `vk::SamplerAddressMode::eMirroredRepeat`：就像 `eRepeat`，但在超出尺寸时颠倒坐标以镜像图像
+ `vk::SamplerAddressMode::eClampToEdge`：取图像尺寸之外最接近坐标的边缘的颜色
+ `vk::SamplerAddressMode::eMirrorClampToEdge`：j就像 `eClampToEdge`，但是使用与最近的边相反的边
+ `vk::SamplerAddressMode::eClampToBorder`：当采样超出图像的尺寸时返回纯色

我们在这里使用哪种寻址模式并不重要，因为在本教程中我们不会在图像之外进行采样。然而，重复模式可能是最常见的模式，因为它可以用于瓷砖纹理，如地板和墙壁。

```cpp
samplerInfo.setAnisotropyEnable(true)
.setMaxAnisotropy(???);
```

这两个字段指定是否应该使用各向异性过滤。除非关注性能，否则没有理由不使用它。`setMaxAnisotropy` 限制了可用于计算最终颜色的纹素样本的数量。值越低，性能越好，但结果质量越低。为了找出我们可以使用的值，我们需要像这样检索物理设备的属性：
```cpp
vk::PhysicalDeviceProperties properties = m_PhysicalDevice.getProperties();
```

如果查看 `vk::PhysicalDeviceProperties` 结构的文档，您将看到它包含一个名为 `limits` 的 `vk::PhysicalDeviceLimits` 成员。它有一个叫做 `maxSamplerAnisotropy` 的成员，这是我们可以为 `maxAnisotropy` 指定的最大值。如果我们想要获得最高质量，我们可以直接使用该值：
```cpp
samplerInfo.setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);
```

您可以在程序开始时查询属性并将它们传递给需要它们的函数，或者在 `createTextureSampler` 函数本身中查询它们。

```cpp
samplerInfo.setBorderColor(vk::BorderColor::eIntOpaqueWhite);
```

`setBorderColor` 字段指定在使用夹紧到边界寻址模式对图像进行采样时返回的颜色。可以在 `float` 或 `int` 格式中返回黑色、白色或透明。不能指定任意颜色。

```cpp
samplerInfo.setUnnormalizedCoordinates(false);
```

`setUnnormalizedCoordinates` 字段指定要使用哪个坐标系统来处理图像中的文本。如果这个字段是 `true`，那么您可以简单地使用 `[0,texWidth)` 和 `[0,texHeight)` 范围内的坐标。如果是 `false`，则在所有轴上使用 `[0,1)` 范围对纹理进行寻址。现实世界的应用程序几乎总是使用标准化的坐标，因为这样就有可能在完全相同的坐标下使用不同分辨率的纹理。

```cpp
samplerInfo.setCompareEnable(false)
    .setCompareOp(vk::CompareOp::eAlways);
```

如果启用了比较功能，则首先将纹素与一个值进行比较，然后在过滤操作中使用比较的结果。这主要用于阴影贴图上的百分比过滤。我们将在以后的章节中讨论这个问题。

```cpp
samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear)
    .setMipLodBias(0.0f)
    .setMinLod(0.0f)
    .setMaxLod(0.0f);
```

所有这些领域都适用于映射。我们将在后面的章节中看到mipmapping，但基本上，它是另一种可以应用的过滤器。
采样器的功能现在已经完全确定。添加一个类成员来保存采样器对象的句柄，并使用 `createSampler` 创建采样器：
```cpp
vk::ImageView m_TextureImageView;
vk::Sampler m_TextureSampler;


void HelloTriangleApplication::createTextureSampler()
{
    ...

    m_TextureSampler = m_Device.createSampler(samplerInfo);
    if (!m_TextureSampler)  throw std::runtime_error("failed to create texture sampler!");
}
```

这与许多旧的 API 不同，它们将纹理图像和过滤合并到单个状态中。记得销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroySampler(m_TextureSampler);
    m_Device.destroyImageView(m_TextureImageView);
    ...
}
```

### 各向异性设备特性

如果现在运行程序，验证层会报错：
```bash
[validation] Validation Error: [ VUID-VkSamplerCreateInfo-anisotropyEnable-01070 ] Object 0: handle = 0x555556306150, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x56f192bc | vkCreateSampler(): Anisotropic sampling feature is not enabled, pCreateInfo->anisotropyEnable must be VK_FALSE. The Vulkan spec states: If the samplerAnisotropy feature is not enabled, anisotropyEnable must be VK_FALSE (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-VkSamplerCreateInfo-anisotropyEnable-01070)
```

因为各向异性过滤是可选的设备特性。需要更新 `createLogicalDevice` 函数请求特性：
```cpp
void HelloTriangleApplication::createLogicalDevice()
{
    ...
    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.setSamplerAnisotropy(true);
    ...
}
```

尽管现代显卡不支持它的可能性很小，但我们应该更新`isDeviceSuitable` 以检查它是否可用：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    ...

    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}
```

`getFeatures` 重新使用了 `vk::PhysicalDeviceFeatures` 结构来指示支持哪些特性，而不是通过设置布尔值来请求。

除了强制各向异性过滤的可用性，还可以通过条件设置简单地不使用它：
```cpp
samplerInfo.setAnisotropyEnable(false)
    .setMaxAnisotropy(1.0f);
```

这里还是开启吧。