# Vulkan Tutorial (C++ Ver.)

## 创建顶点缓冲区（Vertex buffer creation）

### 介绍

Vulkan 中的缓冲区用于存储显卡可以读取的任意数据，是显卡的内存区域，也可以用于其他目的。与目前处理的 Vulkan 对象不同，缓冲区不会自动分配内存。前面章节已经表明，Vulkan API 使程序员控制几乎任何事情，也包括内存管理。

### 创建缓冲区

添加类成员函数 `createVertexBuffer` 并在 `initVulkan` 函数中的 `createCommandBuffers` 之前调用：
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
    createVertexBuffer();
    createCommandBuffers();
    createSyncObjects();
}

void HelloTriangleApplication::createVertexBuffer() {}
```

创建缓冲区需要填充 `vk::BufferCreateInfo` 结构体：
```cpp
vk::BufferCreateInfo bufferInfo{};
bufferInfo.setSize(sizeof(Vertex) * vertices.size());
```

`setSize` 以字节为单位指定缓冲区的大小。使用 `sizeof` 直接计算顶点数据的字节大小。

```cpp
bufferInfo.setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
```

`setUsage` 指定缓冲区中的数据的目的。可以使用按位或操作指定多个用途。这里是顶点缓冲区，后续将看到其他的使用方式。

类似交换链图像，缓冲区也可以由特定的队列家族所有权，或者在多个队列之间共享。缓冲区只从图形队列中使用，因此这里设置为独占访问。

此外还有 `setFlags` 函数，用于指定稀疏缓冲区内存，现在还涉及不到，默认为 0。

通过 `createBuffer` 创建缓冲区，并存储到类成员中：
```cpp
vk::Buffer m_VertexBuffer;

void HelloTriangleApplication::createVertexBuffer()
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(sizeof(Vertex) * vertices.size())
        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);

    m_VertexBuffer = m_Device.createBuffer(bufferInfo);
    if(!m_VertexBuffer) throw std::runtime_error("failed to create vertex buffer!");
}
```

缓冲区应该在程序推出之前一直可用，不依赖于交换链，只需要在 `cleanUp` 函数中销毁即可：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyBuffer(m_VertexBuffer);
    ...
}
```

### 存储需求（Memory requirements）

缓冲区已经创建，还没有分配内存。分配内存的第一步是使用 `vkGetBufferMemoryRequirements` 函数查询内存需求：
```cpp
vk::MemoryRequirements memRequirements = m_Device.getBufferMemoryRequirements(m_VertexBuffer);
```

`vk::MemoryRequirements` 包含三个成员：
+ `size`：需求内存的字节总数大小，可能与 `bufferInfo.size` 不同
+ `alignment`：缓冲区在分配的内存中的起始偏移（以字节为单位）取决于 `bufferInfo.usage` 和 `bufferInfo.flags`
+ `memoryTypeBits`：与缓冲区匹配的内存类型位域

显卡可以提供不同类型的内存进行分配。每种类型的内存的操作和性能各不相同。需要结合缓冲区的需求和自己的应用程序需求找到要使用的正确类型的内存。创建一个新函数 `findMemoryType`：
```cpp
uint32_t HelloTriangleApplication::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags propertyFlags) {}
```

首先需要使用 `vk::PhysicalDeviceMemoryProperties` 查询可用的内存类型
```cpp
vk::PhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice.getMemoryProperties();
```

`vk::PhysicalDeviceMemoryProperties` 结构有两个数组`memoryTypes` 和 `memoryHeaps`。内存堆是不同的内存资源，比如专用的 `VRAM` 和 `VRAM` 用完时 `RAM` 中的交换内存。不同类型的内存存在于这些堆中。现在只关心内存类型，不关心它来自的堆，但是可见的会影响性能。找到最适缓冲区的内存类型：
```cpp
vk::PhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice.getMemoryProperties();
for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    if (typeFilter & (1 << i)) { return i; }

throw std::runtime_error("failed to find suitable memory type!");
```

`typeFilter` 参数用于指定合适的内存类型的位域。通过简单迭代得到合适的内存类型的下标，并检查对应位是否为 1。

然而，我们不仅仅对适合顶点缓冲区的内存类型感兴趣，还需要能够将顶点数据写入内存。`memoryTypes` 数组由 `vk::MemoryType` 结构体组成，该结构体指定每种类型内存的堆和属性。属性定义了内存特性，比如能够映射内存，可以从 CPU 写入内存。这个属性是用`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` 表示的，但还需要使用 `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` 属性。在映射内存时会讲到原因。修改循环检查属性的支持：
```cpp
for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    if (typeFilter & (1 << i) &&
        (memProperties.memoryTypes[i].propertyFlags & propertyFlags))
        { return i; }
```

可能有多个想要的属性，所以不是检查逐位与是否非零，而是等于想要的属性位。如果有适合于缓冲区的内存类型，它也具有需要的所有属性，则返回索引，否则抛出异常。

### 内存分配

现在确定正确的内存类型，填充 `vk::MemoryAllocateInfo` 分配内存：
```cpp
vk::MemoryAllocateInfo allocInfo{};
allocInfo.setAllocationSize(memRequirements.size)
    .setMemoryTypeIndex(
        findMemoryType(memRequirements.memoryTypeBits, 
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent));
```

创建类成员存储句柄，并使用 `m_Device.allocateMemory` 分配内存：
```cpp
vk::DeviceMemory m_VertexBufferMemory;
...
m_VertexBufferMemory = m_Device.allocateMemory(allocInfo);
if (!m_VertexBufferMemory)  throw std::runtime_error("failed to allocate vertex buffer memory!");
```

如果内存分配成功，那么现在可以使用 `m_Device.bindBufferMemory` 将内存与缓冲区关联：
```cpp
m_Device.bindBufferMemory(m_VertexBuffer, m_VertexBufferMemory, 0);
```

第三个参数是内存区域的偏移量。因为 `m_VertexBufferMemory` 是专为顶点缓冲区 `m_VertexBuffer` 分配的，偏移量就是 0。如果偏移量非零，那么它必须能被 `memRequirements.alignment` 整除。

类似 C++ 中的动态内存分配，内存应该在某些时候被释放。一旦缓冲区不再使用，绑定到缓冲区对象的内存可能会被释放，在缓冲区被销毁后释放：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyBuffer(m_VertexBuffer);
    m_Device.freeMemory(m_VertexBufferMemory);
    ...
} 
```

### 填充顶点缓冲区

现在将顶点数据拷贝到缓冲区中。需要使用 `mapMemory` 将缓冲区内存映射到 CPU 可访问的存储：
```cpp
void* data;
m_Device.mapMemory(m_VertexBufferMemory, 0, bufferInfo.size, vk::MemoryMapFlags{0}, &data);
```

`mapMemory` 允许访问由偏移量和大小定义的指定内存资源的区域。偏移量和大小分别是 `0` 和 `bufferInfo`。也可以指定特殊值 `VK_WHOLE_SIZE` 来映射所有内存。倒数第二个参数用于指定标志，但在当前 `API` 中还没有可用的标志。必须设置为 0。最后一个参数指定指向映射内存的指针的输出。

```cpp
void* data;
m_Device.mapMemory(m_VertexBufferMemory, 0, bufferInfo.size, vk::MemoryMapFlags{0}, &data);
memcpy(data, vertices.data(), bufferInfo.size);
m_Device.unmapMemory(m_VertexBufferMemory);
```

简单地将顶点数据 `memcpy` 到映射的内存中，然后使用 `m_Device.unmapMemory` 取消映射。不幸的是，驱动程序可能不会立即将数据复制到缓冲区内存中，例如由于缓存。也有可能对缓冲区的写入在映射内存中还不可见。有两种方法可以解决这个问题：
+ 使用主机一致的内存堆，用 `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` 表示
+ 在写入映射内存之后调用 `m_Device.flushMappedMemoryRanges`，并在从映射内存读取前调用 `m_Device.invalidateMappedMemoryRanges`

这里采用第一种方法，它确保映射的内存始终与分配的内存的内容匹配。这可能会导致性能比显式刷新稍差，下一章会解释这无关紧要。刷新内存范围或使用连贯内存堆意味着驱动会意识到我们对缓冲区的写入，不意味着它们在 GPU 上是可见的。数据到 GPU 的传输是发生在后台的操作，标准告诉我们，它保证在下一次调用`vk::Queue::submit` 时完成。

### 绑定顶点缓冲区

接下来只需要在渲染操作期间绑定顶点缓冲区。扩展 `recordCommandBuffer` 函数：
```cpp
commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline);

vk::Buffer vertexBuffers[] = { m_VertexBuffer };
vk::DeviceSize offset[] = { 0 };
commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offset);

commandBuffer.draw(vertices.size(), 1, 0, 0);
```

`bindVertexBuffers` 函数用于将顶点缓冲区绑定到 bindings。前两个参数指定了顶点缓冲区的偏移量和绑定的数量。最后两个参数指定要绑定的顶点缓冲区数组和开始读取顶点数据的字节偏移量。此外修改 `draw` 的调用，传递缓冲区的顶点数量。

修改 `vertices` 中的颜色等可以看到与之对应的修改，无需重新编译着色器。