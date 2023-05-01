# Vulkan Tutorial (C++ Ver.)

## Vulkan `vk::PhysicalDevice` 和

### 选择物理设备

创建并初始化 `vk::Instance` 后，我们需要选择系统中支持所需特性的图形卡。这里选择最合适的图形卡。在 `initVulkan` 中添加函数 `pickPhysicalDevice` 函数：
```cpp
void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
}

void HelloTriangleApplication::pickPhysicalDevice() {}
```

选择的设备存储在成员变量 `vk::PhysicalDevice` 中：
```cpp
vk::PhysicalDevice m_PhysicalDevice;
```
> 注意 `vk::PhysicalDevice` 会在 `vk::Instance` 销毁前隐式销毁，所以不需要在 `cleanUP` 中显式销毁 `vk::PhysicalDevice`。

从 `vk::Instance` 获取可用的设备列表，并做检查：
```cpp
auto devices = m_Instance.enumeratePhysicalDevices();
if (devices.size() == 0)
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
```

此外检查`devices` 中每个设备是否合适，创建函数 `isDeviceSuitable`：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    return true;
}
```

具体内容后面再填。修改 `pickPhysicalDevice`：
```cpp

void HelloTriangleApplication::pickPhysicalDevice() {
    auto devices = m_Instance.enumeratePhysicalDevices();
    if (devices.size() == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
        if (!m_PhysicalDevice)
            throw std::runtime_error("failed to find a suitable GPU!");
    }
}
```

### 基本的设备检查

为了聘雇设备是否合适，这里要查询一些信息，如名字、类型和支持的 Vulkan 版本号等等，可以通过 `vk::PhysicalDevice` 的 `getProperties` 获得：
```cpp
auto deviceProperties = device.getProperties();
```
此外，可以用 `getFeatures` 获得支持的可选特性，如纹理压缩，64 位浮点数精度和多视角渲染（如 VR）等：
```cpp
auto deviceFeatures = device.getFeatures();
```

例如我们想检查设备是否支持计算着色器：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    auto deviceProperties = device.getProperties();
    auto deviceFeatures = device.getFeatures();

    return deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader;
}
```
给所有的设备排个序是不更好，给个打分，选择最合适的那个：
```cpp

void HelloTriangleApplication::pickPhysicalDevice() {
    ...
    std::multimap<int, vk::PhysicalDevice> candidates;

    for (const auto &device : devices) {
        int score = rateDeviceSuitability(device);
        candidates.insert({ score, device });
    }

    if (candidates.rbegin()->first > 0)
        m_PhysicalDevice = candidates.rbegin()->second;
    else
        throw std::runtime_error("failed to find a suitable GPU!");
}


int HelloTriangleApplication::rateDeviceSuitability(vk::PhysicalDevice device) {
    int score = 0;

    auto deviceProperties = device.getProperties();
    auto deviceFeatures = device.getFeatures();

    if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        score += 1000;

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    if (!deviceFeatures.geometryShader) return 0;

    return score;
}
```
> 目前为止我们只需要支持 `Vulkan` 的 GPU 设备，所以 `isDeviceSuitable` 函数就先这样：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    return true;
}
```

### 队列族（Queue Families）

Vulkan 中的几乎每个操作，从绘制到上传纹理，都需要向队列提交命令。Vulkan 里有集中不同的队列，来自于不同的**队列族**（queue families），每个族只允许所有命令中的部分子集。例如只允许处理计算任务的队列，或者只允许传输内存的命令等等。

首先检查设备支持的队列族，挑选出我们想要提交命令的队列。新建 `findQueueFamilies` 函数查找队列族，目前只要求支持图形命令：
```cpp
uint32_t HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device) {}
```

不过后面还需要别的队列，最好是能把下标存在结构体里：
```cpp
struct QueueFamilyIndices{ uint32_t graphicsFamily; };

HelloTriangleApplication::QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;
    // 查找队列族的逻辑
    return indices;
}
```

如果队列族不可用，可以在 `findQueueFamilies` 中抛出异常，但是这个函数不是决定设备适定性的正确地方。这里需要一个表示确切的队列族是否被找到的方式：
```cpp
struct QueueFamilyIndices{ std::optional<uint32_t> graphicsFamily; };
HelloTriangleApplication::QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;
    // 查找队列族的逻辑
    return indices;
}
```

然后和获取 `vk::PhysicalDevice` 的属性一样：
```cpp
HelloTriangleApplication::QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;
    auto queueFamilyProperties = device.getQueueFamilyProperties();

    return indices;
}
```
函数 `getQueueFamilyProperties` 返回的是包含队列族属性的 `std::vector<vk::QueueFamilyProperties>` 类型。`vk::QueueFamilyProperties` 存储着关于队列族的信息，包括支持的操作和可基于族创建的队列数量。这里需要至少一个支持 `vk::QueueFlagBits::eGraphics` 的队列族：
```cpp
int i = 0;
for (const auto& queueFamily : queueFamilyProperties)
{
    if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        indices.graphicsFamily = i;
    ++i;
}
```
有了队列族的查找函数，就可以用它在 `isDeviceSuitable` 函数中作检查，保证设备可以处理我们想用的命令：
```cpp
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    return indices.graphicsFamily.has_value();
}
```

修改一下 `QueueFamilyIndices`：
```cpp
struct QueueFamilyIndices{ 
    std::optional<uint32_t> graphicsFamily; 
    bool isComplete() { graphicsFamily.has_value(); }    
};
    ...
bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    return indices.isComplete();
}
```

在 `findQueueFamilies` 中用于提前终止搜索：
```cpp
int i = 0;
for (const auto& queueFamily : queueFamilyProperties)
{
    if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        indices.graphicsFamily = i;
    
    if (indices.isComplete())   break;
    ++i;
}
```

最后记得改：
```cpp
void HelloTriangleApplication::pickPhysicalDevice() {
    ...
    for (const auto &device : devices) {
        if (isDeviceSuitable(device))
        {
            int score = rateDeviceSuitability(device);
            candidates.insert({ score, device });
        }
    }
    ...
}
```