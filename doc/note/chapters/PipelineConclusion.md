# Vulkan Tutorial (C++ Ver.)

## 总结

现在可以结合前面的所有结构体和对象创建图形管线，做一下回顾：
+ 着色器阶段（shader stage）：着色器模块定义图形管线中可编程阶段的函数功能
+ 固定功能阶段：定义了所有管线中固定功能部分的结构体，比如输入装配、光栅化、视窗和颜色混合
+ 管线布局：可以在绘制时更新的、着色器对 uniform 和 push values 的引用
+ 渲染通道：由管线各阶段引用的附件和他们的用法

上述结构结合起来就定义了图形管线的功能，在 `createGraphicsPipeline` 函数的末尾添加 `VkGraphicsPipelineCreateInfo` 结构。但是在调用`vkDestroyShaderModule` 之前，因为这些仍然在管线创建过程中使用。

```cpp
vk::GraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.setStages(shaderStagesInfo);
```

填充结构体：
```cpp
pipelineInfo.setPVertexInputState(&vertexInputInfo)
    .setPInputAssemblyState(&inputAssembly)
    .setPViewportState(&viewportState)
    .setPRasterizationState(&rasterizer)
    .setPMultisampleState(&multisampling)
    .setPDepthStencilState(nullptr)     // Optional
    .setPColorBlendState(&colorBlending)
    .setPDynamicState(nullptr);     // Optional
```

然后引用在固定函数阶段创建的结构体：
```cpp
pipelineInfo.setLayout(m_PipelineLayout);
```

之后是管线布局，一个 Vulkan 句柄：
```cpp
pipelineInfo.setRenderPass(m_RenderPass)
    .setSubpass(0);
```

最后设置渲染通道的引用和子通道索引，也可以使用其他渲染通道来代替这个特定的实例，但必须与渲染通道兼容。这里描述了兼容性的要求，但这里不会使用该特性。
```cpp
pipelineInfo.setBasePipelineHandle(nullptr) // OPtional
    .setBasePipelineIndex(-1);  // Optional
```

上面两个 `set` 函数是可选的，Vulkan 允许通过派生现有管线来创建新的图形管线。基本思想是，当管道与现有管道有许多共同功能时，建立管线的成本更低，并且在同一父管线之间切换也更快。可以使用 `setBaseppelinehandle` 指定现有管线的句柄，也可以使用 `setBaseppelineindex` 引用另一个根据下标创建的管线。现在只有一个管线，这里只指定空句柄和无效索引。这些值仅在调用了
```cpp
pipelineInfo.setFlags(vk::PipelineCreateFlagBits::eDerivative)
```
时使用。

创建管线的最后一步，添加类成员
```cpp
vk::Pipeline m_GraphicsPipeline;
```
并创建渲染管线：
```cpp
auto pipeline  = m_Device.createGraphicsPipeline(nullptr, pipelineInfo);
if (pipeline.result != vk::Result::eSuccess)
    throw std::runtime_error("failed to create graphics pipeline!");
m_GraphicsPipeline = pipeline.value;
```

`createGraphicsPipeline` 函数比 Vulkan 中通常的对象创建函数有更多的参数，旨在一次调用中获取多个 `vk::GraphicsPipelineCreateInfo` 对象并创建多个 `vk::Pipeline` 对象。其中的 `vk::PipelineCache` 设置为 `nullptr`，可用于存储和重用在多重调用 `createGraphicsPipeline` 创建管线的相关数据，存储在文件中可以多跨程序执行。后面会涉及到。

所有常见的绘制操作都需要图形管线，它应该只在程序结束时销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroyPipeline(m_GraphicsPipeline);
    m_Device.destroyPipelineLayout(m_PipelineLayout);
    ...
}
```