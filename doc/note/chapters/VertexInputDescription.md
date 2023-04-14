# Vulkan Tutorial (C++ Ver.)

## 顶点输入描述（Vertex input description）

### 介绍

接下来将用内存中的顶点缓冲区替换顶点着色器中的硬编码顶点数据。创建 CPU 可见缓冲区并使用 `memcpy` 直接将顶点数据复制到其中，然后使用暂存缓冲区将顶点数据复制到高性能内存中。

### 顶点着色器

首先修改顶点着色器代码，代码中不再包含顶点数据。顶点着色器使用 `in` 关键字从顶点缓冲区获取输入：
```glsl
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
```

`inPosition` 和 `inColor` 变量是**顶点属性**（vertex attributes）。它们是顶点缓冲区中每个顶点指定的属性，就像手动指定每个顶点的位置和颜色一样。重新编译顶点着色器。

就像 `fragColor`，`layout(location = x)` 记号表示为输入分配索引，以便以后引用。一些类型，如 `dvec3` 64 位向量，使用多个插槽。这意味着后面的指数至少要多 2：
```glsl
layout(location = 0) in dvec3 inPosition;
layout(location = 2) in vec3 inColor;
```

在 OpenGL wiki 上可以查到更多信息。

### 顶点数据

我们将顶点数据从着色器代码中移动到程序代码的数组中。首先包含`GLM` 库，它提供了线性代数相关的类型，如向量和矩阵。使用这些类型指定位置和颜色向量：
```cpp
#include <glm/glm.hpp>
```

创建一个名为 `Vertex` 的结构体，包含将在顶点着色器中使用的两个属性：
```cpp
struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;
};
```

GLM 为我们提供了匹配着色器语言用到的向量类型的 C++ 实现。

```cpp
const std::vector<Vertex> vertices = {
    { {  0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } },
    { { -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }
};
```

适用 `Vertex` 结构体指定一个顶点数组。使用与之前完全相同的位置和颜色值，组合到一个顶点数组中。这被称为**交错**（interleaving）顶点属性。

### 绑定描述（Binding descriptions）

然后告诉 Vulkan 如何将 `Vertex` 格式数据传递给顶点着色器，并上传到 GPU 内存。传递此信息需要两种结构，
第一个结构是 `VkVertexInputBindingDescription` ，添加成员函数 `getBindingDescription` 到 `Vertex`，并填充：
```cpp
struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};

        return bindingDescription;
    }
};
```

顶点绑定描述了在整个顶点中从内存中加载数据的速率。它指定数据项之间的字节数，以及是在每个顶点之后还是在每个实例之后移动到下一个数据项：
```cpp
static vk::VertexInputBindingDescription getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription{};

    bindingDescription.setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);

    return bindingDescription;
}
```

所有逐顶点数据打包在一个数组中，所以只有一个绑定。`setBinding` 函数指定绑定在绑定数组中的索引。`setStride` 函数指定从一个条目到下一个条目的字节数，`setInputRate` 参数值可以是：
+ `vk::VertexInputRate::eVertex`：移动到每个顶点之后的下一个数据项
+ `vk::VertexInputRate::eInstance`：移动到每个实例之后的下一个数据项

这里不打算使用实例渲染，所以将适用顶点数据。

### 属性描述（Attribute descriptions）

第二个结构是 `vk::VertexInputAttributeDescription`，添加另一个辅助函数到 `Vertex` 来填充结构体：
```cpp
static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

    return attributeDescriptions;
}
```

如函数所示，有两种这样的结构。属性描述结构体描述了如何从来自于绑定描述的顶点数据块中提取顶点属性。对于两个属性，`position` 和 `color`，需要两个属性描述结构。

```cpp
attributeDescriptions[0].setBinding(0)
    .setLocation(0)
    .setFormat(vk::Format::eR32G32Sfloat)
    .setOffset(offsetof(Vertex, Vertex::pos));
```

`setBinding` 函数告诉 Vulkan 逐顶点数据来自哪个绑定。`setLocation` 函数引用顶点着色器中输入的位置指令。位置为0 的顶点着色器的输入是位置，它有两个 32 位浮点成员。

`setFormat` 函数指定属性的数据类型。格式使用与颜色格式相同的枚举。着色器类型和格式通常一起使用：
+ `float`：`vk::Format::eR32Sfloat`
+ `vec2`：`vk::Format::eR32G32Sfloat`
+ `vec3`：`vk::Format::eR32G32B32Sfloat`
+ `vec4`：`vk::Format::eR32G32B32A32Sfloat`

应该使用颜色通道的数量与着色器数据类型中组件的数量相匹配的格式。它允许使用比着色器中组件数量更多的通道，但它们将被丢弃。如果通道的数量低于组件的数量，那么 BGA 组件将使用默认值 `(0,0,1)`。颜色类型（`SFLOAT`，`UINT`，`SINT`）和位宽也应该匹配着色器输入的类型。例如：
+ `ivec2`：`vk::Format::eR32G32Sint`，具有两个 32 位有符号整数成分的向量
+ `uvec4`：`vk::Format::eR32G32B32A32Uint`，具有四个 32 位无符号整数成分的向量
+ `double`：`vk::Format::eR64Sfloat`，双精度（64 位）浮点数

`setFormat` 函数参数隐式定义属性数据的字节大小，`setOffset` 参数指定从逐顶点数据开始读取的字节数。绑定每次加载一个顶点，位置属性（`pos`）与该结构的开头相差 0 个字节。使用宏偏移量自动计算。

```cpp
attributeDescriptions[1].setBinding(0)
    .setLocation(1)
    .setFormat(vk::Format::eR32G32B32Sfloat)
    .setOffset(offsetof(Vertex, Vertex::color));
```

颜色属性也同样设置。

### 管线顶点输入（Pipeline vertex input）

现在设置图形管道，通过引用 `createGraphicsPipeline` 函数中的结构来接受这种格式的顶点数据。修改 `vertexInputInfo` 结构体并引用两个描述：
```cpp
auto bindingDescription = Vertex::getBindingDescription();
auto attributeDescriptions = Vertex::getAttributeDescriptions();

vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.setVertexBindingDescriptions(bindingDescription)
    .setVertexAttributeDescriptions(attributeDescriptions);
```

管线已经准备好接受顶点容器格式的顶点数据，并将其传递给顶点着色器。如果现在在启用验证层的情况下运行程序，将看到它提示绑定上没有顶点缓冲区。下一步是创建一个顶点缓冲区，并将顶点数据移动到其中，以便 GPU 能够访问它。

报错信息类似
```bash
validation layerValidation Error: [ VUID-vkCmdDraw-None-02721 ] Object 0: handle = 0x555556939bd0, type = VK_OBJECT_TYPE_COMMAND_BUFFER; Object 1: handle = 0xe88693000000000c, type = VK_OBJECT_TYPE_PIPELINE; | MessageID = 0x99ef63bb | vkCmdDraw: binding #0 in pVertexAttributeDescriptions[1] of VkPipeline 0xe88693000000000c[] is an invalid value for command buffer VkCommandBuffer 0x555556939bd0[]. The Vulkan spec states: For a given vertex buffer binding, any attribute data fetched must be entirely contained within the corresponding vertex buffer binding, as described in Vertex Input Description (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkCmdDraw-None-02721)
```