# Vulkan Tutorial (C++ Ver.)

## 索引缓冲区（Index buffer）

### 介绍

实际程序中渲染的 3D 网格通常在多个三角形之间共享顶点。绘制一个矩形需要两个三角形，需要一个有 6 个顶点的顶点缓冲区。问题是两个顶点的数据需要复制，造成 50% 的冗余。对于更复杂的网格，情况只会变得更糟，顶点在平均 3 个三角形中重复使用。解决方案是使用索引缓冲区（index buffer）。索引缓冲区本质上是一个指向顶点缓冲区的指针数组。允许重新排序顶点数据，并为多个顶点重用现有数据。

### 创建索引缓冲区

本章修改顶点数据并添加索引数据绘制矩形：
```cpp
const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } }
};
```

左上方顶点为红色，右上方为绿色，右下方为蓝色，左下方为白色。添加 `indices` 数据表示索引，应该匹配顶点索引来绘制两个半三角形：
```cpp
const std::vector<uint16_t> indices = {
    0, 1, 2,
    2, 3, 0
};
```

根据顶点中条目的数量，可以使用 `uint16_t` 或 `uint32_t` 作为索引类型。现在使用 `uint16_t`，表示使用的顶点数小于 65536（原文是 65535，不确定是否有特殊值）。

类似顶点数据，索引需要上传到 `vk::Buffer` 中，以便 GPU 访问。定义两个新的类成员来保存索引缓冲区的资源：
```cpp
vk::Buffer m_VertexBuffer;
vk::DeviceMemory m_VertexBufferMemory;
vk::Buffer m_IndexBuffer;
vk::DeviceMemory m_IndexBufferMemory;
```

添加 `createIndexBuffer` 函数：
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createVertexBuffer();
    createIndexBuffer();
    ...
}


void HelloTriangleApplication::createIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(indices.front()) * indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, 
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);
    
    void* data;
    const auto&_ = m_Device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags{0}, &data);
    memcpy(data, indices.data(), bufferSize);
    m_Device.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize, 
        vk::BufferUsageFlagBits::eTransferDst |
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_IndexBuffer, m_IndexBufferMemory);
    
    copyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    m_Device.destroyBuffer(stagingBuffer);
    m_Device.freeMemory(stagingBufferMemory);
}
```

有一些与之前不同的地方。`bufferSize` 现在等于索引数乘索引类型字节数。`indexBuffer` 的 `usage` 标志应该设为 `vk::BufferUsageFlagBits::eTransferDst` 和 `vk::BufferUsageFlagBits::eIndexBuffer`。其他的处理过程完全相同。这里同样创建了暂存缓冲区拷贝 `indices` 的内容到设备的索引缓冲。

索引缓冲应该在程序结束时清理：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyBuffer(m_IndexBuffer);
    m_Device.freeMemory(m_VertexBufferMemory);

    m_Device.destroyBuffer(m_VertexBuffer);
    m_Device.freeMemory(m_VertexBufferMemory);
    ...
}
```

### 使用索引缓冲

使用索引缓冲区进行绘制涉及到 `createCommandBuffers` 的两个更改。我们首先需要绑定索引缓冲区，顶点缓冲区。不同之处在于您只能有一个索引缓冲区。注意不能为每个顶点属性使用不同的索引，即使只有一个属性不同，仍然需要完全复制顶点数据。

修改 `recordCommandBuffer`：
```cpp
commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offset);
commandBuffer.bindIndexBuffer(m_IndexBuffer, 0, vk::IndexType::eUint16);
```

索引缓冲区通过 `bindIndexBuffer` 绑定，`bindIndexBuffer` 参数为索引缓冲区、其中的字节偏移量以及作为参数的索引数据类型。可能的类型是 `vk::IndexType::eUint16` 和 `vk::IndexType::eUint32`。

仅仅绑定一个索引缓冲区并没有改变任何东西，还需要改变绘图命令使用索引缓冲区。删除 `commandBuffer.draw` 行，并替换为`drawIndexed`：
```cpp
commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
```

这个函数类似于 `draw`。前两个参数指定索引的数量和实例的数量。我们不使用实例，所以只指定 1 个实例。索引的数量表示将被传递到顶点缓冲区的顶点的数量。下一个参数指定索引缓冲区的偏移量，1 将导致显卡从第二个索引开始读取。倒数第二个参数指定要添加到索引缓冲区中的索引的偏移量。最后一个参数指定实例的偏移量，我们没有使用它。