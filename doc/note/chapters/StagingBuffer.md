# Vulkan Tutorial (C++ Ver.)

## 暂存缓冲区（Staging buffer）

### 介绍

从 CPU 访问顶点缓冲区的内存类型可能不是显卡本身读取的最优内存类型。最理想的内存具有 `vk::MemoryPropertyFlagBits::eDeviceLocal` 标志，并且在专用的显卡上通常不能被 CPU 访问。本章将创建两个顶点缓冲区。在 CPU 可访问内存中有一个暂存缓冲区（staging buffer），用于上传顶点数组中的数据，在设备本地内存中有一个最终顶点缓冲区。然后使用缓冲区复制命令将数据从暂存缓冲区移动到顶点缓冲区。

### 传输队列（Transfer queue）

缓冲区复制命令需要一个支持传输操作的队列族，该队列族使用`vk::QueueFlagBits::eTransfer` 表示。好消息是
任何具有 `vk::QueueFlagBits::eGraphics` 或 `vk::QueueFlagBits::eCompute` 能力的队列家族已经隐式支持 `vk::QueueFlagBits::eTransfer` 操作。在这些情况下，实现不需要显式地在 `queueFlags` 中列出它。

您喜欢挑战，那么您仍然可以尝试使用专门用于传输操作的不同队列族。它将要求您对您的程序进行以下修改：
+ 修改 `QueueFamilyIndices` 和 `findQueueFamilies` 以显式地查找具有 `vk::QueueFlagBits::eTransfer` 位的队列族，而不是 `vk::QueueFlagBits::eGraphics` 位的队列族
+ 修改 `createLogicalDevice` 请求传输队列的句柄
+ 为在传输队列族上提交的命令缓冲区创建第二个命令池
+ 修改资源的共享模式为 `vk::SharingMode::eConcurrent`
并指定图形队列族和传输队列族
+ 提交任何传输命令，如 `CopyBuffer`（在本章中使用）到传输队列，而不是图形队列

这是一些工作，但它将教会很多关于如何在队列族之间共享资源的知识。

### 抽象创建缓冲区过程

因为将在本章中创建多个缓冲区，所以将缓冲区创建移动到辅助函数。创建成员函数 `createBuffer`，并将`createVertexBuffer` 中的代码（映射除外）移动到函数内：
```cpp
void HelloTriangleApplication::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(size)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);

    buffer = m_Device.createBuffer(bufferInfo);
    if (!buffer) throw std::runtime_error("failed to create buffer!");

    vk::MemoryRequirements memRequirements = m_Device.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));

    bufferMemory = m_Device.allocateMemory(allocInfo);
    if (!bufferMemory)  throw std::runtime_error("failed to allocate buffer memory!");

    m_Device.bindBufferMemory(buffer, bufferMemory, 0);
}
```

函数确保添加了缓冲区大小、内存属性和使用的参数，这样就可以使用函数创建许多不同类型的缓冲区。最后两个参数是要写入句柄的输出变量。现在可以从 `createVertexBuffer` 中删除缓冲区创建和内存分配代码，只调用 `createBuffer`：
```cpp
void HelloTriangleApplication::createVertexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
        m_VertexBuffer, m_VertexBufferMemory);
    
    void* data;
    const auto&_ = m_Device.mapMemory(m_VertexBufferMemory, 0, bufferSize, vk::MemoryMapFlags{0}, &data);
    memcpy(data, vertices.data(), bufferSize);
    m_Device.unmapMemory(m_VertexBufferMemory);
}
```

运行程序保证顺利执行。

### 使用暂存缓冲区

现在要修改 `createVertexBuffer`，只使用一个主机可见缓冲区作为临时缓冲区，使用一个设备本地缓冲区作为实际的顶点缓冲区。

我们现在使用一个新的 `stagingBuffer` 和 `stagingBufferMemory` 映射和复制顶点数据。在本章中，我们将使用两个新的缓冲区使用标志：
+ `vk::BufferUsageFlagBits::eTransferSrc`：缓冲区用作内存传输操作的源头
+ `vk::BufferUsageFlagBits::eTransferDst`：缓冲区用于内存传输操作的目的

`m_VertexBuffer` 现在从设备本地的内存类型中分配，通常意味着不能使用 `mapMemory`。但是，可以将数据从`stagingBuffer` 复制到 `m_VertexBuffer`。我们必须通过为 `stagingBuffer` 指定传输源标志和为 `m_VertexBuffer`指定传输目的地标志，以及顶点缓冲区使用标志来表示计划这样做。

现在添加成员函数 `copyBuffer` 将数据从一个缓冲区复制到另一个缓冲区：
```cpp
void HelloTriangleApplication::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {}
```

内存传输操作使用命令缓冲区执行。因此须分配一个临时命令缓冲区。可以为这些临时缓冲区创建单独的命令池，因为可以应用内存分配优化。应该使用`vk::CommandPoolCreateFlagBits::eTransient`
在这种情况下，在生成命令池时标记：
```cpp
void HelloTriangleApplication::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_CommandPool)
        .setCommandBufferCount(1);

    vk::CommandBuffer commandBuffer = m_Device.allocateCommandBuffers(allocInfo).front();
}
```

并立即开始记录命令缓冲区：
```cpp
vk::CommandBufferBeginInfo beginInfo{};
beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

commandBuffer.begin(beginInfo);
```

这里只使用命令缓冲区一次，并等待从函数返回，直到复制操作执行完毕。使用 `vk::CommandBufferUsageFlagBits::eOneTimeSubmit` 告诉驱动程序我们的意图是一个很好的做法。

```cpp
vk::BufferCopy copyRegin{};
copyRegin.setSrcOffset(0)
    .setDstOffset(0)
    .setSize(size);

commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegin);
```

使用 `copyBuffer` 命令传输缓冲区的内容。它以源缓冲区和目标缓冲区作为参数，并以一个要复制的区域数组作为参数。区域定义在 `vk::BufferCopy` 结构体中，由源缓冲区偏移量、目标缓冲区偏移量和大小组成。这里不可能指定 `VK_WHOLE_SIZE`，这与 `mapMemory` 命令不同。

```cpp
commandBuffer.end();
```

此命令缓冲区只包含复制命令，所以可以在命令后立即结束。然后提交缓冲区：
```cpp
vk::SubmitInfo submitInfo{};
submitInfo.setCommandBuffers(commandBuffer);

m_GraphicsQueue.submit(submitInfo);
m_GraphicsQueue.waitIdle();
```

与绘制命令不同，这次不需要等待任何事件。我们只想立即在缓冲区上执行传输。仍有两种方法等待传输。可以使用 `m_Fence` 并使用 `waitForFences` 等待，或者简单地使用 `waitIdle` 等待传输队列空闲。`m_Fence` 允许同时安排多个传输，并等待所有传输完成，而不是一次执行一个传输。这可能会给驱动更多的优化机会。

记得清除命令缓冲区：
```cpp
m_Device.freeCommandBuffers(m_CommandPool, commandBuffer);
```

在 `createVertexBuffer` 函数内调用 `copyBuffer` 将顶点数据移动到设备缓冲区：
```cpp
createBuffer(bufferSize,
    vk::BufferUsageFlagBits::eTransferDst |
    vk::BufferUsageFlagBits::eVertexBuffer,
    vk::MemoryPropertyFlagBits::eDeviceLocal,
    m_VertexBuffer, m_VertexBufferMemory);

copyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);
```

复制数据之后进行清理：
```cpp
copyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

m_Device.destroyBuffer(stagingBuffer);
m_Device.freeMemory(stagingBufferMemory);
```

这种改进现在可能还不明显，但它的顶点数据现在正在从高性能内存中加载。当我们开始渲染更复杂的几何图形时，这将很重要。

### 总结

实际应用程序中，不应该为每个单独的缓冲区调用 `allocateMemory`。同时内存分配的最大数量受`maxMemoryAllocationCount` 物理设备限制，即使在NVIDIA GTX 1080 这样的高端硬件上，也可能低至 4096。同时为大量对象分配内存的正确方法是创建一个自定义分配器，通过使用我们在许多函数中看到的 `setOffset` 参数，将单个分配分配到许多不同的对象。

您可以自己实现这样一个分配器，也可以使用 GPUOpen 倡议提供的 `VulkanMemoryAllocator` 库。但是，在本教程中，可以为每个资源使用单独的分配，因为我们目前还没有达到这些限制。