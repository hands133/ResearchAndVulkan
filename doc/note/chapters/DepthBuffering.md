# Vulkan Tutorial (C++ Ver.)

## 深度缓冲（Depth buffering）

### 介绍

到目前为止，我们使用的几何图形被投影到三维空间中，但它仍然是完全平坦的。在本章中，我们将为位置添加一个 Z 坐标来准备三维网格。我们将使用第三个坐标在当前的正方形上放置一个正方形，以查看当几何不按深度排序时出现的问题。

### 三维几何

修改 `Vertex` 结构体使用三维向量表示位置，并更新 `vk::VertexInputAttributeDescription` 中的 `format`：
```cpp

struct Vertex{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    ...

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].setBinding(0)
            .setLocation(0)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(Vertex, pos));
        ...
    }
};
```

然后修改顶点着色器接受三维坐标并做变换：
```glsl
layout(location = 0) in vec3 inPosition;
...

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0f);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
```

重新编译着色器后，更新 `vertices` 数组并添加 Z 坐标：
```cpp
const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { -0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
};
```

此时结果与之前相同。我们再复制一个正方形的四个顶点，位置在原先顶点的下面：
```cpp

const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f,  0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { {  0.5f, -0.5f,  0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  0.5f,  0.5f,  0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { -0.5f,  0.5f,  0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
    
    { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { {  0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
};

const std::vector<uint16_t> indices = {
    0, 1, 2,
    2, 3, 0,

    4, 5, 6,
    6, 7, 4
};
```

运行发现远的那个（在下面的）跑到了近的（在上面的）的上面。问题是，较低正方形的片段被绘制在较高正方形的片段之上，仅仅是因为它在索引数组中较晚出现。有两种方法可以解决这个问题：
+ 对所有的绘制调用根据深度从后到前排序
+ 使用深度缓冲做深度测试

第一种方法通常用于绘制透明对象，因为与顺序无关的透明度是一个难以解决的挑战。然而，按深度排序片段的问题通常使用**深度缓冲区**（depth buffer）来解决。

深度缓冲区是一个附加附件，它存储每个位置的深度，就像颜色附件存储每个位置的颜色一样。每次光栅化器产生一个片段时，深度测试将检查新片段是否比前一个更接近。如果不是，则丢弃新片段。通过深度测试的片段将自己的深度写入深度缓冲区。可以从片段着色器中操纵这个值，就像你可以操纵颜色输出一样。
```cpp
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
```
GLM生成的透视投影矩阵默认使用 -1.0 到 1.0 的 OpenGL 深度范围。我们需要将其配置为使用 Vulkan 0.0 到 1.0 的范围
 `GLM_FORCE_DEPTH_ZERO_TO_ONE` 定义。

### 深度图像和视图

深度附件是基于图像的，就像颜色附件一样。不同的是，交换链不会自动为我们创建深度图像。我们只需要一个深度图像，因为一次只运行一个绘制操作。深度图像将再次需要三种资源：图像、内存和图像视图。
```cpp
vk::Image m_DepthImage;
vk::DeviceMemory m_DepthImageMemory;
vk::ImageView m_DepthImageView;
```

添加 `createDepthResources` 成员函数设置资源：
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createCommandPool();
    createDepthResources();
    createTextureImage();
    ...
}

void HelloTriangleApplication::createDepthResources() {}
```

创建深度图像相当简单。它应该具有与颜色附件相同的分辨率，由交换链范围定义，适合深度附件的图像使用，最佳平铺和设备本地内存。唯一的问题是：深度图像的正确格式是什么?格式必须包含深度组件，由`vk::Format` 中的 \_D??\_ 表示。

与纹理图像不同，我们不一定需要特定的格式，因为我们不会直接从程序访问纹理。它只需要有合理的精度，至少 24 位在实际应用中是常见的。有几种格式符合这一要求：
+ `vk::Format::eD32Sfloat`：深度值用 32 位浮点数
+ `vk::Format::eD32SfloatS8Uint`：深度值使用 32 位有符号浮点数，模板成分使用 8 位
+ `vk::Format::eD24UnormS8Uint`：深度之使用 24 位浮点数，模板成分使用 8 位

模板组件用于模板测试，这是一项可与深度测试相结合的附加测试。我们将在以后的章节中讨论这个问题。

我们可以简单地使用 `vk::Format::eD32Sfloat` 格式，因为对它的支持非常普遍（请参阅硬件数据库），但是在可能的情况下为我们的应用程序添加一些额外的灵活性是很好的。我们将编写一个 `findSupportedFormat` 函数，它按照从最理想到最不理想的顺序获取候选格式列表，并检查哪个是第一个被支持的：
```cpp
vk::Format HelloTriangleApplication::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    
}
```

格式的支持取决于平铺模式和用法，因此我们还必须将这些作为参数包括在内。格式可以通过 `getFormatProperties` 查询：
```cpp
for (auto format : candidates)
{
    vk::FormatProperties props = m_PhysicalDevice.getFormatProperties(format);
}
```

`vk::FormatProperties` 包含三个成员：
+ `linearTilingFeatures`：线性平铺支持的情形
+ `optimalTilingFeatures`：最优平铺支持的情形
+ `bufferFeatures`：支持缓冲区的情形

这里只有前两个是相关的，我们检查的一个取决于函数的平铺参数：
```cpp
if (tiling == vk::ImageTiling::eLinear &&
    (props.linearTilingFeatures & features) == features)
    return format;
else if (tiling == vk::ImageTiling::eOptimal &&
    (props.optimalTilingFeatures & features) == features)
    return format;
```

如果所有候选格式都不支持所需的用法，则可以返回一个特殊值或简单地抛出异常：
```cpp
vk::Format HelloTriangleApplication::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for (auto format : candidates)
    {
        vk::FormatProperties props = m_PhysicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features)
            return format;
    }
    
    throw std::runtime_error("failed to find unsupported format!");
}
```

使用此函数创建 `findDepthFormat` 辅助函数选择支持深度附件的深度成分的格式：
```cpp
vk::Format HelloTriangleApplication::findDepthFormat()
{
    return findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}
```

在这种情况下，请确保使用 `vk::FormatFeatureFlags` 标志而不是 `vk::ImageUsageFlags`。所有这些候选格式都包含深度组件，但后两种格式还包含模板组件。我们还不会用到那个，但我们确实需要在使用这些格式的图像上执行布局过渡时考虑到那个。添加一个简单的辅助函数，告诉我们所选择的深度格式是否包含一个模板组件：
```cpp
bool HelloTriangleApplication::hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint ||
            format == vk::Format::eD24UnormS8Uint;
}
```

在 `createDepthResource `内部调用查找深度格式函数：
```cpp
vk::Format depthFromat = findDepthFormat();
```

调用 `createImage` 和 `createImageView`：
```cpp
vk::Format depthFormat = findDepthFormat();
createImage(m_SwapChainExtent.width, m_SwapChainExtent.height,
    depthFormat, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eDepthStencilAttachment,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_DepthImage, m_DepthImageMemory);

m_DepthImageView = createImageView(m_DepthImage, depthFormat);
```
然而，`createImageView` 函数目前假设子资源总是 `vk::ImageAspectFlagBits::eColor`，所以我们需要将该字段转换为参数：
```cpp
vk::ImageView HelloTriangleApplication::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(format)
        .setSubresourceRange(vk::ImageSubresourceRange(
            aspectFlags,
            0, 1, 0, 1));
    ...
}
```

更新所有调用到 `createImageView` 的函数：
```cpp
...
for (const auto& image : m_vecSwapChainImages)
    m_vecSwapChainImageViews.emplace_back(createImageView(image, m_SwapChainImageFormat, vk::ImageAspectFlagBits::eColor));
...
m_DepthImageView = createImageView(m_DepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
...
m_TextureImageView = createImageView(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
...
```

这就是创建深度图像的方法。我们不需要映射它或复制另一个图像到它，因为我们将在渲染通道开始时清除它，就像颜色附件一样。

### 显式过渡深度图像

我们不需要显式地将图像的布局转换为深度附件，因为我们会在渲染通道中处理这个问题。但是，为了完整起见，我仍将在本节中描述该过程。如果你愿意，你可以跳过它。

在 `createDepthResources` 函数的末尾调用`transitionImageLayout`，如下所示：
```cpp
transitionImageLayout(m_DepthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
```

`vk::ImageLayout::eUndefined` 可以用作初始布局，因为没有现有的深度图像内容。我们需要更新 `transitionImageLayout` 中的一些逻辑来使用正确的子资源方面：
```cpp
if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
{
    barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);
    if (hasStencilComponent(format))
        barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
}
```

虽然我们不使用模板组件，但我们确实需要将其包含在深度图像的布局过渡中。最后添加正确的访问掩码和管线阶段：
```cpp
if (oldLayout == vk::ImageLayout::eUndefined &&
    newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlags{0})
        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
            newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
} else if (oldLayout == vk::ImageLayout::eUndefined &&
            newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlags{0})
        .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
} else {
    throw std::invalid_argument("unsupported layout transition!");
}
```

读取深度缓冲区以执行深度测试以查看片段是否可见，并在绘制新片段时将其写入深度缓冲区。读取发生在 `vk::PipelineStageFlagBits::eEarlyFragmentTests` 阶段，写入发生在 `vk::PipelineStageFlagBits::eLateFragmentTests` 阶段。您应该选择与指定操作相匹配的最早的管道阶段，以便在需要时将其用作深度附件。

### 渲染通道

我们现在要修改 `createRenderPass`以包含深度附件。首先指定 `vk::AttachmentDescription`：
```cpp
vk::AttachmentDescription depthAttachment{};
depthAttachment.setFormat(findDepthFormat())
    .setSamples(vk::SampleCountFlagBits::e1)
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
```

格式应该与深度图像本身相同。这一次我们不关心存储深度数据（`setStoreOp`），因为它不会在绘制完成后使用。这可能允许硬件执行额外的优化。就像颜色缓冲一样，我们不关心前面的深度内容，所以我们可以使用 `vk::ImageLayout::eUndefined` 作为初始布局。

```cpp
vk::AttachmentReference depthAttachmentRef{};
depthAttachmentRef.setAttachment(1)
    .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
```

为第一个（也是唯一一个）子通道添加对附件的引用：
```cpp
vk::SubpassDescription subpass{};
subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
    .setColorAttachments(colorAttachmentRef)
    .setPDepthStencilAttachment(&depthAttachmentRef);
```

与颜色附件不同，子通道只能使用单个深度（+模板）附件。在多个缓冲区上进行深度测试没有任何意义。
```cpp
std::array<vk::AttachmentDescription, 2> attachments = 
    { colorAttachment, depthAttachment };

vk::RenderPassCreateInfo renderPassInfo{};
renderPassInfo.setAttachments(attachments)
    .setSubpasses(subpass)
    .setDependencies(dependency);
```

然后更新 `vk::RenderPassCreateInfo` 引用附件。

```cpp
vk::SubpassDependency dependency{};
dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
    .setDstSubpass(0)
    .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setSrcAccessMask(vk::AccessFlags{0})
    .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
    .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
```

最后，我们需要扩展我们的子通道依赖，以确保深度图像的转换和作为其加载操作的一部分被清除之间没有冲突。深度映像在早期的片段测试管道阶段第一次被访问，因为我们有一个清空的加载操作，我们应该为写指定访问掩码。

### 帧缓冲

下一步是修改帧缓冲创建，将深度图像绑定到深度附件。转到 `createFramebuffers` 并指定深度图像视图作为第二个附件：
```cpp
vk::ImageView attachments[] = { m_vecSwapChainImageViews[i], m_DepthImageView };
        
vk::FramebufferCreateInfo framebufferInfo{};
framebufferInfo.setRenderPass(m_RenderPass)
    .setAttachments(attachments)
    .setWidth(m_SwapChainExtent.width)
    .setHeight(m_SwapChainExtent.height)
    .setLayers(1);
```

每个交换链图像的颜色附件是不同的，但是相同的深度图像可以被所有人使用，因为由于我们的信号量，只有一个子通道同时运行。

你还需要将调用移动到 `createFramebuffers`，以确保它在深度图像视图实际创建后被调用：
```cpp

void HelloTriangleApplication::initVulkan() {
    ...
    createDepthResources();
    createFramebuffers();
    ...
}
```

### 清除值

因为我们现在有多个带有 `vk::AttachmentLoadOp::eClear` 的附件，所以我们还需要指定多个清除值。转到 `recordCommandBuffer` 并创建一个 `vk::ClearValue` 结构数组：

```cpp
std::array<vk::ClearValue, 2> clearValues{};
clearValues[0].setColor({ 0.0f, 0.0f, 0.0f, 1.0f });
clearValues[1].setDepthStencil({ 1.0f, 0 });
renderPassInfo.setClearValues(clearValues);
```

在 Vulkan 中，深度缓冲区的深度范围为 0.0 到 1.0，其中 1.0 位于远视图平面，0.0 位于近视图平面。深度缓冲区中每个点的初始值应该是最大可能的深度，即 1.0。

注意，`setClearValues` 的顺序应该与附件的顺序相同。

### 深度和模板状态

现在可以使用深度附件了，但是深度测试仍然需要在图形管道中启用。它是通过 `vk::PipelineDepthStencilStateCreateInfo`结构来配置的：
```cpp
vk::PipelineDepthStencilStateCreateInfo depthStencil{};
depthStencil.setDepthTestEnable(true)
    .setDepthWriteEnable(true);
```

`setDepthTestEnable` 字段指定是否应该将新片段的深度与深度缓冲区进行比较，以查看它们是否应该被丢弃。`setDepthWriteEnable` 字段指定是否应该将通过深度测试的片段的新深度写入深度缓冲区。

```cpp
depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
```

`setDepthCompareOp` 字段指定为保留或丢弃片段而执行的比较。我们坚持较低深度=更近的惯例，所以新碎片的深度应该更小。

```cpp
depthStencil.setDepthBoundsTestEnable(false)
    .setMinDepthBounds(0.0f)    // Optional
    .setMaxDepthBounds(1.0f);   // Optional
```

`setDepthBoundsTestEnable`，`setMinDepthBounds` 和 `setMaxDepthBounds` 字段用于可选的深度绑定测试。基本上，这允许您只保留落在指定深度范围内的片段。我们不会使用这个功能。

```cpp
depthStencil.setStencilTestEnable(false)
    .setFront({})   // Optional
    .setBack({});   // Optional
```

最后三个字段配置模板缓冲区操作，我们也不会在本教程中使用它们。如果要使用这些操作，则必须确保 depth/stencil 图像的格式包含 stencil 组件。

```cpp
pipelineInfo.setPDepthStencilState(&depthStencil)
```

更新 `vk::GraphicsPipelineCreateInfo` 结构体以引用我们刚刚填充的深度模板状态。如果呈现通道包含深度模板附件，则必须始终指定深度模板状态。

现在运行程序发现物体深度排序正确了。

### 处理窗口缩放

深度缓冲区的分辨率应该随着窗口放缩而改变，以匹配新颜色附件的分辨率。修改 `recreateSwapChain` 函数以重新创建深度资源：
```cpp

void HelloTriangleApplication::recreateSwapChain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    while(width == 0 || height == 0) {
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        glfwWaitEvents();
    }

    m_Device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
}
```
回收资源：

```cpp
void HelloTriangleApplication::cleanupSwapChain()
{
    m_Device.destroyImageView(m_DepthImageView);
    m_Device.destroyImage(m_DepthImage);
    m_Device.freeMemory(m_DepthImageMemory);
    ...
}
```

Yeah！终于整的差不多了。