# Vulkan Tutorial (C++ Ver.)

## 帧缓冲（framebuffer）

之前提到过好多次帧缓冲，创建渲染通道时希望有一个与交换链图片格式相同的帧缓冲。在创建呈现通道期间指定的附件通过将它们包装到 `vk::Framebuffer` 对象绑定。一个帧缓冲对象引用了表示附件的所有 `vk::ImageView` 对象。教程的例子中只有一个颜色附件。但是，必须为附件使用的图像取决于在检索用于表示的图像时交换链返回的图像。意味着必须为交换链中的所有图像创建一个帧缓冲，并在绘制时使用与检索到的图像相对应的帧缓冲。

为此创建另一个 `std::vector` 类成员保存帧缓冲：
```cpp
std::vector<vk::Framebuffer> m_SwapchainFramebuffers;
```

添加新函数 `createFramebuffers` 中为数组创建对象，该函数在创建图形管线后立即在 `initVulkan` 里调用：
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
    createFramebuffers();
}

void HelloTriangleApplication::createFramebuffers() {}
```

并创建帧缓冲：
```cpp
void HelloTriangleApplication::createFramebuffers() {
    m_vecSwapchainFramebuffers.resize(m_vecSwapChainImageViews.size());
    for (size_t i = 0; i < m_vecSwapChainImageViews.size(); ++i) {
        vk::ImageView attachments[] = { m_vecSwapChainImageViews[i] };
        
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.setRenderPass(m_RenderPass)
            .setAttachments(attachments)
            .setWidth(m_SwapChainExtent.width)
            .setHeight(m_SwapChainExtent.height)
            .setLayers(1);
        
        m_vecSwapchainFramebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
        if (!m_vecSwapchainFramebuffers[i])
            throw std::runtime_error("failed to create framebuffer!");
    }
}
```

创建帧缓冲非常简单。首先需要指定帧缓冲要与哪个渲染通道兼容。只能使用与之兼容的呈现通道的帧缓冲，这大致意味着它们使用相同数量和类型的附件。

`setAttachments` 指定 `vk::ImageView` 对象，这些对象应该绑定到渲染通道数组中各自的附件描述。`setWidth` 和 `setHeight` 设置如字面描述，`setLayers`  指图像数组的层数。

应该在图像视图和渲染传递之前销毁帧缓冲，但只有在完成渲染之后：
```cpp
void HelloTriangleApplication::cleanUp() {
    for (auto& framebuffer : m_vecSwapchainFramebuffers)
        m_Device.destroyFramebuffer(framebuffer);
    ...
}
```

现在到了一个里程碑，有了渲染所需的所有对象。在下一章编写第一个实际的绘图命令。