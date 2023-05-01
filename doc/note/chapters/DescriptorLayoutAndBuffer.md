# Vulkan Tutorial (C++ Ver.)

## 描述符布局和缓冲区（Description layout and buffer）

### 介绍

现在能够为每个 `Vertex` 传递任意属性到顶点着色器，但是全局变量呢？从本章开始，我们将转向 3D 图形，这需要一个模型—视图—投影矩阵。可以将它包含为顶点数据，但这是一种内存浪费，并且需要在转换发生变化时更新顶点缓冲区。变换可以很容易地改变每一帧。

在 Vulkan 中解决这个问题的正确方法是使用资源描述符。描述符是着色器自由访问缓冲区和图像等资源的一种方式。我们将设置一个包含变换矩阵的缓冲区，并让顶点着色器通过一个描述符访问它们。描述符的用法包括三个部分：
+ 在创建管线的过程中指定描述符布局
+ 从描述符池中分配描述符集合
+ 渲染时绑定描述符集合

描述符布局（descriptor layout）指定将被管线访问的资源的类型，就像渲染通道指定将要访问的附件类型。描述符集合（descriptor set）指定将绑定到描述符的缓冲区或图像资源，就像帧缓冲指定要绑定到渲染传递附件的图像视图一样。描述符集合随后被绑定到绘图命令中，就像顶点缓冲区和帧缓冲一样。

描述符有很多种类型，在本章中，我们将使用统一缓冲对象（Uniform Buffer Objects，UBO）。我们将在以后的章节中看到其他类型的描述符，但基本过程是相同的。假设有想要顶点着色器在 C 结构体中拥有的数据，就像这样：
```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;  
};
```

然后拷贝数据到 `vk::Buffer` 并通过 UBO 从顶点着色器中访问描述符：
```glsl
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0f, 1.0f);
    fragColor = inColor;
}
```

在每一帧更新 `model`，`view` 和 `proj` 矩阵，让之前创建的举行转起来。

### 顶点着色器

修改顶点着色器以包括上面指定的统一缓冲对象。
```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;


void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0f, 1.0f);
    fragColor = inColor;
}
```

注意 `uniform`，`in` 和 `out` 声明的顺序无关紧要。`binding` 指令类似于属性的定位指令。

我们将在描述符布局中引用这个绑定。带有 `gl_Position` 的行被更改为使用转换来计算裁剪坐标中的最终位置。与 2D 三角形不同，裁剪坐标的最后一个组件可能不是1，这将导致在屏幕上转换为最终规范化设备坐标时出现除法。这在透视投影中用作透视划分，对于使近距离的物体看起来比远距离的物体大是必不可少的。

### 描述符布局设置

下一步 C++ 端定义 UBO，并在顶点着色器中告诉 Vulkan 这个描述符。
```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;  
};
```

我们可以使用 GLM 中的数据类型精确匹配着色器中的定义。矩阵中的数据与着色器期待的数据二进制兼容。稍后可以 `memcpy` 一个`UniformBufferObject` 到一个 `vk::Buffer`。

我们需要提供用于创建管线的着色器中使用的每个描述符绑定的详细信息，就像我们必须为每个顶点属性及其位置索引做的那样。添加 `createDescriptorSetLayout` 函数定义这些信息。它应该在管道创建之前被调用，因为我们会在那里需要它。
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createDescriptorSetLayout();
    createGraphicsPipeline();
    ...
}

void HelloTriangleApplication::createDescriptorSetLayout() {}
```

每个绑定都需要通过 `` 结构体描述。
```cpp

void HelloTriangleApplication::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1);
}
```

前两个字段指定着色器中使用的绑定和描述符的类型，这是一个统一的缓冲对象。着色器变量可以表示一个统一缓冲对象的数组，而 `setDescriptorCount` 指定数组中值的数量。例如，这可以用于为骨骼动画中的每个骨骼指定一个转换值。我们的 MVP 转换在一个统一的缓冲区对象中，因此 `setDescriptorCount` 为 1。

```cpp
uboLayoutBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
```

我们还需要指定描述符将在哪个着色器阶段被引用。`setStageFlags` 字段可以是 `vk::ShaderStageFlags` 值或 `vk::ShaderStageFlagBits::eAllGraphics` 值的组合。在我们的例子中，我们只引用顶点着色器的描述符。

```cpp
uboLayoutBinding.setPImmutableSamplers(nullptr);    // Optional
```

`setPImmutableSamplers` 字段只与图像采样相关的描述符相关。您可以保留它的默认值。

所有描述符绑定都被组合到一个 `vk::DescriptorSetLayout` 对象中。在 `vk::PipelineLayout` 上面定义一个新类成员：
```cpp
vk::DescriptorSetLayout m_DescriptorSetLayout;
vk::PipelineLayout m_PipelineLayout;
```

然后用 `vk::createDescriptorSetLayout` 创建它。此函数接受提供一个简单的 `vk::DescriptorSetLayoutCreateInfo` 和绑定数组：
```cpp
vk::DescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.setBindings(uboLayoutBinding);
m_DescriptorSetLayout = m_Device.createDescriptorSetLayout(layoutInfo);
if (!m_DescriptorSetLayout)
    throw std::runtime_error("failed to create descriptor set layout!");
```

我们需要在管线创建期间指定描述符集布局，以告诉 Vulkan 着色器将使用哪些描述符。描述符集合布局在管线布局对象中指定。修改 VkPipelineLayoutCreateInfo 来引用布局对象：
```cpp
vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.setSetLayouts(m_DescriptorSetLayout);
```

您可能想知道为什么可以在这里指定多个描述符集合布局，因为一个描述符集合已经包含了所有的绑定。我们将在下一章中回到这一点，在那里我们将研究描述符池和描述符集合。当我们创建新的图形管线时，描述符布局应该保持不变，即直到程序结束：
```cpp
void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroyDescriptorSetLayout(m_DescriptorSetLayout);
    ...
}
```

### 一致缓冲区（Uniform buffer）

在下一章中，我们将为着色器指定包含 UBO 数据的缓冲区，但需要首先创建这个缓冲区。每一帧都要把新数据复制到统一缓冲区中，所以使用暂存缓冲区没有任何意义。在这种情况下，它只会增加额外的开销，可能会降低性能。

我们应该有多个缓冲区，因为多个帧可能同时在运行，我们不希望在上一帧还在读取时更新缓冲区来准备下一帧。因此，我们需要有和正在运行的帧一样多的一致缓冲区，并写入当前不被 GPU 读取的统一缓冲区。为此，为 `uniformBuffers` 和 `uniformbuffermemory` 添加新的类成员：
```cpp
vk::Buffer m_IndexBuffer;
vk::DeviceMemory m_IndexBufferMemory;

std::vector<vk::Buffer> m_vecUniformBuffers;
std::vector<vk::DeviceMemory> m_vecUniformBuffersMemory;
```

类似的，创建新函数 `createUniformBuffers`，在 `initVulkan` 之后的 `createIndexBuffer` 之后调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    ...
}
...

void HelloTriangleApplication::createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    m_vecUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_vecUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        createBuffer(bufferSize, 
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
            m_vecUniformBuffers[i],
            m_vecUniformBuffersMemory[i]);
}
```

而且需要在 `recreateSwapChain` 中调用：
```cpp

void HelloTriangleApplication::recreateSwapChain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    while(width == 0 || height == 0) {
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        glfwWaitEvents();
    }

    m_Device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createUniformBuffers();
}
```
> 注意，这里和教程里不一样，教程里还调用了 `createCommandBuffers`

### 更新 `uniform` 数据

```cpp

void HelloTriangleApplication::drawFrame()
{
    ...
    uint32_t imageIndex = value.value;
    ...
    updateUniformBuffer(imageIndex);

    vk::SubmitInfo submitInfo{};
    ...
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage) {}
```

这个函数将在每一帧生成一个新的变换，使几何旋转。我们需要包含两个新的头文件来实现这个功能：
```cpp
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
```

`glm/gtc/matrix_transform.hpp` 头文件提供了用于生成变换矩阵的函数如 `glm::rotate`，生成视觉变换矩阵的函数如 `glm::lookAt` 和生成投影变换矩阵的函数 `glm::perspective`。`GLM_FORCE_RADIANS` 宏定义指定类似 `glm::rotate` 函数必须使用弧度制参数，避免可能的误导。

`chrono` 标准库提供了精确的计时函数。这里使用它做独立于帧率的每秒 90 度的旋转。
```cpp
void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
}
```

`updateUniformBuffer` 函数将从一些以秒为单位计算时间的逻辑开始，因为呈现是以浮点精度开始的。
现在，我们将在 `uniform` 缓冲对象中定义模型、视图和投影转换。模型旋转将是使用时间变量围绕 `z` 轴的简单旋转：
```cpp
UniformBufferObject ubo{};
ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
```

`rotate` 函数将现有的变换、旋转角度和旋转轴作为参数。`glm::mat4(1.0f)` 构造函数返回单位矩阵（identity）。使用时间的旋转角度 `* glm::radians(90.0f)` 达到每秒旋转90 度的目的。

```cpp
ubo.view = glm::lookAt(glm::vec3(2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
```

对于视图变换，决定以 45 度角从上面看几何图形。`lookAt` 函数将眼睛位置、中心位置和向上的轴向作为参数。

```cpp
ubo.proj = glm::perspective(glm::radians(45.0f), 
    static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height),
        0.01f, 10.0f);
```

选择使用 45 度垂直视觉场的透视投影。其他参数是纵横比，近视图平面和远视图平面。重要的是使用当前的交换链来计算纵横比，以考虑调整大小后窗口的新宽度和高度。

```cpp
ubo.proj[1][1] *= -1;
```

GLM 最初是为 OpenGL 设计的，其中裁剪坐标的 Y 坐标是倒置的。最简单的方法是在投影矩阵 Y 轴的比例因子上翻转符号。如果你不这样做，那么呈现的图像将会颠倒。

所有转换现在都已定义，因此我们可以将统一缓冲区对象中的数据复制到当前统一缓冲区。这与我们对顶点缓冲区所做的完全相同，只是没有暂存缓冲区：
```cpp
void* data;
const auto&_ = m_Device.mapMemory(m_vecUniformBuffersMemory[currentImage], 0, sizeof(ubo), vk::MemoryMapFlags{0}, &data);
memcpy(data, &ubo, sizeof(ubo));
m_Device.unmapMemory(m_vecUniformBuffersMemory[currentImage]);
```

以这种方式使用 UBO 并不是将频繁变化的值传递给着色器的最有效的方法。向着色器传递少量数据缓冲区的更有效方法是`push Constant`。我们将在以后的章节中讨论这些问题。
在下一章中，我们将研究描述符集合，将 `vk::Buffers` 绑定到统一的缓冲区描述符，以便着色器可以访问这个变换数据。