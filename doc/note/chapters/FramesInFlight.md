# Vulkan Tutorial (C++ Ver.)

## 帧（Frames in flight）

### 运行帧

完成的差不多了，但是有个小瑕疵。程序需要等待前一帧完成，然后才能开始渲染下一帧，这导致了主机不必要的空闲（等待）。解决方法是允许多帧同时运行，即允许一帧的渲染不影响下一帧的记录。在呈现期间访问和修改的任何资源都必须复制。因此，我们需要多个命令缓冲区、信号量和围栏。在后面的章节中，我们还将添加其他资源的多个实例。

首先添加一个常量，定义应该并发处理多少帧：
```cpp
const int MAX_FRAMES_IN_FLIGHT = 2;
```

设置为 2 帧是因为我们不希望 CPU 比 GPU 领先太多。有 2 帧在运行，CPU 和 GPU 可以同时处理各自的任务。如果 CPU 提前完成，它会等待 GPU 完成渲染后再提交工作。当有 3 帧或更多帧在运行时，CPU 可能会超过 GPU，增加帧延迟。通常不需要额外的延迟，但是让应用程序控制运行帧的帧数是 Vulkan 显式 API 的另一个例子。

每个帧应该有个自己的命令缓冲区，一些信号量和围栏。将对应的成员修改为容器：
```cpp
// vk::CommandBuffer m_CommandBuffer;
std::vector<vk::CommandBuffer> m_vecCommandBuffers;
// vk::Semaphore m_ImageAvailableSemaphore;
// vk::Semaphore m_RenderFinishedSemaphore;
// vk::Fence m_InFlightFence;
std::vector<vk::Semaphore> m_vecImageAvailableSemaphores;
std::vector<vk::Semaphore> m_vecRenderFinishedSemaphores;
std::vector<vk::Fence> m_vecInFlightFences;
```

创建多个命令缓冲区。重命名 `createCommandBuffer` 为`createCommandBuffers`。然后将命令缓冲区数组调整为`MAX_FRAMES_IN_FLIGHT` 的大小，修改 `vk::CommandBufferAllocateInfo`，然后将目标更改为命令缓冲区数组：
```cpp
void HelloTriangleApplication::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_CommandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
    m_vecCommandBuffers = m_Device.allocateCommandBuffers(allocInfo);
}
```

修改 `createSyncObjects` 函数：
```cpp
void HelloTriangleApplication::createSyncObjects()
{
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_vecImageAvailableSemaphores.emplace_back(m_Device.createSemaphore(semaphoreInfo));
        m_vecRenderFinishedSemaphores.emplace_back(m_Device.createSemaphore(semaphoreInfo));
        m_vecInFlightFences.emplace_back(m_Device.createFence(fenceInfo)):
    }
}
```

`cleanUp` 函数中做出对应的修改：
```cpp
void HelloTriangleApplication::cleanUp() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_Device.destroySemaphore(m_vecImageAvailableSemaphores[i]);
        m_Device.destroySemaphore(m_vecRenderFinishedSemaphores[i]);
        m_Device.destroyFence(m_vecInFlightFences[i]);
    }
    ...
}
```

注意命令缓冲区不需要我们在回收命令池时回收，不需要额外的回收命令缓冲操作。

添加成员变量标定当前帧：
```cpp
uint32_t m_CurrentFrame = 0;
```

修改 `drawFrame` 函数：
```cpp
void HelloTriangleApplication::drawFrame()
{
    const auto& _ = m_Device.waitForFences(m_vecInFlightFences[m_CurrentFrame], true, std::numeric_limits<uint64_t>::max());
    m_Device.resetFences(m_vecInFlightFences[m_CurrentFrame]);

    const auto& value = m_Device.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_vecImageAvailableSemaphores[m_CurrentFrame], nullptr);
    uint32_t imageIndex = value.value;

    m_vecCommandBuffers[m_CurrentFrame].reset(vk::CommandBufferResetFlags{0});
    recordCommandBuffer(m_vecCommandBuffers[m_CurrentFrame], imageIndex);

    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = { m_vecImageAvailableSemaphores[m_CurrentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.setWaitSemaphores(waitSemaphores)
        .setWaitDstStageMask(waitStages)
        .setCommandBuffers(m_vecCommandBuffers[m_CurrentFrame]);

    vk::Semaphore signalSemaphores[] = { m_vecRenderFinishedSemaphores[m_CurrentFrame] };
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_GraphicsQueue.submit(submitInfo, m_vecInFlightFences[m_CurrentFrame]);
    ...
    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

最后需要循环递增 `m_CurrentFrame`。

现在实现了所需的同步，确保队列中的运行帧不超过`MAX_FRAMES_IN_FLIGHT`，并且这些帧不会相互跨过。注意代码的其他部分（如最终的清理），可以依赖更粗略的同步，如`m_Device.waitIdle()`，用户应根据性能需求决定使用哪种方法。

注意 `recordCommandBuffer` 函数也要修改，不知道作者是忘了还是怎么的：
```cpp
void HelloTriangleApplication::recordCommandBuffer(vk::CommandBuffer, uint32_t imageIndex)
{
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlags{0})  // Optional
        .setPInheritanceInfo(nullptr);  // Optional
        
    auto& commandBuffer = m_vecCommandBuffers[m_CurrentFrame];
    commandBuffer.begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(m_RenderPass)
        .setFramebuffer(m_vecSwapchainFramebuffers[imageIndex])
        .setRenderArea(vk::Rect2D({0, 0}, m_SwapChainExtent));

    vk::ClearValue clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    renderPassInfo.setClearValues(clearColor);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();
    commandBuffer.end();
}
```