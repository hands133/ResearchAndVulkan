# Vulkan Tutorial (C++ Ver.)

## 命令缓冲区（command buffers）

Vulkan 中如绘图操作和内存传输等命令，不是直接调用函数执行的。必须在命令缓冲区中记录要执行的操作。这样做的好处是，当我们告诉 Vulkan 想做什么时，所有的命令都被一起提交，Vulkan可以更有效地处理这些命令，因为所有这些命令都是一起使用的。此外，如果需要，Vulkan 允许用多线程记录命令。

### 命令池（command pools）

创建命令缓冲区之前必须创建命令池。命令池管理存储缓冲区的内存，并从其中分配命令命令缓冲区。添加一个类成员存储命令池：
```cpp
vk::CommandPool m_CommandPool;
```
并添加函数 `createCommandPool`，在 `initVulkan` 函数创建帧缓冲之后调用：
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
}

bool HelloTriangleApplication::createCommandPool() {}
```

创建命令池：
```cpp
vk::CommandPoolCreateInfo poolInfo{};
poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value())
```

`setFlags` 的参数有两种选项（其实有三种）：
+ `vk::CommandPoolCreateFlagBits::eTransient`：命令缓冲区经常填充新命令（有许多更改内存分配的行为）
+ `vk::CommandPoolCreateFlagBits::eResetCommandBuffer`：允许命令缓冲读单独记录，如果没有此标志，它们必须一起重置

这里每一帧记录一个命令缓冲区，所以希望能重置和重新记录它。因此需要为命令池设置 `vk::CommandPoolCreateFlagBits::eResetCommandBuffer` 标志位。

命令缓冲区通过在一个设备队列上提交它们来执行，就像检索的图形和表示队列一样。每个命令池只能分配在单一类型队列上提交的命令缓冲区。这里需要记录用于绘图的命令，这就是选择图形队列族的原因。

```cpp
m_CommandPool = m_Device.createCommandPool(poolInfo);
if (!m_CommandPool) throw std::runtime_error("failed to create command pool!");
```

使用 `createCommandPool` 创建命令池。命令将在整个程序中使用，以便在屏幕上绘制东西，所以命令池应该只在结束时销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroyCommandPool(m_CommandPool);
    ...
}
```

### 命令缓冲区分配

现在创建命令缓冲区，添加 `vk::CommandBuffer` 类成员：
```cpp
vk::CommandBuffer m_CommandBuffer;
```

并添加函数 `createCommandBuffer` 函数，从命令池分配单一命令缓冲区：
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
}

void HelloTriangleApplication::createCommandBuffer() {}
```

命令缓冲区通过 `allocateCommandBuffers` 函数分配，该函数参数为 `vk::CommandBufferAllocateInfo` 结构体，指定命令池和要分配的缓冲区数量：
```cpp
vk::CommandBufferAllocateInfo allocInfo{};
allocInfo.setCommandPool(m_CommandPool)
    .setLevel(vk::CommandBufferLevel::ePrimary)
    .setCommandBufferCount(1);
auto commandBuffer = m_Device.allocateCommandBuffers(allocInfo);
m_CommandBuffer = commandBuffer.front();
```

`setLevel` 参数指定了分配的命令缓冲区是基本的或者是二级命令缓冲区：
+ `vk::CommandBufferLevel::ePrimary`：可以提交到队列执行，但不能从其他命令缓冲区调用
+ `vk::CommandBufferLevel::eSecondary`：不能直接提交，但可以从主命令缓冲区调用

这里不使用辅助命令缓冲区功能，但是可以想到，重用来自主命令缓冲区的常见操作是很有帮助的。由于只分配一个命令缓冲区，`setCommandBufferCount` 的值为 1。

### 命令缓冲区记录

现在开始处理 `recordCommandBuffer` 函数，该函数将想要执行的命令写入命令缓冲区。用到的 `vk::CommandBuffer` 将作为参数传入，以及我们想要写入的当前交换链的索引。

```cpp
void HelloTriangleApplication::recordCommandBuffer(vk::CommandBuffer, uint32_t imageIndex) {}
```

总是通过调用 vkBeginCommandBuffer 开始记录命令缓冲区，并使用一个小的 VkCommandBufferBeginInfo 结构作为参数，该结构指定了关于特定命令缓冲区使用的一些细节：
```cpp
vk::CommandBufferBeginInfo beginInfo{};
beginInfo.setFlags(vk::CommandBufferUsageFlags{0})  // Optional
    .setPInheritanceInfo(nullptr);  // Optional

    m_CommandBuffer.begin(beginInfo);
```

`setFlags` 参数指定了我们如何使用命令缓冲区，可选的值有：
+ `vk::CommandBufferUsageFlagBits::eOneTimeSubmit`：命令缓冲区将执行之后立即记录
+ `vk::CommandBufferUsageFlagBits::eRenderPassContinue`：这是一个次要的命令缓冲区，将完全在一个单一的渲染通道中
+ `vk::CommandBufferUsageFlagBits::eSimultaneousUse`：命令缓冲区可以在已经挂起执行时重新提交

但是所有这些标志都不适合。`setPInheritanceInfo` 参数只与次要命令缓冲区相关。它指定从调用主命令缓冲区继承哪个状态。如果命令缓冲区已经记录了一次，那么调用 `begin` 将隐式重置它，不可能在以后将命令追加到缓冲区。

### 启动一个渲染通道

绘制开始于使用 `vk::RenderPassBeginInfo` 启动渲染通道。渲染通道使用 `vk::RenderPassBeginInfo` 结构的参数进行配置。

```cpp
vk::RenderPassBeginInfo renderPassInfo{};
renderPassInfo.setRenderPass(m_RenderPass)
    .setFramebuffer(m_vecSwapchainFramebuffers[imageIndex]);
```

我们为每个交换链图像创建了帧缓冲，交换链图像也具有特定的颜色附件。因此需要为我们想要绘制的交换链图像绑定帧缓冲。使用函数传入的 `imageIndex` 参数，可以选择当前交换链图像的正确帧缓冲。

```cpp
renderPassInfo.setRenderArea(vk::Rect2D({0, 0}, m_SwapChainExtent));
```

渲染区域定义着色器加载和存储的位置。区域外的像素将有未定义值。它应该与性能最好的附件的大小相同。

```cpp
vk::ClearValue clearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
renderPassInfo.setClearValues(clearColor);
```

最后两个参数定义了用于 `vk::AttachmentLoadOp::eClear` 的颜色清除值，用于颜色附件的加载操作。这里的清零颜色为 100% 不透明度的黑色。

```cpp
m_CommandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
```

渲染通道现在开始！所有记录命令的函数都可以通过 `m_CommandBuffer` 的函数设置。它们都返回 `void`，所以没有错误处理。

第一个参数为指定的渲染通道细节，第二个参数控制渲染通道内部的命令如何提供，参数包括两个：
+ `vk::SubpassContents::eInline`：渲染通道命令会被嵌入到基础命令缓冲区，次级命令不会执行
+ `vk::SubpassContents::eSecondaryCommandBuffers`：渲染通道命令会从次级命令缓冲区执行

因为这里不需要次级缓冲区，所以使用第一个选项。


### 基本绘制命令

绑定图形管线：
```cpp
m_CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline);
```

第一个参数指定了管线是图形的还是计算的。现在告诉 Vulkan 在图形管线中的操作，以及在片元着色器中使用哪个图形附件，剩下的就是绘制三角形了：
```cpp
m_CommandBuffer.draw(3, 1, 0, 0);
```

具体的 `draw` 函数参数很简单，所有的信息已经提前给出了。对应四个参数的作用分别为：
+ `vertexCount`：尽管不需要顶点缓冲区，这里还是需要绘制三个顶点
+ `instanceCount`：用于渲染实例，如果不用的话就设置为 1
+ `firstVertex`：用于顶点着色器的偏移，定义 `gl_Vertex` 的最小值
+ `firstInstance`：用于渲染实例的偏移，定义了 `gl_InstanceIndex` 的最小值

### 守卫

渲染通道可以结束了，命令缓冲区也随后结束：
```cpp
m_CommandBuffer.endRenderPass();
m_CommandBuffer.end();
```

下一章修改渲染循环函数。