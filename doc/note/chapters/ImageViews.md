# Vulkan Tutorial (C++ Ver.)

## 图片视图

为了使用 `vk::Image`，包括在交换链中的，在渲染管线中需要创建 `vk::ImageView` 对象。图像视图字面上是图像的视图，它描述了如何访问图像，访问图像的哪些部分。例如一张图像是否当作 2D 的深度纹理，不包含任何 mipmap 层级。

后续我们添加 `createImageViews` 函数为交换链中每个 `vk::Image` 创建基本的图像视图以便后续用于关于颜色目标对象。

添加类成员：
```cpp
std::vector<vk::ImageView> m_vecSwapChainImageViews;
```

添加 `createImageViews` 函数并在 `initVulkan` 中的交换链创建后调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

void HelloTriangleApplication::createImageViews() {}
```

对于每个交换链图像，创建并填充 `vk::ImageViewCreateInfo`：
```cpp
for (auto& image : m_vecSwapChainImages)
{
    vk::ImageViewCreateInfo createInfo;
    createInfo.setImage(image)
    .setViewType(vk::ImageViewType::e2D)
    .setFormat(m_SwapChainImageFormat);
}
```

`setViewType` 和 `setFormat` 指定解释图像的方式。`setViewType` 允许将图像处理为 1D 纹理、2D 纹理、3D 纹理和立方贴图等，此外：
```cpp
createInfo.setComponents(vk::ComponentMapping(
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity));
```
`setComponents` 允许 swizzle（不会翻译）颜色通道。例如可以将通道映射到红色通道，用于单色纹理。也可以将 0 到 1 之间的常值映射到某一通道。这里采用默认映射。

```cpp
createInfo.setSubresourceRange(vk::ImageSubresourceRange(
    vk::ImageAspectFlagBits::eColor,
    0,
    1,
    0,
    1));
```

`setSubresourceRage` 描述了图像的目的和那些图像会被访问到。构造函数包括五个参数：
```cpp
VULKAN_HPP_CONSTEXPR ImageSubresourceRange( VULKAN_HPP_NAMESPACE::ImageAspectFlags aspectMask_     = {},
                                            uint32_t                               baseMipLevel_   = {},
                                            uint32_t                               levelCount_     = {},
                                            uint32_t                               baseArrayLayer_ = {},
                                            uint32_t                               layerCount_     = {} ) VULKAN_HPP_NOEXCEPT
```
这里指定的是，图像用于颜色目标，且不带 mipmap 和多重层。

如果开发的是立体 3D 应用程序，那么需要创建一个具有多层的交换链。然后可以访问不同的层，并为表示左右眼视图的每张图像创建多个图像视图。

调用 `vk::CreateImateView` 创建图像视图：
```cpp
auto& view = m_vecSwapChainImageViews.emplace_back(m_Device.createImageView(createInfo));
if (!view)  throw std::runtime_error("failed to create image views!");
```

与 `vk::Image` 不同，图像视图是被显式创建的，所以需要显式销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    for (auto& imageView : m_vecSwapChainImageViews)
        m_Device.destroyImageView(imageView);
    ...
}
```

图像视图足够开始使用图像作为纹理，但还没准备好用作渲染目标。这需要**帧缓冲**（framebuffer）。但首先必须设置图形管线。