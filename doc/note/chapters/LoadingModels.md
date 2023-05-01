# Vulkan Tutorial (C++ Ver.)

## 加载模型

### 介绍

您的程序现在已经准备好渲染纹理 3D 网格，但是顶点和索引数组中的当前几何形状还不是很有趣。在本章中，我们将扩展程序，从实际的模型文件中加载顶点和索引，以使显卡实际做一些工作。

许多图形 API 教程都要求读者在这样的章节中编写自己的 OBJ 加载器。这样做的问题是，任何远程有趣的 3D 应用程序很快就会需要这种文件格式不支持的特性，比如骨骼动画。在本章中，我们将从 OBJ 模型中加载网格数据，但我们将更多地关注网格数据与程序本身的集成，而不是从文件中加载数据的细节。

### 三方库

我们将使用 tinyobjloader 库从 OBJ 文件中加载顶点和面。它很快，而且很容易集成，因为它是一个像 stb_image 一样的单一文件库。转到上面链接的存储库，并将 tiny_obj_loader.h 文件下载到库目录中的一个文件夹中。请确保使用来自主分支的文件版本，因为最新的官方版本已经过时了。

### 实例模型

在本章中，我们将不启用照明，所以它有助于使用一个样本模型，照明烘焙到纹理。找到这种模型的一个简单方法是在 Sketchfab上 寻找3D扫描。该站点上的许多模型都以具有自由许可证的 OBJ 格式提供。

在本教程中，我决定使用 Nigelgoh (CC by 4.0) 的维京房间模型。我调整了模型的大小和方向，将其用作当前几何体的替代品：
+ `viking_room.obj`：`src/models/viking_room/viking_room.obj`
+ `viking_room.png`：`src/models/viking_room/viking_room.png`

您可以随意使用自己的模型，但要确保它只由一种材料组成，并且尺寸约为 1.5x1.5x1.5 单位。如果它比这个大，那么你就必须改变视图矩阵。

```cpp
const uint32_t m_Width = 600;
const uint32_t m_Height = 400;

const std::string MODEL_PATH = "./src/models/viking_room/viking_room.obj";
const std::string TEXTURE_PATH = "./src/models/viking_room/viking_room.png";
```

并更新 `createTextureImage`，使用路径：
```cpp
stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
```

### 加载顶点和索引

我们现在要从模型文件中加载顶点和索引，所以你现在应该移除全局顶点和下标数组。将它们替换为非 const 容器作为类成员：
```cpp
std::vector<Vertex> m_Vertices;
std::vector<uint32_t> m_Indices;
vk::Buffer m_VertexBuffer;
vk::DeviceMemory m_VertexBufferMemory;
```

你应该把索引的类型从 `uint16_t` 改为 `uint32_t`，因为会有比 65535 更多的顶点。记住也要改变 `bindIndexBuffer` 参数：
```cpp
commandBuffer.bindIndexBuffer(m_IndexBuffer, 0, vk::IndexType::eUint32);
```

tinyobjloader 库的包含方式与 STB 库相同。包括 `tiny_obj_loader.h` 文件，并确保在一个源文件中定义 `TINYOBJLOADER_IMPLEMENTATION` 以包含函数体并避免链接错误：
```cpp
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
```

添加 loadModel 函数，它使用这个库来填充顶点和索引容器，其中包含来自网格的顶点数据。它应该在创建顶点和索引缓冲区之前的某个地方调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    ...
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    ...
}


void HelloTriangleApplication::loadModel() {}
```

通过调用 `tinyobj::LoadObj` 函数将模型加载到库的数据结构中：
```cpp
void HelloTriangleApplication::loadModel()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
        throw std::runtime_error(warn + err);
}
```

一个 OBJ 文件由位置、法线、纹理坐标和面组成。面由任意数量的顶点组成，其中每个顶点通过索引指代一个位置、法线和/或纹理坐标。这使得不仅可以重用整个顶点，还可以重用单个属性。

属性容器在 `attrib.vertices`，`attrib.normals` 和 `attrib.texcoords` 中保存其属性中的所有位置、法线和纹理坐标。形状容器包含所有单独的对象及其面。每个面由一组顶点组成，每个顶点包含位置、法线和纹理坐标属性的索引。OBJ 模型也可以定义每个面的材质和纹理，但我们将忽略这些。

`err` 字符串包含错误，而 `warn` 字符串包含加载文件时发生的警告，例如缺少材料定义。只有当 LoadObj 函数返回 `false` 时，加载才真正失败。如上所述，OBJ 文件中的面实际上可以包含任意数量的顶点，而我们的应用程序只能呈现三角形。幸运的是，LoadObj 有一个可选参数来自动三角化这些面，这在默认情况下是启用的。

我们将把文件中的所有面合并到一个模型中，所以只需遍历所有形状：
```cpp
for(const auto& shape : shapes)
{
    
}
```

三角测量功能已经确保了每个面有三个顶点，所以我们现在可以直接迭代顶点，并将它们直接转储到我们的顶点向量中：

```cpp
for(const auto& shape : shapes)
{
    for(const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        m_Vertices.emplace_back(vertex);
        m_Indices.emplace_back(indices.size());
    }
}
```

为了简单起见，我们现在假设每个顶点都是唯一的，因此使用简单的自动增量索引。索引变量的类型是 `tinyobj::index_t`，它包含 `vertex_index`、`normal_index` 和 `texcoord_index` 成员。我们需要使用这些索引来查找属性数组中的实际顶点属性：
```cpp
vertex.pos = {
    attrib.vertices[3 * index.vertex_index + 0],
    attrib.vertices[3 * index.vertex_index + 1],
    attrib.vertices[3 * index.vertex_index + 2]};

vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
    attrib.texcoords[2 * index.texcoord_index + 1]};

vertex.color = { 1.0f, 1.0f, 1.0f };
```

不幸的是，这个 `attrib.vertices` 数组是一个浮点值数组，而不是像 `glm::vec3` 这样的数组，所以你需要将索引乘以 3。类似地，每个条目有两个纹理坐标组件。0，1 和 2 的偏移量用于访问 X，Y 和 Z 分量，或者在纹理坐标的情况下访问 U 和 V 分量。

在启用了优化的情况下运行你的程序（例如，在Visual Studio中使用发布模式，在 GCC 中使用 -O3 编译器标志）。这是必要的，因为否则加载模型将非常缓慢。

很好，几何图形看起来正确，但纹理不对。OBJ 格式假设一个坐标系统，其中垂直坐标为 0 表示图像的底部，但是我们已经将图像上传到 Vulkan 中，从上到下的方向，其中 0 表示图像的顶部。通过翻转纹理坐标的垂直分量来解决这个问题：
```cpp
vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
    1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
};
```

### 顶点重复数据删除

不幸的是，我们还没有真正利用索引缓冲区。顶点向量包含了大量重复的顶点数据，因为许多顶点包含在多个三角形中。我们应该只保留唯一的顶点，并在它们出现时使用索引缓冲区来重用它们。实现这一点的一个简单方法是使用 `map` 或 `unordered_map` 来跟踪唯一的顶点和各自的索引：

```cpp
#include <unordered_map>

...

std::unordered_map<Vertex, uint32_t> uniqueVertices{};

for(const auto& shape : shapes)
{
    for(const auto& index : shape.mesh.indices) {
        Vertex vertex{};
        ...
        if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] = m_Vertices.size();
            m_Vertices.emplace_back(vertex);
        }
        
        m_Indices.emplace_back(uniqueVertices[vertex]);
    }
}
```

每次我们从 OBJ 文件中读取一个顶点时，我们都会检查之前是否已经看到了一个具有完全相同位置和纹理坐标的顶点。如果不是，我们将其添加到顶点，并将其索引存储在 `uniqueVertices` 容器中。之后，我们将新顶点的索引添加到索引中。如果我们之前见过完全相同的顶点，那么我们就在 `uniqueVertices` 中查找它的索引并将索引存储在 `m_Indices` 中。

程序现在将无法编译，因为使用用户定义的类型（如 `Vertex` 结构）作为哈希表中的键需要实现两个函数：相等性测试和哈希计算。前者很容易通过覆盖 `Vertex` 结构体中的 `operator==` 操作符来实现：

```cpp
bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
}
```

`Vertex` 的哈希函数是通过为 `std::hash<T>` 指定一个模板特化来实现的。哈希函数是一个复杂的主题，但 cppreference.com 推荐以下方法组合结构的字段来创建一个体面的质量哈希函数：
```cpp

namespace std{
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1) >> 1) ^
                    (hash<glm::vec2>()(vertex.texCoord) << 1));
        }
    };
}
```

此函数应该放在 `Vertex` 结构体之外，GLM 类型的哈希函数应该额外包含进来：
```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
```

散列函数是在 gtx 文件夹中定义的，这意味着从技术上讲，它仍然是 GLM 的实验性扩展。因此，您需要定义 `GLM_ENABLE_EXPERIMENTAL` 来使用它。这意味着 API 可能会随着未来新版本的 GLM 而改变，但实际上 API 是非常稳定的。

现在应该能够成功编译并运行程序了。如果你检查顶点的大小，你会发现它已经从 1,500,000 缩小到 265,645。这意味着每个顶点在平均约6个三角形中被重用。这无疑为我们节省了大量 GPU 内存。

