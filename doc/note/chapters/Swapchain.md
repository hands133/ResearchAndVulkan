# Vulkan Tutorial (C++ Ver.)

## 交换链（Swap chain）

Vulkan 没有“默认帧缓冲”（default framebuffer）的概念，它需要一个基础设施在呈现到屏幕上之前准备缓冲区，即**交换链**，它必须显式创建。交换链本质是一个等待显示在屏幕上的图像队列。程序将获取图像并绘制，然后提交到队列。队列的工作方式以及从队列中显示图像的条件取决于交换链的设置方式，但交换链一般是同步图像的显示与屏幕的刷新率。

### 检查交换链支持

并非所有的显卡都能够将图像直接显示到屏幕（服务器等）。而且，由于图像呈现与窗口系统和与窗口相关的表面密切相关，因此它不是 Vulkan 核心的一部分。需要查询一下设备对 `VK_KHR_swapchain` 的支持。

首先扩展 `isDeviceSUitable` 函数检查扩展。添加类成员：
```cpp
const std::vector<const char*> m_vecDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
```

添加函数 `checkDeviceExtensionSupport`
```cpp
bool HelloTriangleApplication::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    return true;
}
```

并修改 ``isDeviceSuitable` 函数：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    return indices.isComplete() && extensionsSupported;
}
```

枚举设备的扩展并修改函数体，检查需要的扩展：
```cpp
bool HelloTriangleApplication::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    auto deviceExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(m_vecDeviceExtensions.begin(), m_vecDeviceExtensions.end());

    for (const auto& extension : deviceExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();    
}
```

这里用一组字符串表示所需的扩展，列举扩展时可以将其划掉。呈现队列的可用性意味着必须支持交换链扩展。


### 开启设备扩展

在 `createLogicalDevice` 函数中创建逻辑设备的结构体，以支持 `VK_KHR_swapchain`：
```cpp
vk::DeviceCreateInfo createInfo{};
    createInfo.setQueueCreateInfos(queueCreateInfos)
    .setPEnabledFeatures(&deviceFeatures)
    .setPEnabledExtensionNames(m_vecDeviceExtensions);
```    
代替 `.setEnabledExtensionCount(0)`。

### 查询交换链支持的细节

只查询对交换链链的支持是不够的，因为它可能与窗口表面不兼容。创建交换链所做的工作也比创建 `vk::Instance` 和 `vk::Device` 多。要检查以下几点：
+ 基本的表面能力（交换链中的最大/最小图像数量，图像的最大/最小宽、高和厚度）
+ 表面格式（像素格式，颜色空间）
+ 可用的呈现模式

类似 `findQueueFamilies`，使用结构体传递要查询的信息，此结构体包含上述提到的三点信息：
```cpp
struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};
```

添加函数 `querySwapChainSupport` 处理此结构体：
```cpp
HelloTriangleApplication::SwapChainSupportDetails
HelloTriangleApplication::querySwapChainSupport(vk::PhysicalDevice device)
{
    SwapChainSupportDetails details;

    return details;
}
```

基本的表面能力很容易查询：
```cpp
details.capabilities = device.getSurfaceCapabilitiesKHR(m_Surface);
```

然后查询支持的表面格式：
```cpp
details.formats = device.getSurfaceFormatsKHR(m_Surface);
```

最后查询支持的呈现模式：
```cpp
details.presentModes = device.getSurfacePresentModesKHR(m_Surface);
```

有了存储信息的结构体，我们扩展 `isDeviceSuitable` 并验证交换链的支持是否足够。这个教程中，如果给定窗口表面，至少有一种支持的图像格式和一种支持的表示模式，那么交换链支持就足够了。
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}
```

这里只在验证扩展可用后才尝试查询交换链支持。

### 选择交换链正确的设置

如果 `swapChainAdequate` 条件满足，那就足够了，但仍然可能有许多不同的最优变化模式。写几个函数为交换链找到可能的最佳设置。有三种类型的设置需要确定:
+ 表面格式（色深）
+ 呈现模式（将图像“交换”到屏幕的条件）
+ 交换上拿下问（交换链中的图像分辨率）
对于每一个都有一个理想值，可用就用，不能用就下一个。

#### 表面格式

添加函数 `chooseSwapSurfaceFormat`，之后将结构体 `SwapChainSupportDetails` 中的 `formats` 传递给此函数：
```cpp
vk::SurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {}
```

每个 `vk::SurfaceFormatKHR` 包含 `format` 字段和 `colorSpace` 字段。`format` 成员指定了色彩通道和类型。例如 `vk::Format::eB8G8R8A8Srgb` 指按照蓝、绿、红和 alpha 通道的顺序，每个通道由 8 bits 表示，每个像素由 32 bits 表示。`colorSpace` 成员表示 SRGB 颜色空间是否支持，由 `vk::ColorSpaceKHR::eSrgbNonlinear` 决定。

这里颜色空间用 SRGB，它直观上表示的颜色更准确（？），也是图像更标准的颜色空间，比如之后要用到的纹理等。同样，格式采用 SRGB，最常用的是 `vk::Format::eB8G8R8A8Srgb`：
```cpp
for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
        && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return availableFormat;
}
```

如果没有匹配的，就返回列表第一个：
```cpp
vk::SurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
            && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return availableFormat;
    }
    return availableFormats.front();
}
```

#### 呈现模式

交换链的呈现模式中最重要的设置，它表示了在屏幕上显示图像的条件。Vulkan 中有四种可能的模式：
+ `vk::PresentModeKHR::eImmediate`：图像通过程序立即传输到屏幕，可能导致画面撕裂。
+ `vk::PresentModeKHR::eFifo`：交换链是一个队列，当刷新显示时，显示器从队头获取图像，程序将需要呈现的图像插入到队尾。如果队列已满，程序必须等待。这与垂直同步类似。显示刷新的时刻被称为“垂直空白”（vertical blank）。
+ `vk::PresentModeKHR::eFifoRelaxed`：此模式与上一模式不同之处在于程序延迟导致队列在最后垂直空白为空队列。到达的图像将会立即传输而不是等待下一个垂直空白。这可能导致可见的撕裂。
+ `vk::PresentModeKHR::eMailbox`：是第二个模式的变种。当队列满时，队列中的图像会被新图像替换，而不是阻塞程序。这种模式用于尽可能快地渲染帧，同时避免撕裂，比标准垂直同步的延迟问题更少。它也被称为“三重缓冲”，尽管单独存在三个缓冲区并意味着帧速率被解锁。

只有 `vk::PresentModeKHR::eFifo` 是保证支持的，所以这里用另外一个函数查找最合适的呈现模式：
```cpp
vk::PresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& abailablePresentModes)
{
    return vk::PresentModeKHR::eFifo;
}
```

作者认为 `vk::PresentModeKHR::eMailbox` 是很好的 trade-off。既避免了画面的撕裂，同时保持相当低的延迟，渲染尽可能最新图像直到垂直空白。在功耗限制更严格的移动设备上，使用 `vk::PresentModeKHR::eMailbox` 可能更好。这里看看 `vk::PresentModeKHR::eMailbox` 是不是可用：
```cpp
vk::PresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
            return availablePresentMode;

    return vk::PresentModeKHR::eFifo;
}
```

### 交换上下文

只剩下一个主要属性，为它添加最后一个属性：
```cpp
vk::Extent2D HelloTriangleApplication::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {}
```

交换范围是交换链图像的分辨率，几乎完全等于绘制的窗口的分辨率（以像素为单位）。可能的范围在 `vk::SurfaceCapabilitiesKHR` 结构中定义。通过在`currentExtent` 成员中设置宽度和高度匹配窗口的分辨率。然而某些窗口管理器允许不同的操作，通过设置 `currentExtent` 中的宽度和高度为特殊值：`uint32_t` 的最大值来表示的。此时将在 `minImageExtent` 和 `maxImageExtent` 范围内选择最匹配窗口的分辨率。这里必须指定正确的分辨率。

GLFW 使用像素和屏幕坐标度量尺寸。例如之前创建窗口时使用指定的分辨率 `{WIDTH, HEIGHT}` 是以屏幕坐标测量的。但是Vulkan 使用像素，因此交换链范围必须以像素为单位指定。如果使用 DPI 显示器（苹果的 Retina 显示器等），屏幕坐标不对应于像素。更高的像素密度会导致以像素为单位的窗口的分辨率大于屏幕坐标的分辨率。如果 Vulkan 不修复交换范围，我们就不能只使用原来的 `{WIDTH, HEIGHT}` 而必须使用`glfwGetFramebufferSize` 在匹配最小和最大图像范围之前以像素为单位查询窗口的分辨率。
```cpp
#include <limits>
#include <algorithm>

...

vk::Extent2D HelloTriangleApplication::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;
    else 
    {
        int width, height;
        glfwGetFramebufferSize(m_pWindow, &width, &height);

        vk::Extent2D actualExtent(width, height);

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent,height,
            capabilities.maxImageExtent.height);
        
        return actualExtent;
    }
}
```

`clamp` 函数用于将 `width` 和 `height` 的值限制在支持的允许的最小和最大范围。

### 创建交换链

既然有了函数在运行时帮助我们选择，我们可以使用所有需要的信息创建交换链了。添加函数 `createSwapChain` 调用上述函数，并在 `initVulkan` 中创建逻辑设备后调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
}
...
void HelloTriangleApplication::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_PhysicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
}
```
除了这些属性，还必须决定在交换链中有多少图像。指定运行所需的最小数量：
```cpp
uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
```

此外要确保不会超出最大的图片数量限制，0 是表示没有最大值限制的特殊值：
```cpp
if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    imageCount = swapChainSupport.capabilities.maxImageCount;
```

创建交换链需要准备一大堆信息：
```cpp
vk::SwapchainCreateInfoKHR createInfo{};
createInfo.setSurface(m_Surface)
    .setMinImageCount(imageCount)
    .setImageFormat(surfaceFormat.format)
    .setImageColorSpace(surfaceFormat.colorSpace)
    .setImageExtent(extent)
    .setImageArrayLayers(1)
    .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
```

`setImageArrayLayers` 指定每个图片所包含的层数，值总是 1，除非开发 3D 立体程序才会用到（这里应该是三维图片数据吧？）。`setImageUsage` 的参数指定交换链中针对图片的操作。这里直接对图片进行渲染，意味着图片用于颜色附件。当然可以先渲染在独立的图片中以便后处理，这种情况下需要 `vk::ImageUsageFlagBits::eTransferDst` 之类的标志位，并使用内存操作将渲染的图片传输给交换链图片。

向函数中添加：
```cpp
QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);
uint32_t queueFamilyIndices[] = {
    indices.graphicsFamily.value(),
    indices.presentFamily.value() };
if (indices.graphicsFamily != indices.presentFamily)
    createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
    .setQueueFamilyIndices(queueFamilyIndices);
else
    createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
```

上述代码用于在图像队列和呈现队列不同的情况下指定交换链图片指针如何在不同队列族中共享。之后会在图像队列中绘制在交换链中的图像，然后提交到呈现队列。这里有两种针对多队列共享的句柄访问方式：
+ `vk::SharingMode::eConcurrent`：一个图像一次属于一个队列族，在另一个队列族中使用之前，必须显式地转移图像所有权。此选项提供了最佳性能。
+ `vk::SharingMode::eExclusive`：图像可以被多个队列族访问，无需显式转移所有权。

如果队列族不同，使用 `vk::SharingMode::eConcurrent` 模式，以避免对概念的深入解释（后面解释会更好）。此模式要求使用 `setQueueFamilyIndices` 预先指定要共享所有权的队列族。如果图像队列和呈现队列相同，这也是设备上最可能出现的情况，那么，应该用 `vk::SharingMode::eExclusive`，因为 `vk::SharingMode::eConcurrent` 模式需要指定至少两个不同的队列族。

```cpp
createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform);
```
这里可以指定应该应用于交换链中图像的转换（`capabilities` 中的 `supportedTransforms` 表示是否支持此特性），如 90 度顺时针旋转或水平翻转。使用 `currentTransform` 表示只采用当前变换，不作其他变换。

```cpp
createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
```
上述代码指定 alpha 通道是否被用于与其他窗口系统里的窗口作混合（？）。使用 `vk::CompositeAlphaFlagBitsKHR::eOpaque` 从而忽略它。

```cpp
createInfo.setPresentMode(presentMode).setClipped(true);
```

`setPresentMode` 不必多说。如果设定 `clipped` 为 `true`，那么无需关心被遮挡的像素（比如有其他窗口在像素之前）。除非真的需要读取被遮挡的像素并返回期望的值，为了性能也该允许 clipping。

```cpp
createInfo.setOldSwapchain(nullptr);
```
上述代码涉及 `oldSwapChain`。在 Vulkan 中，随着程序的执行，交换链可能无效或者欠优化。例如调整窗口大小等等。这种情况下交换链需要重新创建，这里必须指定指向旧交换链的引用。后面可能会涉及吧（希望）。现在假定这里只需要创建一个交换链。

添加类成员 `vk::SwapchainKHR m_SwapChain` 并创建交换链：
```cpp
m_SwapChain = m_Device.createSwapchainKHR(createInfo);
if (!m_SwapChain)   throw std::runtime_error("failed to create swap chain!");
```

它应该在逻辑设备销毁之前销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroySwapchainKHR(m_SwapChain);
    m_Device.destroy();
    ...
}
```

如果取消设置 `createInfo.setImageExtent(extent)`，验证层会报错：
```bash
validation layerValidation Error: [ VUID-VkSwapchainCreateInfoKHR-imageExtent-01689 ] Object 0: handle = 0x5555562a1410, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x13140d69 | vkCreateSwapchainKHR(): pCreateInfo->imageExtent = (0, 0) which is illegal. The Vulkan spec states: imageExtent members width and height must both be non-zero (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-VkSwapchainCreateInfoKHR-imageExtent-01689)
validation layerValidation Error: [ VUID-VkSwapchainCreateInfoKHR-imageExtent-01274 ] Object 0: handle = 0x5555562a1410, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x7cd0911d | vkCreateSwapchainKHR() called with imageExtent = (0,0), which is outside the bounds returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): currentExtent = (600,400), minImageExtent = (600,400), maxImageExtent = (600,400). The Vulkan spec states: imageExtent must be between minImageExtent and maxImageExtent, inclusive, where minImageExtent and maxImageExtent are members of the VkSurfaceCapabilitiesKHR structure returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR for the surface (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-VkSwapchainCreateInfoKHR-imageExtent-01274)
```

### 取回交换链图像

交换链创建之后，接下来要取回其中的一系列 `vk::Image`。后续渲染时需要他们的句柄。添加类成员：
```cpp
std::vector<vk::Image> m_vecSwapChainImages;
```

图像通过交换链的实现创建，它们会随着交换链的销毁自动回收，因此不需要额外的清空操作。

添加检索句柄的代码到 `createSwapChain` 函数末尾的 `createSwapchainKHR` 调用之后。图像检索与从 Vulkan 检索对象数组的操作相似。之前只在交换链中指定了最小数量的图像，该实现允许创建具有更多图像的交换链。所以首先使用 `vkGetSwapchainImagesKHR` 查询图像数量：
```cpp
m_vecSwapChainImages = m_Device.getSwapchainImagesKHR(m_SwapChain);
```

最后用成员变量存储为交换链图像设定的格式和上下文：
```cpp
vk::SwapchainKHR m_SwapChain;
std::vector<vk::Image> m_vecSwapChainImages;
vk::Format m_SwapChainImageFormat;
vk::Extent2D m_SwapChainExtent;

...

m_SwapChainImageFormat = surfaceFormat.format;
m_SwapChainExtent = extent;
```

现在有了一组可以绘制和呈现到窗口的图像。之后介绍如设定图像作为渲染目标，并开始涉及具体的图形管线和渲染命令。

