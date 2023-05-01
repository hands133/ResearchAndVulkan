# Vulkan Tutorial (C++ Ver.)

## 固定功能（Fixed functions）

旧 API 为图形管线的绝大部分阶段提供默认功能。Vulkan 中需要显式明确一切，从视窗大小到颜色混合函数。

### 顶点输入

结构体 `vk::PipelineVertexInputStateCreateInfo` 描述了将传递给顶点着色器的顶点数据的格式，简单的说是两个方面：
+ 绑定：数据的间距以及数据是逐顶点还是逐实例（参见instancing）
+ 属性描述：传递给顶点着色器的属性类型，哪个绑定加载它们以及在哪个偏移量（啥鬼翻译了哇）

由于顶点位置是硬编码写在顶点着色器里的，需要填充结构体指定当前不需要加载顶点数据。后面会填充：
```cpp
vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.setVertexBindingDescriptions({})
    .setVertexAttributeDescriptions({});
```

`setVertexBindingDescriptions` 和 `setVertexAttributeDescriptions` 描述之前提到的加载顶点着色器的细节。将上述代码添加到 `createGraphicsPipeline` 函数的 `shaderStages` 之后。

### 输入装配

结构体 `vk::PipelineInputAssemblyStateCreateInfo` 描述了两件事：从顶点绘制什么样的图形，以及是否应该启用原语重启。前者使用 `setTopology` 设置可能的取值：
+ `vk::PrimitiveTopology::ePointList`：来自顶点的点
+ `vk::PrimitiveTopology::eLineList`：每 2 顶点组成的线，不重用顶点
+ `vk::PrimitiveTopology::eLineStrip`：每条线的结束顶点用于下一条线的起始顶点
+ `vk::PrimitiveTopology::eTriangleList`：每 3 个顶点组成三角形，不重用顶点
+ `vk::PrimitiveTopology::eTriangleStrip`：每个三角形的第二个和第三个顶点用于下一个三角形的第一个和第二个顶点

通常情况下，顶点按顺序从顶点缓冲区中按索引加载，但是使用元素缓冲区，可以指定索引。这允许执行优化，如重用顶点。如果将 `primitiveRestartEnable` 成员设置为 `VK_TRUE`，则可以使用 `0xFFFF` 或 `0xFFFFFFFF` 的特殊索引来分解`_STRIP` 拓扑模式中的直线和三角形。

教程倾向于绘制三角形，所以按如下方式填充结构体：
```cpp
vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
    .setPrimitiveRestartEnable(false);
```

### 视窗和裁剪

视窗（viewport）基本描述了要渲染的帧缓冲的区域，基本上是 (0, 0) 到 (`width`, `height`)，本教程就这么干：
```cpp
vk::Viewport viewport{};
    viewport.setX(0.0f)
    .setY(0.0f)
    .setWidth(m_SwapChainExtent.width)
    .setHeight(m_SwapChainExtent.height)
    .setMinDepth(0.0f)
    .setMaxDepth(1.0f);
```

交换链的大小和图像可能与图片的 `WIDTH` 和 `HEIGHT` 不同。交换链图像被用于后面的帧缓冲，所以固定尺寸不改变。

`setMinDepth` 和 `setMaxDepth` 指定帧缓冲的深度值范围。深度值在 [0.0f, 1.0f] 之间，但是 `setMinDepth` 的值可以比 `setMaxDepth` 大。如果没有特殊的要求，就默认最小值为 0.0f，最大值为 1.0f。

视窗定义了从图像到帧缓冲的转换，而剪刀矩形定义了实际存储像素的区域。剪刀矩形之外的像素都被光栅化器丢弃。它们的功能更像过滤器而不是转换。

本教程绘制整个帧缓冲，所以剪刀矩形应该覆盖帧缓冲：
```cpp
vk::Rect2D scissor{};
scissor.setOffset(vk::Offset2D(0, 0))
        .setExtent(m_SwapChainExtent);
```

使用 `vk::PipelineViewportStateCreateInfo` 结构体将视窗和剪刀整合到渲染管线当中。在一些图形卡上可以使用多个视窗和剪刀矩形，所以参数为数组类型。使用多个视窗需要开启 GPU 特征（查看逻辑设备创建章节）：

```cpp
vk::PipelineViewportStateCreateInfo viewportState;
viewportState.setViewports(viewport)
    .setScissors(scissor);
```

### 光栅化

光栅化接收来自顶点着色器的顶点组成的几何数据，并将其变为要在片元着色器中上色的片元。此阶段也做深度测试、面剔除和裁剪测试，而且可以指定输出的片元是完全多边形填充，或者只有边（渲染线框）。使用结构体 `vk::PipelineRasterizationStateCreateInfo` 进行配置：
```cpp
vk::PipelineRasterizationStateCreateInfo rasterizer{};
rasterizer.setDepthClampEnable(false);
```

如果 `setDepthClampEnable` 设置为 `true`，那么超出远近平面的片元将被挤压到视锥体内而不是直接丢弃。某些类似 shadow map 的场景下用得到。使用它需要开启 GPU 特性。
```cpp
rasterizer.setRasterizerDiscardEnable(false);
```

如果 `setRasterizerDiscardEnable` 设为 `true`，那么几何将不经过光栅化阶段。这个设置基本阻断了任何到帧缓冲的输出。

```cpp
rasterizer.setPolygonMode(vk::PolygonMode::eFill);
```

`setPolygonMode` 决定如何从几何生成片元，有几种选项：
+ `vk::PolygonMode::eFill`：用片元填充多边形区域
+ `vk::PolygonMode::eLine`：多边形的边绘制为线
+ `vk::PolygonMode::ePoint`：多边形的顶点绘制为点

任何其他的选项需要开启 GPU 特性。

```cpp
rasterizer.setLineWidth(1.0f);
```

`setLineWidth` 设置线的宽度，对应于片元的数量。最大线宽取决于硬件，任何大于 1.0f 的线宽需要开启 GPU 的 `wideLines` 特性。

```cpp
rasterizer.setCullMode(vk::CullModeFlagBits::eBack)
    .setFrontFace(vk::FrontFace::eClockwise)
```

`setCullMode` 决定面剔除的类型，可以取消剔除，剔除前向面，剔除后向面或者全剔除。`setFrontFace` 设置面的顶点顺序是按顺时针顺序考虑还是逆时针顺序考虑。

```cpp
rasterizer.setDepthBiasEnable(false)
    .setDepthBiasConstantFactor(0.0f)   // Optional
    .setDepthBiasClamp(0.0f)            // Optional
    .setDepthBiasSlopeFactor(0.0f);     // Optional
```

光栅化可以基于片元的坡度，通过添加或便宜值来修改深度值，有时用于 shadow mapping，这里不考虑，采用默认值即可。


### 多重采样（Multisampling）

结构体 `vk::PipelineMultisampleStateCreateInfo` 指定了多重采样，是抗锯齿的方法的其中一种。原理是将多个多边形光栅化到同一像素的片段着色器结果结合起来。这主要发生在边缘，也是最明显的锯齿出现的地方。如果只有一个多边形映射到一个像素，它不需要多次运行片段着色器，这比简单地渲染到更高的分辨率然后缩放要高效得多。启用它需要启用 GPU 特性。

```cpp
vk::PipelineMultisampleStateCreateInfo multisampling{};
multisampling.setSampleShadingEnable(false)
    .setRasterizationSamples(vk::SampleCountFlagBits::e1)
    .setMinSampleShading(1.0f)      // Optional
    .setPSampleMask(nullptr)        // Optional
    .setAlphaToCoverageEnable(false)// Optional
    .setAlphaToOneEnable(false);    // Optional
```

具体细节后续讨论。

### 深度测试和模板测试

如果使用深度和/或模板缓冲，需要使用结构体 `vk::PipelineDepthStencilStateCreateInfo` 指定深度测试和模板测试。这里还不需要，到时候传递 `nullptr` 即可。后面会涉及此部分内容。

### 颜色混合

片元着色器返回了颜色，需要结合已经在帧缓冲中的颜色。此变换也即颜色混合，有两种方法可以做到：
+ 混合旧值和新值以得到最终颜色
+ 通过位操作结合旧值和新值

配置颜色混合需要两种结构体。第一个是 `vk::PipelineColorBlendAttachmentState`，包含附加帧缓冲的配置，第二个结构体 `vk::PipelineColorBlendStateCreateInfo` 包含全局的颜色混合设置。这里只有一个帧缓冲：
```cpp
vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlags(
    vk::ColorComponentFlagBits::eR |
    vk::ColorComponentFlagBits::eG |
    vk::ColorComponentFlagBits::eB |
    vk::ColorComponentFlagBits::eA))
    .setBlendEnable(false)
    .setSrcColorBlendFactor(vk::BlendFactor::eOne)  // Optional
    .setDstColorBlendFactor(vk::BlendFactor::eZero) // Optional
    .setColorBlendOp(vk::BlendOp::eAdd)  // Optional
    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)  // Optional
    .setDstAlphaBlendFactor(vk::BlendFactor::eZero) // Optional
    .setAlphaBlendOp(vk::BlendOp::eAdd);    // Optional
```
这是逐帧缓冲的结构体，允许用户指定颜色混合的方式。将要执行的操作可用下述伪代码表示：
```psecode
if (blendEnable) {
    finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOP> (dstColorBlendFactor * oldColor.rgb);
    finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOP> (dstAlphaBlendFactor * oldColor.a);
} else {
    finalColor = newColor;
}

finalColor = finalColor & colorWriteMash;
```

如果 `blendEnable` 设置为 `false`，来自片段着色器的新颜色将被不经修改地传递。否则，对颜色进行混合操作并计算新颜色。最终颜色与 `colorWriteMask` 做与运算，决定哪些通道需要向后传递。

常见的使用颜色混合的方式是实现颜色混合，即希望用新颜色基于透明度混合旧颜色。`finalColor` 计算如下：
```psecode
finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
finalColor.a = newAlpha.a;
```

基于之前的结构体设置可以实现上述要求：
```cpp
colorBlendAttachment.setBlendEnable(true)
    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
    .setColorBlendOp(vk::BlendOp::eAdd)
    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
    .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
    .setAlphaBlendOp(vk::BlendOp::eAdd);
```

第二个结构引用所有帧缓冲的结构数组，并允许设置混合常量，可以用作混合因子：
```cpp
vk::PipelineColorBlendStateCreateInfo colorBlending{};
colorBlending.setLogicOpEnable(false)
.setLogicOp(vk::LogicOp::eCopy)     // Optional
.setAttachments(colorBlendAttachment)
.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f }); // Optional
```
如果想用混合的第二种方法（按位组合），那么应将`logicOpEnable` 设置为 `true`，然后在 `logicOp` 中指定按位操作。这将自动禁用第一个方法。`colorWriteMask` 能在此模式下使用，以确定帧缓冲中的哪些通道将受影响。也可以禁用这两种模式，在这种情况下，片段颜色将原封不动地写入帧缓冲。

### 动态阶段（Dynamic state）

前面的结构体中指定的有限数量的状态可以在不重新创建管道的情况下更改。例如视口的大小、行宽和混合系数。如果要进行修改，必须填写一个 `vk::PipelineDynamicStateCreateInfo` 结构体：
```cpp
std::vector<vk::DynamicState> dynamicStates {
    vk::DynamicState::eViewport,
    vk::DynamicState::eLineWidth
};
vk::PipelineDynamicStateCreateInfo dynamicState{};
dynamicState.setDynamicStates(dynamicStates);
```
这会导致值的配置被忽略，并要求用户在绘制时指定数据，后面的章节会讨论。如果没有动态状态，这个结构体可以稍后用 `nullptr` 替换。

### 管线布局（Pipeline layout）

可以在着色器中使用 `uniform` 的值，这是一个全局变量，类似于动态状态变量，可以在绘制时改变着色器行为，不必重新创建。它们通常用于将变换矩阵传递给顶点着色器，或者在片段着色器中创建纹理采样器。

这些 `uniform` 变量需要在管线创建时，通过结构体 `vk::PipelineLayout` 指定，添加类成员：
```cpp
vk::PipelineLayout m_PipelineLayout;
```
并在 `createGraphicsPipeline` 函数中创建对象：
```cpp
vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.setSetLayouts({})    // Optional
    .setPushConstantRanges({}); // Optional
m_PipelineLayout = m_Device.createPipelineLayout(pipelineLayoutInfo);
if (!m_PipelineLayout)  throw std::runtime_error("failed to create pipeline layout!");
```

结构体同样指定了 push constants，是另一种传递动态值到着色器的方法。管线布局将会一直沿用到程序结束，所以应该在最后被销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroyPipelineLayout(m_PipelineLayout);
    ...
}
```

### 总结

上述为固定功能的阶段。这里需要做很多工作，好在现在对图形管线中正在发生的一切有了大致了解，从而降低了发生意外行为的可能，因为某些组件的默认状态与期望不符。在最终创建图形管线之前，还需要创建渲染通道（render pass）。