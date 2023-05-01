# Vulkan Tutorial (C++ Ver.)

## 着色器模块（Shader modules）

不像早期 API，Vulkan 中的着色器代码需要指定为字节码格式，与可读的 GLSL 和 HLSL 不同。字节码格式叫做 SPIR-V，指定用于 Vulkan 和 OpenCL（也是 Khronos API 的）。字节码格式可以用于编写图形和计算着色器，教程就关注用于 Vulkan 图形管线。

字节码格式的优点在于，GPU 供应商编写的将着色器代码转换为本机代码的编译器明显不那么复杂。经验表明，像 GLSL 等可读语法，一些 GPU 供应商对标准的解释很灵活。用户编写的着色器代码可能因为语法错误而被其他供应商的驱动拒绝，而且，可能着色器因为编译器错误而得到不同的结果。使用像 SPIR-V 这样简单的字节码格式，希望能避免这种情况。

用户不需要手工编写字节码。Khronos 发布了独立于供应商的编译器，可以将 GLSL 编译为 SPIR-V。编译器目的是验证你的着色器代码完全符合标准，并产生 SPIR-V 二进制文件，用户可以和程序一起发布。还可以将这个编译器作为三方库运行时生成 SPIR-V，但此教程不会这样做。编译器有 glslangValidator.exe 可用，但这里将通过谷歌使用 glslc.exe。glslc 的优点是它使用与 GCC 和 Clang 等编译器相同的参数格式，并包括一些额外的功能，如 `includes`。它们已经包含在 Vulkan SDK 中，不需要额外下载。

GLSL 是 C 风格语法的着色语言。它的程序有一个主函数，每个对象都会调用该函数。GLSL 使用全局变量处理输入和输出，而不是使用参数作为输入，返回值作为输出。该语言包括许多有助于图形编程的特性，如内置的向量和矩阵原语。函数的操作，如叉乘，矩阵向量积和沿着向量的反射等。

vector 类型称为 `vec`，其中有一个数字表示元素的数量。例如，3D 位置将存储在 `vec3` 中。可以通过 `.x` 等成员访问单个成分，也可以从多个成分创建新向量。例如表达式 `vec3(1.0, 2.0, 3.0).xy` 会得到 `vec2`。向量的构造函数也可以取向量对象和标量的组合。例如，`vec3` 可以用 `vec3(vec2(1.0, 2.0), 3.0)` 构造。

### 顶点着色器（Vertex shader）

顶点着色器处理每个输入的顶点。它接收世界坐标、颜色、法线和纹理坐标等属性作为输入，输出是裁剪坐标的最终位置，以及传递给片段着色器的属性，如颜色和纹理坐标。这些值将在光栅化阶段插值到片元上，以产生平滑的过渡。

裁剪坐标（clip coordinate）是一个来自顶点着色器的四维向量，之后会通过除向量中的最后一个元素变为归一化设备坐标（normalized device coordinate）。归一化的设备坐标是同构的，它通过 [-1, 1] 坐标系统将帧缓冲映射到 [-1, 1]。

OpenGL 的 Y 坐标符号是相反的，Z 坐标使用与 Direct3D 相同的范围，即从 0 到 1。对于第一个三角形，我们不会应用变换，直接指定三个顶点的位置作为归一化的设备坐标来创建形状。

直接输出归一化的设备坐标，将它们作为顶点着色器的裁剪坐标输出，最后一个分量设置为 1。这样，将裁剪坐标转换为归一化设备坐标的除法将不做改变。

通常坐标存储在顶点缓冲区，但在 Vulkan 中创建顶点缓冲区并填充数据并不容易。这里先硬编码写死：
```glsl
#version 450

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, -.5)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
```

`main` 函数在每个顶点上调用。内置的 `gl_VertexIndex` 变量包含当前顶点的下标，但是当前情况下它是硬编码的顶点数组的下标。着色器代码中顶点位置存储在常量数组中，与默认 `z` 和 `w` 的值组成裁剪坐标。内置变量 `gl_Position` 函数作为输出。

### 片元着色器

由来自顶点着色器坐标的顶点组成的三角形，使用片元填充了屏幕。在这些片元上调用的片元着色器计算帧缓冲的颜色和深度。一个简单的输出红色三角形的片元着色器长这样：
```glsl
#version 450

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
```

`main` 函数在每个片元上调用。GLSL 中的颜色是 4 成分的向量，即 R、G、B 和 alpha 通道，值域 [0, 1]。这里没有内置的变量表示输出颜色，需要指定每个帧缓冲的输出，输出位置为 `layout(location = 0)`，表示帧缓冲的下标。红色被写入 `outColor` 变量，并且只连接到下标为 0 的帧缓冲当中。

### 逐顶点颜色

为支持顶点的不同颜色，需要在顶点着色器中指定指定：
```glsl
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
```

将逐顶点的颜色传递给片段着色器，以便片段着色器进行颜色插值并写入帧缓冲。在顶点着色器中添加输出，并在 `main` 函数中进行赋值：
```glsl
layout(location = 0) out vec3 fragColor;
...
void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
```

然后在片元着色器中匹配输入颜色：
```glsl
layout(location = 0) in vec3 fragColor;
...
void main() {
    outColor = vec4(fragColor, 1.0);
}
```

输入变量不一定要同名，它们使用定位指令指定的索引链接在一起。`main` 改为输出颜色和 alpha 值。如上述代码，`fragColor` 的值将自动内插到三个顶点之间的片元中，从而产生平滑的渐变。

### 编译着色器

创建 `shaders` 文件夹，并添加着色器代码文件，顶点着色器 `shader.vert` 和片元着色器 `shader.frag`。GLSL 着色器没有官方的扩展名，`.vert` 和 `.frag` 是常用的。`shader.vert` 的内容如下：
```glsl
#version 450

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, -.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
```

`shader.frag` 内容如下：
```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

现在使用 `glslc` 将代码转为字节码。Windows 下的 `compile.bat` 命令为：
```bat
C:/VulkanSDK/x.x.x.x/Bin32/glslc.exe shader.vert -o vert.spv
C:/VulkanSDK/x.x.x.x/Bin32/glslc.exe shader.frag -o frag.spv
pause
```

Linux 下的 `compile.sh` 命令为：
```shell
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
```

必要时需要添加 `chmod +x` 以修改权限。其他平台的就不说了。

如果着色器包含语法错误，那么编译器会告诉你行号和问题所在，比如去掉分号并再次编译。还可以在不带任何参数的情况下运行编译器，观察支持类型的标志。例如，它还可以将字节码输出为可读格式，就可以看到着色器的工作，以及应用的优化。

在命令行上编译着色器是本教程的操作，也可以直接从代码编译着色器。Vulkan SDK 包括 lib shaderc，是一个在程序中将GLSL 代码编译为 SPIR-V 的库。


### 载入着色器

添加简单的函数从文件中读取二进制数据：
```cpp
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
        file.close();
    }
}
```

`readFile`  函数读取指定文件的字节码，并存储在 `std::vector` 中。使用到的标志分别为
+ `std::ios::ate`：从文件末尾读取
+ `std::ios::binary`：按二进制读取文件（避免文档变换）
从文件末尾读取的优势在于可以使用读取位置决定分配缓冲区的大小：
```cpp
size_t fileSize = file.tellg();
std::vector<char> buffer(fileSize);
```
向前追溯到文件头，并一次读取文件：
```cpp
file.seekg(0);
file.read(buffer.data(), fileSize);
```
最后关闭文件：
```cpp
file.close();
return buffer;
```
在类成员函数 `createGraphicsPipeline` 中调用 `readFile` 并从着色器中读取字节码：
```cpp
void HelloTriangleApplication::createGraphicsPipeline()
{
    auto vertShaderCode = readFile("./src/shaders/vert.spv");
    auto fragShaderCode = readFile("./src/shaders/frag.spv");
}
```

确保着色器加载正确。代码不需要用 `\0` 作为终止符。

### 创建着色器模块

在代码传递给管线之前，代码需要包装到 `vk::ShaderModule` 对象中。创建成员函数 `createShaderModule`：
```cpp
vk::ShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char>& code) {}
```

函数接收字节码缓冲区，并创建 `vk::ShaderModule`。创建着色器模块很简单，只需要指定一个指针指向缓冲区的字节码和长度。该信息在 `vk::ShaderModuleCreateInfo` 结构中指定。问题是字节码的大小是以字节为单位指定的，但是字节码指针是 `uint32_t` 指针而不是 `char` 指针。因此需要使用 `reinterpret_cast` 强制转换指针，需要确保数据满足`uint32_t` 的对齐要求。好在数据存储在 `std::vector` 中，默认的分配器已经确保数据满足最坏情况下的对齐要求。
```cpp
vk::ShaderModuleCreateInfo createInfo;
    createInfo.setCodeSize(code.size())
    .setPCode(reinterpret_cast<const uint32_t*>(code.data()));
```

`vk::ShaderModule` 创建如下：
```cpp
vk::ShaderModule shaderModule = m_Device.createShaderModule(createInfo);
if (!shaderModule)  throw std::runtime_error("failed to create shader module!");
```

着色器模块只是从文件中加载的着色器字节码和其中定义的函数的简单包装。编译和链接 SPIR-V 字节码到机器码，以便由 GPU 执行，直到图形管道被创建。一旦管道创建完成，着色器就可以销毁，所以这里采用 `createGraphicsPipeline` 中的局部变量，而不是类成员:
```cpp
void HelloTriangleApplication::createGraphicsPipeline()
{
    auto vertShaderCode = readFile("./src/shaders/vert.spv");
    auto fragShaderCode = readFile("./src/shaders/frag.spv");

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    ...
```

并在函数末尾销毁（没必要一定反着创建顺序销毁）
```cpp
    ...
    m_Device.destroy(fragShaderModule);
    m_Device.destroy(vertShaderModule);
}
```

### 创建着色器阶段

为了使用着色器，我们需要用 `vk::PipelineShaderStageCreateInfo` 将他们指定到固定渲染阶段：
```cpp
vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
    .setModule(vertShaderModule)
    .setPName("main");
```

`setStage` 指定着色器所处的阶段，`setModule` 指定包含着色器代码的着色器模块，而 `setPName` 指定着色器代码中要调用的函数入口（entrypoint）。这可以将多个片段着色器组合到同一个模块，并使用不同的入口点来区分行为。这里只使用 `main` 作为入口。

还有一个可选成员 `pSpecializationInfo`，这里不用它，但值得讨论。它允许为着色器常量指定值。可以使用单独的着色器模块，通过指定其中使用常量的值来配置不同管线。这比在渲染时使用变量配置着色器更有效，因为编译器可以进行优化，比如消除依赖于这些值的 `if` 语句。如果没有常量，默认为 `nullptr`。

同样为片元着色器添加结构体：
```cpp
vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
    .setModule(fragShaderModule)
    .setPName("main");
```

最后将两个着色器阶段创建信息存储到数组中备用：
```cpp
vk::PipelineShaderStageCreateInfo shaderStagesInfo[] =
    { vertShaderStageInfo, fragShaderStageInfo };
```

管线中可编程阶段的信息准备完成。接下来是固定功能阶段。