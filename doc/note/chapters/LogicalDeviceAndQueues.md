# Vulkan Tutorial (C++ Ver.)

## Vulkan `vk::Device` 和 `vk::Queue`

### 介绍

选择了 `vk::PhysicalDevice` 后还需要一个**逻辑设备**（logical device）与之交互。逻辑设备的创建过程和 `vk::Instance` 的创建过程类似，它描述了我们希望使用的特征。还需要指定要创建哪些队列。甚至可以从同一个物理设备创建多个逻辑设备。

添加一个成员变量存储逻辑设备：
```cpp
vk::Device device;
```

在 `initVulkan` 中添加创建逻辑设备的成员函数 `createLogicalDevice`：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
}

void HelloTriangleApplication::createLogicalDevice() {}
```

### 确定需要创建的队列

逻辑设备的创建需要给结构体 `vk::DeviceQueueCreateInfo` 提供不少信息，它包含了我们想要从单个队列族中创建的队列数量。当前只关心具有图形功能的队列：
```cpp
void HelloTriangleApplication::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);

    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.setQueueFamilyIndex(indices.graphicsFamily.value()).setQueueCount(1);
}
```
当前可用的驱动只允许对每个队列族创建少量的队列，但是咱不需要那么多。因为多线程下可以创建多个命令缓冲区，并在主线程一次性提交，减少了调用开销。

Vulkan 允许我们给队列设置优先级，使用 (0.0, 1.0] 之间的浮点数影响命令缓冲区执行的调度。只有一个队列也得设置：
```cpp
void HelloTriangleApplication::createLogicalDevice()
{
    ...
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.setQueueFamilyIndex(indices.graphicsFamily.value())
    .setQueueCount(1)
    .setQueuePriorities(queuePriority);
}
```

### 确定用到的设备特性

接下来指定设备特性，即之前中用 `getFeatures` 查询支持的特性。这会儿不需要什么特性，直接 `VK_FALSE`，后面还会回来的。
```cpp
vk::PhysicalDeviceFeatures deviceFeatures{};
```

### 创建逻辑设备

接下来准备 `vk::DeviceCreateInfo` 结构体：
```cpp
vk::DeviceCreateInfo createInfo{};
```
然后传递存储创建队列和设备特性的结构体指针：
```cpp
createInfo.setQueueCreateInfos(queueCreateInfo)
    .setPEnabledFeatures(&deviceFeatures);
```

后面和 `vk::InstanceCreateInfo` 一样，需要确定扩展和验证层。比方说设备扩展需要 `VK_KHR_swapchain`，允许从设备渲染图像到窗口。具体后面会说。

以前的 Vulkan 区分了实例和设备特定的验证层，现在不是了。`Vk::DeviceCreateInfo` 的 `enabledLayerCount` 和`ppEnabledLayerNames` 字段被忽略。倒是可以为了兼容性加上：
```cpp
vk::DeviceCreateInfo createInfo{};
    createInfo.setQueueCreateInfos(queueCreateInfo)
    .setPEnabledFeatures(&deviceFeatures)
    .setEnabledExtensionCount(0);

if (m_EnableValidationLayers)
    createInfo.setPEnabledLayerNames(m_VecValidationLayers);
```

现在还不需要特定扩展。通过 `vk::PhysicalDevice` 的 `createDevice` 函数创建 `vk::Device`：
```cpp
m_Device = m_PhysicalDevice.createDevice(createInfo);
if(!m_Device)
    throw std::runtime_error("failed to create logical device!");
```

参数包括要连接的物理设备、刚指定的队列和使用信息、可选的分配回调函数指针和指向存储逻辑设备句柄的变量的指针。类似创建  `vk::Instance` 的函数，当指定不存在的扩展或指定不支持的特性的预期用法时会返回错误。

程序退出前，在 `vk::PhysicalDevice` 销毁前销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Device.destroy();
    ...
}
```
> 逻辑设备不直接与 `vk::Instance` 交互，所以没有什么参数。

### 获取队列句柄

队列是随着逻辑设备的创建一同创建的，但是我们没有能与之对接的句柄。添加成员：
```cpp
vk::Queue m_GraphicsQueue;
```
设备队列会在 `vk::Device` 销毁时一同销毁，不需要额外清理。

使用 `vk::Device::getQueue` 从 `vk::Device` 中队列族获取队列句柄：
```cpp
m_GraphicsQueue = m_Device.getQueue(indices.graphicsFamily.value(), 0);
```
第二个参数为队列族，第三个参数为队列下标。这里只用到了队列族中的一个队列，所以队列下标为 0。