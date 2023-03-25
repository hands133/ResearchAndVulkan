# Vulkan Tutorial (C++ Ver.)

## 窗口表面（Window surface）

Vulkan 是非感知的 API，不能直接与窗口系统交互。为了与窗口系统连接并显示结果，我们需要使用 WSI（Window System Integration）扩展。第一个要讨论的扩展就是 `VK_KHR_surface`，它暴露了 `vk::SurfaceKHR` 对象，该对象表示要呈现渲染图像的表面的抽象类型。程序的表面将被已经用 GLFW 打开的窗口支持。

`VK_KHR_surface` 扩展是一个实例级扩展，实际上已经启用了，它包含在 `glfwGetRequiredInstanceExtensions` 返回的列表中。该列表还包括一些将用到的其他 WSI 扩展。

在实例创建之后立即创建窗口表面，它会影响物理设备的选择。Vulkan 中的窗口表面是可选的，比如离屏渲染就不需要不需要像创建一个隐形窗口（对于 OpenGL 是必需的）。

### 创建窗口表面（WIndow surface creation）

添加 `vk::SurfaceKHR` 类成员：
```cpp
vk::SurfaceKHR m_Surface;
```
`vk::SurfaceKHR` 对象及其用法是非平台感知的，它的创建过程并不是，因为它以来窗口系统的信息。例如在 Windows 系统下它需要 `HWND` 和 `HMODULE` 句柄。所以扩展需要额外的平台相关信息，Windows 系统下的扩展叫做 `VK_KHR_win32_surface`，自动在 `glfwGetRequiredInstanceExtensions` 返回的列表中引用。

简单展示一下怎样在 Windows 下使用确定的平台相关扩展（本文不会用到）。GLFW 中的 `glfwCreateWindowSurface` 函数随平台不同而变化。为了访问本地平台函数，需要在头部引入
```cpp
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
```

窗口表面是 Vulkan 对象，它带有 `VkWin32SurfaceCreateInfoKHR` 结构体。它有两个重要参数 `hwnd` 和 `hinstance`，是窗口和进程的句柄。
```cpp
VkWin32SurfaceCreateInfoKHR createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
createInfo.hwnd = glfwGetWin32Window(window);
createInfo.hinstance = GetModuleHandle(nullptr);
```
`glfwGetWin32Window` 函数用于从 `GLFW` 窗口对象获得原始 `HWND`，`GetModuleHandle` 调用返回了当前进程的 `HINSTANCE` 句柄。

之后用 `vkCreateWin32SurfaceKHR` 创建表面，其中包括实例的参数、表面创建细节、自定义分配器和用于存储表面句柄的变量。这是一个 WSI 扩展函数，标准的 Vulkan 加载器都包含了它，所以与其他扩展不同，用户不需要显式加载。
```cpp
if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
}
```

过程类似其他平台，如 Linux，`vkCreateXcbSurfaceKHR` 将 XCB 连接和窗口作为 X11 的创建细节。

`glfwCreateWindowSurface` 函数对每个平台用不同的实现执行操作，这里对其进行整合。添加函数 `createSurface`，并在 `initVulkan` 中的 `setupDebugMessenger` 之后调用：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void HelloTriangleApplication::createSurface() {}
```
然后 GLFW 调用接受 `VkSurfaceKHR` 而不是 `vk::SurfaceKHR`：
```cpp
void HelloTriangleApplication::createSurface()
{
    VkSurfaceKHR tmpSurface;
    VkResult result = glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &tmpSurface);
    if (result != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
    m_Surface = tmpSurface;
}
```

参数类型为 `vk::Instance`，GLFW 窗口指针，定制分配器和指向 `VkSurfaceKHR` 的指针，返回 `VkResult`。在 `vk::Instance` 前销毁 `vk::SurfaceKHR`：
```cpp

void HelloTriangleApplication::cleanUp() {
    ...
    m_Instance.destroySurfaceKHR(m_Surface);
    m_Instance.destroy();
    ...
}
```

### 查询对呈现的支持

尽管 Vulkan 的实现可能集成了窗口系统，也不意味着系统的所有设备都支持。这里需要扩展 `isDeviceSuitable`，保证设备可以将图片呈现到我们创建的表面。呈现是队列相关的特性，问题变为查找支持呈现到表面的队列族。

支持绘制命令的队列族和支持呈现的队列族可能不重叠，须通过修改 `QueueFamilyIndices` 并考虑可能存在不同的表示队列：
```cpp
struct QueueFamilyIndices{ 
    std::optional<uint32_t> graphicsFamily; 
    std::optional<uint32_t> presentFamily;

    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }    
};
```

然后修改 `findQueueFamilies` 函数查找支持呈现到表面的能力的队列族，用 `vk::PhysicalDevice` 的 `getSurfaceSupportKHR` 判断设备是否支持 `vk::SurfaceKHR`：
```cpp
HelloTriangleApplication::QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device)
{
    ...
    for (const auto& queueFamily : queueFamilyProperties)
    {
        ...
        auto presentSupport = device.getSurfaceSupportKHR(i, m_Surface);
        if (presentSupport) indices.presentFamily = i;
        ...
    }
    ...
}
```

这些队列可能是同一个队列族，但这里将把它们视为单独的队列。可以添加逻辑显式选择支持在同一队列中绘制和呈现的物理设备，以提高性能。

### 创建呈现队列

修改逻辑设备的创建过程，从而创建呈现队列并取回 `vk::Queue` 句柄。添加成员变量：
```cpp
vk::Queue m_PresentQueue;
```

然后为了处理多个 `vk::DeviceQueueCreateInfo` 结构体并从队列族创建队列，这里创建队列所必需的所有唯一队列族的集合：
```cpp
#include <set>
...
void HelloTriangleApplication::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.setQueueFamilyIndex(queueFamily)
        .setQueueCount(1)
        .setQueuePriorities(queuePriority);
        queueCreateInfos.push_back(queueCreateInfo);
    }
    ...
    vk::DeviceCreateInfo createInfo{};
    createInfo.setQueueCreateInfos(queueCreateInfos)
    .setPEnabledFeatures(&deviceFeatures)
    .setEnabledExtensionCount(0);
    ...
    m_GraphicsQueue = m_Device.getQueue(indices.graphicsFamily.value(), 0);
    m_PresentQueue = m_Device.getQueue(indices.presentFamily.value(), 0);
}
```
如果队列族相同，这里只需要传递一次下标。队列族相同时，两个句柄可能有相同的值。