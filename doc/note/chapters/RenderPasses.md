# Vulkan Tutorial (C++ Ver.)

## 渲染通道（Render passes）

### 设置

在创建管线前，我们需要告诉 Vulkan 在呈现时将使用的帧缓冲附件。需要指定有多少颜色和深度缓冲区，每个缓冲区使用多少样本，以及在整个渲染操作中如何处理的内容。所有信息都包装在一个渲染传递对象中，为此创建 `createRenderPass` 函数，并在`initVulkan` 函数中的 `createGraphicsPipeline` 之前调用它：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
}

void HelloTriangleApplication::createRenderPass(){}
```

### 附件描述（Attachment description）

我们的情况是需要一个颜色缓冲附件，表示来自交换链中的一张图像。
```cpp
void HelloTriangleApplication::createRenderPass()
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(m_SwapChainImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1);
}
```

颜色附件的 `format` 应该与交换链图像的格式匹配。这里不修改多重采样设置，使用 1 样本/像素。
```cpp
colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eStore);
```

`setLoadOp` 和 `setStoreOp` 表示在渲染前后对帧缓冲的数据的行为。对于 `setLoadOp` 有几种选择：
+ `vk::AttachmentLoadOp::eLoad`：保留附件的现有内容
+ `vk::AttachmentLoadOp::eClear`：开始时清空值为常量
+ `vk::AttachmentLoadOp::eDontCare`：现有内容不确定，不关心

例子中使用 `clear` 操作在绘制新帧之前将帧缓冲清除为黑色。`setStoreOp` 只有两种可能（实际上有不少，可能其他的需要开启 GPU 特性？或者是教程太旧了）：   
+ `vk::AttachmentStoreOp::eStore`：渲染内容将存储在内存中，可以稍后读取
+ `vk::AttachmentStoreOp::eDontCare`：帧缓冲的内容在渲染操作后未定义

考虑到渲染三角形到屏幕上，这里将采用 `store` 操作。
```cpp
colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
```

`setLoadOp` 和 `setStoreOp` 用在颜色和深度值上，`setStencilLoadOp` 和 `setStencilStoreOp` 应用在模板数据上。本程序不会涉及到模板缓冲，所以结果和加载和存储无关。

```cpp
colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined)
    .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
```

Vulkan 中的纹理和帧缓冲由固定格式的 `vk::Image` 对象表示，然而内存中的像素布局可以基于对图像的操作发生改变。一些最常用的布局有：
+ `vk::ImageLayout::eColorAttachmentOptimal`：图像用于颜色附件
+ `vk::ImageLayout::ePresentSrcKHR`：交换链中用于呈现的图片
+ `vk::ImageLayout::eTransferDstOptimal`：图像用于内存拷贝操作的目标

在纹理章节中会更深入地讨论。图像需要过渡到适合下一步操作的特定布局。

`setInitialLayout` 指定在渲染通道开始之前图像具有的布局。`setFinalLayout` 指定渲染传递结束时要自动转换到的布局。对于 `setInitialLayout` 使用 `vk::ImageLayout::eUndefined` 表示不关心图像之前的布局。需要注意的是图像的内容不保证会被保留。希望图像在呈现后使用交换链准备好呈现，所以使用 `vk::ImageLayout::ePresentSrcKHR` 作为最终布局。

### 子通道和附件引用

一个渲染通道由多个子通道组成。子通道依赖于之前传递中的帧缓冲内容的后续呈现操作，例如一个接一个应用的后处理效果序列。如果将这些呈现操作分组到一个呈现通道中，那么 Vulkan 能够重新排列这些操作并节省内存带宽，从而可能获得更好的性能。对于第一个三角形，这里只使用一个子通道。

每个子通道引用一个或多个附件，这些附件是前几节中使用结构描述的。这些引用就是 `vk::AttachmentReference` 结构体：
```cpp
vk::AttachmentReference colorAttachmentRef{};
colorAttachmentRef.setAttachment(0)
    .setLayout(vk::ImageLayout::eColorAttachmentOptimal);
```

`setAttachment` 基于下标指定附件描述数组中哪一个附件要被引用。我们的数组由单一 `vk::AttachmentDescription` 组成，所以下标为 0。`setLayout` 布局指定在使用此引用的子通道期间，我们希望附件具有哪种布局。当子通道启动时，Vulkan 自动将附件转换到此布局。这里计划使用附件作为颜色缓冲区，`vk::ImageLayout::eColorAttachmentOptimal` 布局将提供最佳性能。

子通道通过 `vk::SubpassDescription` 结构体描述：
```cpp
vk::SubpassDescription subpass{};
subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
```
Vulkan 也支持计算子通道，所以需要显式指定当前为图形子通道。指定颜色附件的引用：
```cpp
subpass.setColorAttachments(colorAttachmentRef);
```

此数组中的附件下标直接被片元着色器中的 `layout(location = 0) out vec4 outColor` 引用。子通道还可以引用其他类型的附件：
+ `setInputAttachments`：从着色器中读取的附件
+ `setResolveAttachments`：用于对颜色附件多重采样的附件
+ `setPDepthStencilAttachment`：用于深度和模板数据的附件
+ `setPreserveAttachments`：不用于此子通道的附件，但是数据需要保留

### 渲染通道（Render pass）

对附件和引用它的子通道进行描述后，就可以创建呈现通道了。创建一个新的类成员变量，将 `vk::RenderPass` 对象放在`vk::PipelineLayout` 的上面:
```cpp
vk::RenderPass m_RenderPass;
vk::PipelineLayout m_PipelineLayout;
```

渲染通道对象可以直接由使用一系列附件和子通道的数组填充后的结构体 `vk::RenderPassCreateInfo` 创建：
```cpp
vk::RenderPassCreateInfo renderPassInfo{};
renderPassInfo.setAttachments(colorAttachment)
    .setSubpasses(subpass);
m_RenderPass = m_Device.createRenderPass(renderPassInfo);
if (!m_RenderPass)  throw std::runtime_error("failed to create render pass!");
```

在程序退出时销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroyPipelineLayout(m_PipelineLayout);
    m_Device.destroyRenderPass(m_RenderPass);
    ...
```