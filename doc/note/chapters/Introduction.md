# Vulkan Tutorial (C++ Ver.)

## Introduction（渲染管线）

在接下来的几章中，将设置一个图形管线，用于绘制一个三角形。图形管线是将网格的顶点和纹理一直到渲染目标中的像素的操作序列。图就不放了，到处都是。

管线分为几个阶段：
+ **input assembler**：输入装配，收集来自用户指定缓冲区的原始顶点数据，可能用到指标缓冲区以重复利用固定元素，而不用重复顶点数据本身。
+ **vertex shader**：顶点着色器，对每个顶点运行，通常在这里应用变换，把顶点位置从模型空间变换到屏幕空间。它也逐顶点向下传递给管线。
+ **tessellation shaders**：细分着色器，基于特定规则细分几何以提升网格质量。常用于使类似砖块墙、楼梯等表面在近处看起来更平整。
+ **geometry shader**：几何着色器，运行在每个图元（三角形，线，点）上，可以抛弃或者输出比输入更多的图元。它与细分着色器类似，但是更灵活。它在现在的程序中不常见，因为在除了 Intel 集成显卡外的其他显卡上性能不太好。
+ **rasterization**：光栅化，将基础图元离散为片元（fragments），即填充帧缓冲定的像素单元。任何位于窗口之外的片元会被丢弃，来自顶点着色器的属性会在片元内插值。通常基于深度测试，位于其他图元片元之后的片元会被丢弃。
+ **fragment shader**：片元着色器，运行在剩下的片元上，并决定片元要以什么颜色、深度值写入哪些帧缓冲。片元着色器通过对顶点着色器的数据插值来做这些事情，数据包括纹理坐标和光照法向等。
+ **color blending**：颜色混合，混合映射到帧缓冲同一像素的不同片元。片元之间可以根据透明度直接覆写，相加或混合。
> + input assembler、rasterization 和 color blending 阶段叫做固定功能（fixed-function）阶段，这些阶段允许通过参数调整设置，但是功能是预先定义好的。
> + vertex shader、tessellation、geometry shader 和 fragment shader 是可编程的（programmable），用户可以上传自己的代码到图形卡，并准确应用用户想要的操作。比如用户可以使用片元着色器实现从纹理采样、光照到光线追踪的各种功能。这种在 GPU 多核运行的程序同时处理许多对象，如并行处理顶点和片元等。

对于像 OpenGL 和 Direct3D 这样的旧 API，可以调用 glBlendFunc 和 OMSetBlendState 随意更改管线设置。Vulkan 中的图形管线几乎不可变，所以**如果要改变着色器，绑定不同的帧缓冲或改变混合方式，必须从头创建管道**。缺点是必须创建许多想在呈现操作中使用的所有状态组合的管线。优点是由于管线中要执行的所有操作都预知，因此驱动可以更好地优化程序。

有些可编程阶段是可选的，如果只是绘制简单的几何图形，则可以禁用细分着色器和几何着色器阶段。如果要用深度值，那么可以禁用片段着色器阶段，这对于阴影映射生成很有用。

在下一章中，将首先创建将三角形放到屏幕上所需的两个可编程阶段：顶点着色器和片元着色器。固定功能的配置，如混合模式，视窗，光栅化在后面设置。在 Vulkan 中设置图形管线的最后一部分涉及到输入和输出帧缓冲的规范。

添加 `createGraphicsPipeline` 函数，在 `initVulkan` 中的 `createImageViews` 之后调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
}
...
void HelloTriangleApplication::createGraphicsPipeline() {}
```