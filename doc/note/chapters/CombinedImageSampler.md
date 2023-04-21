# Vulkan Tutorial (C++ Ver.)

## 组合图像采样器

### 介绍

在本教程的 `uniform` 缓冲区部分，我们第一次看到了描述符。在本章中，我们将研究一种新的描述符：**组合图像采样器**（combined image sampler）。这个描述符使着色器可以通过一个采样器对象访问资源。

我们将首先修改描述符布局、描述符池和描述符集，以包含这样一个组合的图像采样器描述符。之后，我们将添加纹理坐标到顶点，并修改碎片着色器从纹理读取颜色，而不仅仅是插值顶点颜色。

### 更新描述符

浏览 `createDescriptorSetLayout` 函数并为组合图像采样器添加 `vk::DescriptorSetLayoutBinding`，将它放在 `uboLayoutBinding` 绑定之后：
```cpp
vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
samplerLayoutBinding.setBinding(1)
    .setDescriptorCount(1)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    .setStageFlags(vk::ShaderStageFlagBits::eFragment)
    .setPImmutableSamplers(nullptr);

std::array<vk::DescriptorSetLayoutBinding, 2> bindings = 
{   uboLayoutBinding, samplerLayoutBinding };

vk::DescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.setBindings(bindings);
```

确保设置 `stageFlags` 以表明我们打算在片元着色器中使用组合图像采样器描述符。这就是决定片元颜色的阶段。在顶点着色器中使用纹理采样是可能的，例如，通过高度图动态地变形一个顶点网格。

我们还必须创建一个更大的描述符池，以便通过向`vk::DescriptorPoolCreateInfo` 添加另一个类型为 `vk::DescriptorType::eCombinedImageSampler` 的`vk::PoolSize` 来为组合图像采样器的分配腾出空间。转到`createDescriptorPool` 函数并修改它以包含此描述符的`vk::DescriptorPoolSize`：
```cpp
std::array<vk::DescriptorPoolSize, 2> poolSizes{};
poolSizes[0].setType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);
poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler)
    .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);

vk::DescriptorPoolCreateInfo poolInfo{};
poolInfo.setPoolSizes(poolSizes)
    .setMaxSets(MAX_FRAMES_IN_FLIGHT);
```

描述符池不足是验证层无法捕获的问题的一个很好的例子:在Vulkan 1.1 中，如果池不足够大，`allocateDescriptorSets` 可能会失败，错误代码为`vk::OutOfPoolMemoryError`。但驱动程序也可能尝试在内部解决问题。这意味着有时（取决于硬件、池大小和分配大小）驱动程序会允许我们使用超出描述符池限制的分配。其他时候，`allocateDescriptorSets` 将失败并返回 `vk::OutOfPoolMemoryError`。如果分配在某些机器上成功，而在其他机器上失败，这可能会特别令人抓狂。

由于 Vulkan 将分配的责任转移给了驱动，因此不再严格要求只分配与创建描述符池的相应 `descriptorCount` 成员指定的特定类型（`vk::DescriptorType::eCombinedImageSampler` 等）相同数量的描述符。但是，这样做仍然是最佳实践，并且在将来，如果您启用最佳实践验证，`VK_LAYER_KHRONOS_validation` 将警告此类问题。

最后一步是将实际图像和采样器资源绑定到描述符集中的描述符。转到 `createDescriptorSets` 函数。

```cpp

for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
{
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.setBuffer(m_vecUniformBuffers[i])
        .setOffset(0)
        .setRange(sizeof(UniformBufferObject));

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImageView(m_TextureImageView)
        .setSampler(m_TextureSampler);
        ...
}
```

组合图像采样器结构的资源必须在 `vk::DescriptorImageInfo` 结构中指定，就像在 `vk::DescriptorBufferInfo` 结构中指定统一缓冲区描述符的缓冲区资源一样。这就是前一章的对象集合的地方。

```cpp

std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
descriptorWrites[0].setDstSet(m_vecDescriptorSets[i])
    .setDstBinding(0)
    .setDstArrayElement(0)
    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(1)
    .setBufferInfo(bufferInfo);

descriptorWrites[1].setDstSet(m_vecDescriptorSets[i])
    .setDstBinding(1)
    .setDstArrayElement(0)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
    .setDescriptorCount(1)
    .setImageInfo(imageInfo);

m_Device.updateDescriptorSets(descriptorWrites, nullptr);
```

描述符必须使用此图像信息更新，就像缓冲区一样。这次我们使用的是 `setImageInfo` 数组而不是 `setBufferInfo`。描述符现在可以被着色器使用了!

### 纹理坐标

纹理映射有一个重要的成分仍然缺失，那就是每个顶点的实际坐标。坐标决定了图像实际映射到几何图形的方式。
```cpp

struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};

        bindingDescription.setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].setBinding(0)
            .setLocation(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(Vertex, pos));
        
        attributeDescriptions[1].setBinding(0)
            .setLocation(1)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(Vertex, color));

        attributeDescriptions[2].setBinding(0)
            .setLocation(2)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(Vertex, texCoord));
            
        return attributeDescriptions;
    }
};
```

修改 `Vertex` 结构以包含一个 `vec2` 的纹理坐标。确保还添加了 `vk::VertexInputAttributeDescription`，这样我们就可以在顶点着色器中使用访问纹理坐标作为输入。这是必要的，以便能够将它们传递给片段着色器，以便在正方形表面上进行插值。
```cpp
const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
};
```

在本教程中，我将使用从左上角的 0，0 到右下角的 1，1 的坐标来填充纹理正方形。请随意尝试不同的坐标。尝试使用低于 0 或高于 1 的坐标来查看实际的寻址模式！

### 着色器

最后一步是修改着色器以从纹理中取样颜色。我们首先需要修改顶点着色器，将纹理坐标传递到片段着色器：
```glsl
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0f, 1.0f);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
```

就像每个顶点的颜色一样，`fragTexCoord` 值将通过光栅器平滑地在正方形的区域内插值。我们可以通过片段着色器将纹理坐标输出为颜色来可视化这一点：
```glsl
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragTexCoord, 0.0, 1.0);
}
```

绿色通道表示水平坐标，红色通道表示垂直坐标。黑色和黄色的角确认纹理坐标被正确地从 0，0 插值到 1，1 横跨正方形。使用颜色可视化数据是着色器编程的等效 `printf` 调试，没什么更好的选择了。

在 GLSL 中，组合图像采样器描述符由采样器统一表示。在片段着色器中添加一个引用：
```glsl
layout(binding = 1) uniform sampler2D texSampler;
```

对于其他类型的图像，有等效的 `sampler1D` 和 `sampler3D` 类型。确保在这里使用正确的绑定。

```glsl
void main() {
    outColor = texture(texSampler, fragTexCoord);
}
```

使用内置的纹理函数对纹理进行采样。它将采样器和坐标作为参数。采样器自动处理背景中的过滤和转换。当你运行应用程序时，你现在应该看到正方形上的纹理。

尝试通过将纹理坐标缩放到大于 1 的值来尝试寻址模式。例如，当使用 `vk::SamplerAddressMode::eRepeat` 时，下面的着色器代码将产生不一样的平铺效果：
```glsl
void main() {
    outColor = texture(texSampler, fragTexCoord * 2.0);
}
```

同样也可以使用顶点颜色操作纹理坐标颜色：
```glsl
void main() {
    outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0f);
}
```

注意这里分离了 RGB 通道和颜色通道。

现在你知道如何在着色器中访问图像了。当与在帧缓冲中写入的图像结合使用时，这是一项非常强大的技术。您可以使用这些图像作为输入来实现后期处理和3D世界中的相机显示等炫酷效果。