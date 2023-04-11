# Vulkan Tutorial (C++ Ver.)

## 渲染和呈现

本章将编写将在渲染循环中调用的 `drawFrame` 函数并将三角形绘制到屏幕上：

```cpp
void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
        drawFrame();
    }
}
...
void HelloTriangleApplication::drawFrame() {}
```

### 帧框架

高层级讲，在 Vulkan 中渲染一帧包括常用的几步：
+ 等待前一帧结束
+ 请求交换链中的一张图像
+ 记录将场景绘制到图像的命令缓冲区
+ 提交到记录的命令缓冲区
+ 呈现交换链图片

虽然在后面的章节中才扩展绘制函数，但现在这是渲染循环的核心。

### 同步（Synchronization）

Vulkan 的核心设计理念是：GPU 上的执行同步是明确的。操作的顺序由我们使用各种同步原语来定义，这些原语告诉驱动程序想要运行的顺序。这意味着许多开始在 GPU 上执行工作的 Vulkan API调用是异步的，函数将在操作完成之前返回。

在本章中，我们需要明确地对事件进行排序，因为它们发生在 GPU上，例如：
+ 从交换链请求图像
+ 执行绘制到请求图像上的命令
+ 呈现图像，并返还给交换链

这些事件中每一个都使用单个函数调用进行设置，但都是异步执行的。函数调用将在操作实际完成之前返回，并且执行顺序未定义。这是不幸的，因为每一个操作都依赖于前一个操作的完成。因此，我们需要探索可以使用哪些原语来实现所需的排序。

#### 信号量（Semaphores）

信号量在队列操作之间添加顺序。队列操作指的是提交给队列的工作，可以是在命令缓冲区中，也可以是在函数中。队列有图形队列和表示队列。信号量既用于在同一队列内排序，也用于在不同队列之间排序。

Vulkan 中有两种信号量，二进制和时间轴。本教程只使用二进制信号量，所以我们将不讨论时间轴信号量。后面提到的信号量专门指二进制信号量。

信号量要么是无信号的，要么是有信号的。它一开始是没有信号的。我们使用信号量来排序队列操作的方法是在一个队列操作中提供相同的信号量作为“信号”信号量，在另一个队列操作中提供相同的信号量作为“等待”信号量。例如，假设我们有信号量 S 和队列操作 A 和 B，我们希望按顺序执行它们。我们告诉 Vulkan 的是，操作 A 将在信号量 S 完成执行时给信号量 S “发信号”，而操作 B 将在信号量 S 开始执行前“等待”。当操作 A 结束时，信号量 S 将被发出信号，而操作 B 直到 S 被发出信号才会开始。操作 B 开始执行后，信号量 S 将自动重置为无信号状态，允许再次使用。

伪代码如下：
```psecode
VkCommandBuffer A, B = ... // record command buffers
VkSemaphore S = ... // create a semaphore

// enqueue A, signal S when done - starts executing immediately
vkQueueSubmit(work: A, signal: S, wait: None)

// enqueue B, wait on S to start
vkQueueSubmit(work: B, signal: None, wait: S)
```

在这个代码片段中，对 `vkQueueSubmit()` 的两个调用都立即返回——等待只发生在 GPU 上。CPU 继续运行，不阻塞。为了让CPU 等待，我们需要一个不同的同步原语。

#### 围栏（Fences）

围栏具有类似的目的，它用于同步执行，但它用于对 CPU（主机）上的执行进行排序。如果主机需要知道 GPU 什么时候完成了某件事，我们就使用围栏。

与信号量类似，围栏要么处于有信号状态，要么处于无信号状态。当我们提交工作要执行时，我们可以为该工作附加一个围栏。当工作完成时，围栏将发出信号。然后，我们可以让主机等待围栏发出信号，确保在主机继续之前工作已经完成。

以截屏为例。假设已经在 GPU 上完成了必要的工作。现在要将图像从 GPU 传输到主机，然后将内存保存到文件。我们有执行传输的命令缓冲区 A 和围栏 F。我们提交带有围栏 F 的命令缓冲区 A，然后立即告诉主机等待 F 发出信号。主机被阻塞，直到命令缓冲区 A完成执行。这样我们就可以安全地让主机将文件保存到磁盘，因为内存传输已经完成。

伪代码如下：
```psecode
VkCommandBuffer A = ... // record command buffer with the transfer
VkFence F = ... // create the fence

// enqueue A, start work immediately, signal F when done
vkQueueSubmit(work: A, fence: F)

vkWaitForFence(F) // blocks execution until A has finished executing

save_screenshot_to_disk() // can't run until the transfer has finished
```

与信号量的例子不同，这个例子会阻塞主机的执行。这意味着主机不做任何事情，只是等待执行完成。对于这种情况，我们必须确保在截图保存到磁盘之前完成传输。

一般来说，除非必要，最好不要阻塞主机。我们希望为 GPU 和主机提供有用的工作。等待围栏发出信号并不是什么有用的工作。因此，应该更倾向于用信号量或其他尚未涉及的同步原语来同步工作。

必须手动重置围栏，使其恢复到无信号状态。这是因为围栏用于控制主机的执行，因此主机可以决定何时重置围栏。与此相反，信号量用于在不涉及主机的情况下对 GPU 上的工作进行排序。

总之，信号量用于指定 GPU 上操作的执行顺序，而围栏用于保持 CPU 和 GPU 彼此同步。

#### 选择什么？

有两个同步原语可以用，可以方便地在两个地方应用同步：交换链操作和等待前一帧完成。我们希望使用信号量进行交换链操作，因为它们发生在 GPU 上，不让主机等待。对于等待前一帧结束，我们希望使用围栏，因为我们需要主机等待。这样就不会一次画多过一帧。因为每一帧都要重新记录命令缓冲区，所以在当前帧执行完成之前，不能将下一帧的工作记录到命令缓冲区中，因为我们不想在 GPU 使用命令缓冲区时覆盖命令缓冲区的当前内容。

### 创建同步对象

这里需要一个信号量表示已经从交换链中获取图像并准备好进行渲染，另一个信号量表示渲染已经完成，可以用于呈现，还需要一个围栏确保一次只渲染一帧。

创建三个类成员存储信号量和围栏对象：
```cpp
vk::Semaphore m_ImageAvailableSemaphore;
vk::Semaphore m_RenderFinishedSemaphore;
vk::Fence m_InFlightFence;
```

为创建信号量，添加 `createSyncObjects` 成员函数：
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
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
}
...
void HelloTriangleApplication::createSyncObjects() {}
```

创建信号量需要填充 `vk::SemaphoreCreateInfo` 结构体，但是当前 API 下不需要什么信息
```cpp
vk::SemaphoreCreateInfo semaphoreInfo{};
```

可能需要填充一些 `setFlags` 之类的参数。填充 `vk::FenceCreateInfo` 结构体并创建信号量和围栏：
```cpp
m_ImageAvailableSemaphore = m_Device.createSemaphore(semaphoreInfo);
m_RenderFinishedSemaphore = m_Device.createSemaphore(semaphoreInfo);
m_InFlightFence = m_Device.createFence(fenceInfo);
```

信号量和围栏也应该在程序结束前销毁，要求所有的命令都技术，不再需要同步。
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroySemaphore(m_ImageAvailableSemaphore);
    m_Device.destroySemaphore(m_RenderFinishedSemaphore);
    m_Device.destroyFence(m_InFlightFence);
    ...
}
```

### 等待上一帧

在一帧开始时，我们想要一直等待直到前一阵结束，此时命令缓冲区和信号量就可以被使用。首先调用设备的 `waitForFences` 函数： 
```cpp
void HelloTriangleApplication::drawFrame()
{
    m_Device.waitForFences(m_InFlightFence, true, std::numeric_limits<uint64_t>::max());
}
```

函数的第二个参数传递为 `true`，表示我们想要等待所有的围栏，但是这里只有一个，所以无所谓。此函数的第三个参数为超时参数，传递为 64 位整数的最大值，表示不限时。

等待后，需要设置围栏的状态：
```cpp
m_Device.resetFences(m_InFlightFence);
```

在继续之前，设计中有一个小问题。在第一帧中，调用 `drawFrame()`，它立即等待 `inFlightFence` 发出信号。`inFlightFence` 只在渲染完一帧后才会发出信号，但这是第一帧，所以没有前面的帧可以给 `fence` 发信号。因此，`vkWaitForFences()` 将无限期阻塞，等待永远不会发生的事情。

在许多解决方案中，API 内置了一个方案。在有信号状态下创建栅栏，这样对 `vkWaitForFences()` 的第一个调用就会立即返回，因为栅栏已经有信号了。要这么做，需要在创建围栏时设置标记：
```cpp
void HelloTriangleApplication::createSyncObjects()
{
    ...
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    ...
}
```

### 从交换链获取图像

在 `drawFrame` 函数中的下一件事是从交换链中获取图像。回想一下，交换链是一个扩展特性，所以必须使用具有 `KHR` 后缀的函数:
```cpp
const auto& value = m_Device.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphore);
uint32_t imageIndex = value.value;
```

第二个参数指定图像可用的时间限制（纳秒）。使用 64 位浮点数的最大值可以取消时间限制。接下来的的参数指定在表示引擎获取图像时发出信号的同步对象。这是开始绘制的时间点。可以指定信号量、围栏或两者都指定。我们将使用 `m_ImageAvailableSemaphore` 来实现。

返回值用于输出可用的交换链图像索引。索引引用 `swapChainImages` 数组中的 `vk::Image`。这里将使用该索引选择 `vk::FrameBuffer`。

### 记录命令缓冲区

使用 `imageIndex` 指定要使用的交换链图像，现在记录命令缓冲区。首先在命令缓冲区上调用 `vkResetCommandBuffer` 记录命令缓冲区。
```cpp
m_CommandBuffer.reset(vk::CommandBufferResetFlags{0});
```

参数为标志位 `vk::CommandBufferResetFlags`，设置为 0。现在调用函数记录命令：
```cpp
recordCommandBuffer(m_CommandBuffer, imageIndex);
```

记录命令后提交命令。

### 提交命令缓冲区

队列提交和同步通过结构体 `vk::SubmitInfo` 的参数设置：

```cpp
vk::SubmitInfo submitInfo{};
vk::Semaphore waitSemaphores[] = { m_ImageAvailableSemaphore };
vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
submitInfo.setWaitSemaphores(waitSemaphores)
    .setWaitDstStageMask(waitStages);
```

`setWaitSemaphores` 指定在执行开始前等待的信号量，`setWaitDstStageMask` 决定管线的哪个阶段等待。我们希望等待将颜色写入图像，直到图像可用为止，因此指定了写入颜色附件的图形管线阶段。这意味着理论上实现已经可以开始执行顶点着色器，而图像还没有可用。`waitStages` 数组中的每一项都对应于 `setWaitSemaphores` 中具有相同索引的信号量。

`setCommandBuffers` 指定实际提交执行的命令缓冲区：
```cpp
submitInfo.setCommandBuffers(m_CommandBuffer);
```
这里就一个命令缓冲区。

```cpp
vk::Semaphore signalSemaphores[] = { m_RenderFinishedSemaphore };
submitInfo.setSignalSemaphores(signalSemaphores);
```

`signalsemaphores` 和 `setSignalSemaphores` 指定在命令缓冲区完成执行后发出哪些信号。本例子中使用 `m_RenderFinishedSemaphore` 实现此目的。
```cpp
m_GraphicsQueue.submit(submitInfo, m_InFlightFence);
```

现在可以使用 `submit` 将命令缓冲区提交到图形队列。当工作负载非常大时，该函数以 `vk::SubmitInfo` 结构数组作为参数。最后一个参数引用了一个可选的围栏，当命令缓冲区完成执行时发出信号。这让我们知道什么时候重用命令缓冲区是安全的，因此给它 `m_InFlightFence`。在下一帧，CPU 将等待命令缓冲区完成执行，然后再记录新命令。

### 子通道依赖

渲染通道中的子通道会自动处理图像布局传输。这些传输操作由**子通道依赖**项控制，子通道依赖项指定子通道之间的内存和执行依赖项。现在只有一个子通道，但是在这个子通道之前和之后的传输也算隐含的“子通道”。

有两个内置依赖项负责呈现通道开始和结束时的传输，但前者没有在正确的时间发生。它假设传输发生在管线的开始，但是还没有获得在那一点的图像。有两种方法处理问题。我们可以改变`m_ImageAvailableSemaphore` 的 `waitStages` 为
`vk::PipelineStageFlagBits::eTopOfPipe` 来确保渲染通道在图像可用之前不会开始，或者可以让渲染通道等待 `vk::PipelineStageFlagBits::eColorAttachmentOutput` 阶段。在这里使用第二个选项，因为这是一个了解子通道依赖关系及其工作方式的好方法。

子通道依赖通过 `vk::SubpassDependency` 结构体，在 `createRenderPass` 函数中添加：
```cpp
vk::SubpassDependency dependency{};
dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
    .setDstSubpass(0);
```

上段代码指定依赖项的依赖子通道的索引。宏`VK_SUBPASS_EXTERNAL` 指的是在呈现传递之前或之后的隐式子传递，取决于它是在 `setSrcSubpass` 还是 `setDstSubpass` 中指定的。索引 0 指的是我们的子通道，它是第一个也是唯一一个。`setDstSubpass` 的值必须高于`setSrcSubpass`，以防止依赖关系图中的循环（除非其中一个子通道是 `VK_SUBPASS_EXTERNAL`）。

```cpp
dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
    .setSrcAccessMask(vk::AccessFlags{0});
```

接下来指定等待的操作以及操作发生的阶段。我们需要等待交换链完成从图像的读取，然后才能访问。这通过等待颜色附件输出阶段来完成。

```cpp
dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
    .setDstAccessMask(vk::AccessFlags{0});
```

应该等待此操作的操作位于颜色附件阶段，并涉及到颜色附件的写入。这些设置将阻止传输，直到它实际上是必要的（和允许的）：当要写入颜色时。

```cpp
renderPassInfo.setDependencies(dependency);
```

最后把 `dependency` 填充到结构体 `renderPassInfo`。

### 呈现

绘制的最后一步是将结果提交回交换链，使其显示在屏幕上。表示是通过 `drawFrame` 函数末尾的 `vk::PresentInfoKHR` 结构配置的。

```cpp
vk::PresentInfoKHR presentInfo{};
presentInfo.setWaitSemaphores(signalSemaphores);
```

参数指定在呈现之前等待哪个信号量，类似 `vk::SubmitInfo`，因为想要等待命令缓冲区完成执行，三角形就被绘制出来了，所以我们取将要发出信号的信号量并等待它们，因此我们使用 `setWaitSemaphores`。

```cpp
vk::SwapchainKHR swapChains[] = { m_SwapChain };
presentInfo.setSwapchains(swapChains)
    .setImageIndices(imageIndex);
```

上述代码指定要向其显示图像的交换链和每个交换链的图像索引。

```cpp
presentInfo.setResults({});    // Optional
```

可选参数 `setResults`，指定 `vk::Result` 值的数组，以检查每个单独的交换链，如果呈现为成功。如果您只使用单个交换链，则没有必要这样做，因为您可以简单地使用 `presentKHR` 函数的返回值。
> 注意！这么写会导致出错，注释掉就没事儿了：`pPresentInfo->swapchainCount must be greater than 0`

```cpp
m_PresentQueue.presentKHR(presentInfo);
```

`presentKHR` 提交向交换链提供图像的请求。下一章中为 `vkAcquireNextImageKHR` 和 `vkQueuePresentKHR` 添加错误处理，因为它们失败并不意味着程序终止。

运行应该能看到三角形了。但是关闭的时候会出错：
```shell
validation layerValidation Error: [ VUID-vkDestroySemaphore-semaphore-01137 ] Object 0: handle = 0x5555562c8df0, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0xa1569838 | Cannot call vkDestroySemaphore on VkSemaphore 0xd5b26f0000000010[] that is currently in use by a command buffer. The Vulkan spec states: All submitted batches that refer to semaphore must have completed execution (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkDestroySemaphore-semaphore-01137)
validation layerValidation Error: [ VUID-vkDestroySemaphore-semaphore-01137 ] Object 0: handle = 0x5555562c8df0, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0xa1569838 | Cannot call vkDestroySemaphore on VkSemaphore 0x980f360000000011[] that is currently in use by a command buffer. The Vulkan spec states: All submitted batches that refer to semaphore must have completed execution (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkDestroySemaphore-semaphore-01137)
validation layerValidation Error: [ VUID-vkDestroyFence-fence-01120 ] Object 0: handle = 0xdcc8fd0000000012, type = VK_OBJECT_TYPE_FENCE; | MessageID = 0x5d296248 | VkFence 0xdcc8fd0000000012[] is in use. The Vulkan spec states: All queue submission commands that refer to fence must have completed execution (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkDestroyFence-fence-01120)
validation layerValidation Error: [ VUID-vkDestroyCommandPool-commandPool-00041 ] Object 0: handle = 0x555556905c70, type = VK_OBJECT_TYPE_COMMAND_BUFFER; | MessageID = 0xad474cda | Attempt to destroy command pool with VkCommandBuffer 0x555556905c70[] which is in use. The Vulkan spec states: All VkCommandBuffer objects allocated from commandPool must not be in the pending state (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkDestroyCommandPool-commandPool-00041)
```

`drawFrame` 中的所有操作都是异步的。意味着在 `mainLoop` 中退出循环时，绘图和表示操作可能仍在进行。在这种情况下不应该清理资源。解决方案就是在退出 `mainLoop` 并销毁窗口之前等待逻辑设备完成操作：
```cpp
void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
        drawFrame();
    }

    m_Device.waitIdle();
}
```

还可以使用 `waitIdle` 等待特定命令队列中的操作完成。这些函数可以作为执行同步的基本方式使用。现在关闭窗口后可以正常退出。

### 总结

900 多行代码之后终于看到屏幕上的三角形了。创建 Vulkan 程序需要大量的工作，但关键信息是，Vulkan 显式地提供了大量的控制。建议重新阅读代码，并建立一个关于程序中所有 Vulkan 对象的用途以相互关联的模型。从现在开始，我们将在这些知识的基础上扩展程序的功能。