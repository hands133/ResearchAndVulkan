# Vulkan Tutorial (C++ Ver.)

## 描述符池和描述符集合（Descriptor pool and sets）

### 介绍

前一章中的描述符布局描述了可以绑定的描述符类型。在本章中，我们将为每个 `vk::Buffer` 资源创建一个描述符集，将其绑定到统一的缓冲区描述符。

### 描述符池（Descriptor pool）

描述符集不能直接创建，它们必须像命令缓冲区一样从池中分配。描述符集合的对等物被称为描述符池。我们将编写一个新函数`createDescriptorPool` 来设置它。
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createUniformBuffers();
    createDescriptorPool();
    ...
}

void HelloTriangleApplication::createDescriptorPool() {}
```

首先，我们需要使用 `vk::DescriptorPoolSize` 结构来描述我们的描述符集将包含哪些描述符类型以及它们的数量。
```cpp
vk::DescriptorPoolSize poolSize{};
poolSize.setDescriptorCount(MAX_FRAMES_IN_FLIGHT)
    .setType(vk::DescriptorType::eUniformBuffer);
```
我们将为每一帧分配一个这样的描述符。这个池大小结构被主`VkDescriptorPoolCreateInfo` 引用：
```cpp
vk::DescriptorPoolCreateInfo poolInfo{};
poolInfo.setPoolSizes(poolSize);
```

除了可用的单个描述符的最大数量，我们还需要指定可以分配的描述符集的最大数量：
```cpp
poolInfo.setMaxSets(MAX_FRAMES_IN_FLIGHT);
```

该结构有一个可选标志，类似于命令池，用于确定是否可以释放单个描述符集合：`vk::DescriptorPoolCreateFlags`。在创建描述符集之后，我们不会去碰它，所以我们不需要这个标志。您可以将 `setFlags` 保留为默认值 0。

添加类成员并创建：
```cpp
vk::DescriptorPool m_DescriptorPool;
...
m_DescriptorPool = m_Device.createDescriptorPool(poolInfo);
if (!m_DescriptorPool)  throw std::runtime_error("failed to create descriptor pool!");
```

### 描述符集合

现在可以分配描述符集合。添加 `createDescriptorSets`：
```cpp

void HelloTriangleApplication::initVulkan() {
    ...
    createDescriptorPool();
    createDescriptorSets();
    ...
}
...

void HelloTriangleApplication::recreateSwapChain()
{
    ...
    createDescriptorPool();
    createdescriptorSets();
    ...
    // 完全不明白这个要加在那里，这教程写的
}

void HelloTriangleApplication::createDescriptorSets() {}
```

描述符集分配是用 `VkDescriptorSetAllocateInfo` 结构体描述的。你需要指定要分配的描述符池，要分配的描述符集的数量，以及基于它们的描述符布局：
```cpp
std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
vk::DescriptorSetAllocateInfo allocInfo{};
allocInfo.setDescriptorPool(m_DescriptorPool)
    .setDescriptorSetCount(MAX_FRAMES_IN_FLIGHT)
    .setSetLayouts(layouts);
```

在本例中，我们将为每个交换链图像创建一个描述符集，它们都具有相同的布局。不幸的是，我们确实需要布局的所有副本，因为下一个函数需要一个与集合数量匹配的数组。

添加一个类成员来保存描述符集句柄，并使用`allocateDescriptorSets` 分配它们：
```cpp
vk::DescriptorPool m_DescriptorPool;
std::vector<vk::DescriptorSet> m_vecDescriptorSets;
...
m_vecDescriptorSets = m_Device.allocateDescriptorSets(allocInfo);
```

不需要显式地清理描述符集，因为当描述符池被销毁时，它们将被自动释放。调用 `allocateDescriptorSets` 将分配描述符集，每个描述符集都有一个统一的缓冲区描述符。
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyDescriptorPool(m_DescriptorPool);
    m_Device.destroyDescriptorSetLayout(m_DescriptorSetLayout);
    ...
}
```

描述符集合仍在分配中，但是里面的描述符需要被指定。添加循环遍历描述符。引用缓冲区的描述符，如我们的统一缓冲区描述符，使用 `VkDescriptorBufferInfo` 结构进行配置。该结构指定了缓冲区和其中包含描述符数据的区域。

```cpp
for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
{
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.setBuffer(m_vecUniformBuffers[i])
        .setOffset(0)
        .setRange(sizeof(UniformBufferObject));
}
```

如果你遍历整个缓冲区，那么也可以使用 `VK_WHOLE_SIZE` 值的范围。描述符的配置是使用 `vkUpdateDescriptorSets` 函数更新的，该函数以一个 `vk::WriteDescriptorSet` 结构数组作为参数。

```cpp
vk::WriteDescriptorSet descriptorWrite{};
descriptorWrite.setDstSet(m_vecDescriptorSets[i])
    .setDstBinding(0)
    .setDstArrayElement(0);
```

前两个字段指定要更新的描述符集和绑定。我们给了统一缓冲区绑定索引 0。请记住，描述符可以是数组，因此我们还需要指定要更新的数组中的第一个索引。我们没有使用数组，所以下标是 0。

```cpp
descriptorWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(1);
```

我们需要再次指定描述符的类型。可以一次更新数组中的多个描述符，从索引 `setDstArrayElement` 开始。`setDescriptorCount` 字段指定要更新多少个数组元素。

```cpp
descriptorWrite.setBufferInfo(bufferInfo)
    .setImageInfo(nullptr)  // Optional
    .setTexelBufferView(nullptr);   // Optional
```

> 那两个 Optional 不能加啊！加了就出错了，救命

最后一个字段引用一个带有 `descriptorCount` 结构体的数组，该结构体实际配置描述符。这取决于你实际需要使用的描述符的类型。`setBufferInfo` 字段用于引用缓冲区数据的描述符，`setImageInfo` 用于引用图像数据的描述符，`setTexelBufferView` 用于引用缓冲区视图的描述符。我们的描述符基于缓冲区，所以我们使用 `setBufferInfo`。
```cpp
m_Device.updateDescriptorSets(descriptorWrite, nullptr);
```

使用 `updateDescriptorSets` 应用更新。它接受两种数组作为参数：`vk::WriteDescriptorSet` 数组和`vk::CopyDescriptorSet` 数组。顾名思义，后者可用于将描述符相互复制。

### 使用描述符集合

我们现在需要更新 `createCommandBuffers` 函数，以实际将每个交换链图像的正确描述符集绑定到着色器中的描述符`vkCmdBindDescriptorSets`。这需要在`vkCmdDrawIndexed` 调用之前完成：
```cpp
commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, m_vecDescriptorSets[imageIndex], nullptr);
commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
```

与顶点和索引缓冲区不同，描述符集并不是图形管道所独有的。因此，我们需要指定是否要将描述符集绑定到图形或计算管道。下一个参数是描述符所基于的布局。接下来的三个参数指定了第一个描述符集的索引、要绑定的集的数量以及要绑定的集的数组。我们一会儿再回到这个问题上。最后两个参数指定用于动态描述符的偏移量数组。我们将在以后的章节中讨论这些问题。

如果您现在运行您的程序，那么您将不幸地注意到什么都不可见。问题是，由于我们在投影矩阵中做了 `y` 翻转，顶点现在是以逆时针顺序绘制的，而不是顺时针顺序。这将导致背面剔除，并阻止任何几何图形被绘制。转到 `createGraphicsPipeline` 函数，修改 `vk::PipelineRasterizationStateCreateInfo` 中的`setFrontFace` 来纠正这一点：
```cpp
rasterizer.setCullMode(vk::CullModeFlagBits::eBack)
    .setFrontFace(vk::FrontFace::eCounterClockwise);
```

运行程序，跑了一会儿卡住了，可能是 `vk::Device::waitForFences` 死锁，报错 `ErrorDeviceLost` 后面再解决。

### 对齐需求

目前略了 C++ 结构中的数据应该如何与着色器中的统一定义相匹配。在两者中使用相同的类型就足够了：
```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
```

尝试修改结构和着色器：
```cpp
struct UniformBufferObject {
    glm::vec2 foo;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

layout(binding = 0) uniform UniformBufferObject {
    vec2 foo;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
```

运行发现彩色的举行消失了。这里需要考虑对齐要求（alignment requirement）。Vulkan 希望结构体的数据以固定方式对齐：
+ 常量以 N（=4字节，32位浮点数等）对齐
+ `vec2` 以 2N（=8字节）对齐
+ `vec3` 或 `vec4` 以 4N（=16字节）对齐
+ 嵌套结构必须按其成员的基本对齐方式四舍五入至16的倍数进行对齐
+ `mat4` 矩阵需要和 `vec4` 一样对齐

原先的着色器其只使用了三个 `mat4` 成员，满足对齐要求。每个 `mat4` 大小为 4x4x4=64 字节，`model` 的偏移为 0,`view` 和 `proj` 的偏移分别为 64 和 128，这些都是 16 的倍数，所以工作正常。

新着色器以 `vec2` 开头，只有 8 字节，偏移出了问题。现在 `model` 的偏移为 8,`view` 的偏移是 72，`proj` 的偏移是 136，都不是 16 的倍数。可以通过使用 C++ 11 的 `alignas` 关键字特定方式改正问题：
```cpp
struct UniformBufferObject {
    glm::vec2 foo;
    alignas(16) glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
```
现在应该没问题了。幸运的是，有一种方法可以在大多数时候不必考虑这些对齐需求。我们可以正确地定义   `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` 
在 GLM 之前：
```cpp
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
```

这将强行让 GLM 使用已经为我们指定了对齐要求的 `vec2` 和 `mat4` 版本。如果您添加了这个定义，那么您可以删除 `alignas` 说明符，您的程序仍然可以工作。
不幸的是，如果开始使用嵌套结构，这种方法可能会失效。考虑 C++ 代码中的以下定义：
```cpp
struct Foo {
    glm::vec2 v;
};

struct UniformBufferObject {
    Foo f1;
    Foo f2;
};
```

和如下着色器代码：
```glsl
struct Foo {
    vec2 v;
};

layout(binding = 0) uniform UniformBufferObject {
    Foo f1;
    Foo f2;
} ubo;
```

在这种情况下，`f2` 的偏移量为 8，但它的偏移量应该为 16，因为它是一个嵌套结构。在这种情况下，必须自己指定对齐方式：
```cpp
struct UniformBufferObject {
    Foo f1;
    alignas(16) Foo f2;
};
```

这些问题是始终明确对齐的好理由。这样，您就不会被对齐错误搞得猝不及防。

```cpp
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
```

### 多个描述符集合

正如一些结构和函数调用所暗示的，可以同时绑定多个描述符集。在创建管道布局时，需要为每个描述符集指定描述符布局。着色器可以引用特定的描述符集，如下所示：
```glsl
layout(set = 0, binding = 0) uniform UniformBufferObject { ... }
```

可以使用此特性将每个对象不同的描述符和共享的描述符放入单独的描述符集。在这种情况下，您可以避免在 `draw` 调用之间重新绑定大多数描述符，这可能更有效。