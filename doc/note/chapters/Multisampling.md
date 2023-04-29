# Vulkan Tutorial (C++ Ver.)

## 多重采样（Multisampling）

### 介绍

我们的程序现在可以加载多个层次的纹理细节，当渲染物体远离观看者时修复 artifact。图像现在平滑了很多，但是仔细观察你会发现锯齿状的锯齿状图案沿着绘制的几何形状的边缘。这在我们早期的一个程序中特别明显。

这种不希望的效果被称为“混叠”，它是可用的渲染像素数量有限的结果。由于没有分辨率无限的显示器，所以在某种程度上它总是可见的。有很多方法可以解决这个问题，在本章中，我们将重点关注一个更流行的方法:多重采样抗锯齿（MSAA）。

通常渲染时像素基于单一样本决定，大多数情况下样本点在屏幕像素的中心。如果绘制直线的部分经过了特定的像素，但是没有覆盖单个样本，那么那个像素将会被掠过，导致引起锯齿形的“楼梯”效应。

MSAA 所做的是使用每个像素的多个采样点（因此得名）来确定其最终颜色。正如人们所期望的那样，更多的样本导致更好的结果，但是也更昂贵。

在我们的实现中，我们将重点关注使用最大可用样本计数。根据您的应用程序，这可能并不总是最好的方法，如果最终结果满足您的质量要求，为了获得更高的性能，使用更少的样本可能更好。

### 获得可用的样本数

让我们从确定硬件可以使用多少个样本开始。大多数现代 GPU 支持至少 8 个样本，但这个数字不能保证在任何地方都是相同的。我们将通过添加一个新的类成员来跟踪它：
```cpp
...
vk::SampleCountFlagBits m_MSAASamples = vk::SampleCountFlagBits::e1;
...
```

默认情况下，我们将每个像素只使用一个样本，这相当于没有多次采样，在这种情况下，最终的图像将保持不变。可以从与所选物理设备相关联的 `vk::PhysicalDeviceProperties` 中提取样本的确切最大数量。我们正在使用深度缓冲，所以我们必须考虑颜色和深度的样本计数（&）支持的最多样本计数将是我们可以支持的最大值。添加一个函数来获取这些信息：

```cpp

vk::SampleCountFlagBits HelloTriangleApplication::getMaxUsableSampleCount()
{
    vk::PhysicalDeviceProperties physicalDeviceProperties = m_PhysicalDevice.getProperties();
    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64)  return vk::SampleCountFlagBits::e64;
    if (counts & vk::SampleCountFlagBits::e32)  return vk::SampleCountFlagBits::e32;
    if (counts & vk::SampleCountFlagBits::e16)  return vk::SampleCountFlagBits::e16;
    if (counts & vk::SampleCountFlagBits::e8)  return vk::SampleCountFlagBits::e8;
    if (counts & vk::SampleCountFlagBits::e4)  return vk::SampleCountFlagBits::e4;
    if (counts & vk::SampleCountFlagBits::e2)  return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
}
```

现在，我们将使用这个函数在物理设备选择过程中设置 `m_MSAASamples`变量。为此，我们必须稍微修改一下 `pickPhysicalDevice` 函数：
```cpp
if (candidates.rbegin()->first > 0)
{
    m_PhysicalDevice = candidates.rbegin()->second;
    m_MSAASamples = getMaxUsableSampleCount();
}
```

### 设置渲染对象

在 MSAA 中，每个像素在屏幕外缓冲区中采样，然后渲染到屏幕上。这个新的缓冲区与我们一直渲染的常规图像略有不同——它们必须能够在每个像素上存储多个样本。一旦创建了多采样缓冲区，就必须将其解析为默认的帧缓冲（每个像素只存储一个样本）。这就是为什么我们必须创建一个额外的渲染目标并修改我们当前的绘图过程。我们只需要一个渲染目标，因为一次只有一个绘制操作是活动的，就像深度缓冲区一样。添加以下类成员：
```cpp
...
vk::Image m_ColorImage;
vk::DeviceMemory m_ColorImageMemory;
vk::ImageView m_ColorImageView;
...
```

这个新图像必须存储每像素所需的样本数量，所以我们需要在图像创建过程中将这个数字传递给 `vk::ImageCreateInfo`。通过添加 `numSamples` 参数修改 `createImage` 函数：

```cpp
void HelloTriangleApplication::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
    vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling,
    vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, 
    vk::Image& image, vk::DeviceMemory& memory)
{
    ...
    imageInfo.setSamples(numSamples);
    ...
}
```

现在，使用 `vk::SampleCountFlagBits::e1` 更新对该函数的所有调用-随着实现的进展，我们将用适当的值替换它：
```cpp
createImage(m_SwapChainExtent.width, m_SwapChainExtent.height, 1,
    vk::SampleCountFlagBits::e1, depthFormat, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_DepthImage, m_DepthImageMemory);
...
createImage(texWidth, texHeight, m_MipLevels,
    vk::SampleCountFlagBits::e1,
    vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
    vk::ImageUsageFlagBits::eTransferSrc |
    vk::ImageUsageFlagBits::eTransferDst |
    vk::ImageUsageFlagBits::eSampled,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_TextureImage, m_TextureImageMemory);
```

现在创建一个多采样的颜色缓冲。添加 `createColorResources` 函数，并注意我们在这里使用`m_MSAASamples` 作为 `createImage` 的函数参数。我们也只使用一个 mip 级别，因为这是由 Vulkan 规范强制执行的，在每个像素有多个样本的情况下。此外，这个颜色缓冲区不需要mipmaps，因为它不会被用作纹理：
```cpp
void HelloTriangleApplication::createColorResources()
{
    vk::Format colorFormat = m_SwapChainImageFormat;
    createImage(
        m_SwapChainExtent.width,
        m_SwapChainExtent.height,
        1,
        m_MSAASamples,
        colorFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment |
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_ColorImage,
        m_ColorImageMemory);
    m_ColorImageView = createImageView(m_ColorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}
```

为保持一致性，在 `createDepthResources` 前调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createColorResources();
    createDepthResources();
    ...
}
```

现在我们有了一个多采样的颜色缓冲，是时候处理深度了。修改 `createDepthResources` 并更新深度缓冲区使用的样本数量：
```cpp
void HelloTriangleApplication::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();
    createImage(m_SwapChainExtent.width, m_SwapChainExtent.height, 1,
        m_MSAASamples, depthFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_DepthImage, m_DepthImageMemory);

    m_DepthImageView = createImageView(m_DepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

    transitionImageLayout(m_DepthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
}
```

记得销毁：
```cpp
void HelloTriangleApplication::cleanupSwapChain()
{
    m_Device.destroyImageView(m_ColorImageView);
    m_Device.destroyImage(m_ColorImage);
    m_Device.freeMemory(m_ColorImageMemory);
    ...
}
```

更新 `recreateSwapChain` 使得心得颜色图片可以在窗口尺寸缩放时，正确分辨率下重建：
```cpp
void HelloTriangleApplication::recreateSwapChain()
{
    ...
    createGraphicsPipeline();
    createColorResources();
    createDepthResources();
    ...
}
```

### 添加新附件

让我们先处理渲染通道。修改 `createRenderPass` 和更新颜色和深度附件创建信息结构：
```cpp
void HelloTriangleApplication::createRenderPass()
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(m_SwapChainImageFormat)
        .setSamples(m_MSAASamples)
        .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
        ...
    
    depthAttachment.setFormat(findDepthFormat())
        .setSamples(m_MSAASamples)
        ...
}
```

您会注意到我们已经将 `setFinalLayout` 从 `vk::ImageLayout::ePresentSrcKHR` 更改为 `vk::ImageLayout::eColorAttachmentOptimal`。这是因为多分辨率图像不能直接呈现。我们首先需要将它们解析为常规图像。这个要求并不适用于深度缓冲区，因为它不会在任何时候出现。因此，我们只需要为颜色添加一个新的附件，即所谓的解析附件：
```cpp
...
vk::AttachmentDescription colorAttachmentResolve{};
colorAttachmentResolve.setFormat(m_SwapChainImageFormat)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStoreOp(vk::AttachmentStoreOp::eStore)
    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
...
```

渲染通道现在必须指示将多采样彩色图像解析为常规附件。创建一个新的附件引用，它将指向作为解析目标的颜色缓冲区：
```cpp
vk::AttachmentReference colorAttachmentResolveRef{};
colorAttachmentResolveRef.setAttachment(2)
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
```

将 `setResolveAttachments` 子通道结构成员设置为指向新创建的附件引用。这足以让渲染通道定义一个多样本解析操作，让我们将图像渲染到屏幕上：
```cpp
subpass.setResolveAttachments(colorAttachmentResolveRef)
    ...
```

现在使用心得颜色附件更新渲染通道信息结构体：
```cpp
...
std::array<vk::AttachmentDescription, 3> attachments = 
    { colorAttachment, depthAttachment, colorAttachmentResolve };
...
```

修改 `createFramebuffers` 并添加新图像到列表：
```cpp
void HelloTriangleApplication::createFramebuffers()
{
    ...
    vk::ImageView attachments[] = { m_ColorImageView, m_DepthImageView, m_vecSwapChainImageViews[i] };
    ...
}
```

最后提高斯管线使用更多的样本，修改 `createGraphicsPipeline`：
```cpp
...
vk::PipelineMultisampleStateCreateInfo multisampling{};
multisampling.setRasterizationSamples(m_MSAASamples);
...
```

运行程序发现yeah！运行结果会更清楚，也没有锐利的毛边了。

### 提升质量

我们目前的 MSAA 实现有一定的局限性，这可能会影响更详细场景中输出图像的质量。比如，我们目前没有解决由着色器锯齿引起的潜在问题，即 MSAA 只平滑几何形状的边缘，而不是内部填充。这可能会导致这样一种情况，当你在屏幕上渲染一个光滑的多边形时，如果它包含高对比度的颜色，那么应用的纹理仍然看起来有锯齿。解决这个问题的一种方法是启用**采样着色**（sample shading），这将进一步提高图像质量，尽管需要额外的性能成本：
```cpp
void HelloTriangleApplication::createLogicalDevice()
{
    ...
    deviceFeatures.setSampleRateShading(true);
    ...
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    ...
    multisampling.setSampleShadingEnable(true)
        .setMinSampleShading(0.2f);
    ...
}
```

### 总结

待解决的：
+ Push constants
+ Instanced rendering
+ Dynamic uniforms
+ Separate images and sampler descriptors
+ Pipeline cache
+ Multi-threaded command buffer generation
+ Multiple subpasses
+ Compute shaders

目前的程序可以在许多方面进行扩展，如添加 Blinn-Phong 照明，后期处理效果和阴影映射。您应该能够从其他 API 的教程中了解这些效果是如何工作的，因为尽管 Vulkan 具有显式性，但许多概念仍然是相同的。