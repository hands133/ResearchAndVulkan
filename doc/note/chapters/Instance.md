# Vulkan Tutorial (C++ Ver.)

## Vulkan `vk::Instance`

### 创建一个 `vk::Instance`

一开始要通过创建 `vk::Instance` 来初始化 Vulkan 库。`vk::Instance` 是连接程序和 Vulkan 库的桥梁，创建 `vk::Instance` 需要指定一些关于程序的详细信息。

在 `initVulkan` 中添加 `createInstance`：
```cpp
void initVulkan() {
    createInstance();
}
```

向类中添加一个 `vk::Instance` 对象：
```cpp
private:
    vk::Instance m_Instance;
```

### 结构体 `vk::ApplicationInfo`

创建 `vk::Instance` 需要准备一些信息，这些信息需要填入一个结构体当中。此结构体是可选的，但是可以帮助驱动优化程序（原文说 *because it uses a well-known graphics engine with certain special behavior*，暂时不动什么意思）。结构体叫做 `vk::ApplicationInfo`：
```cpp
void createInstance() {
    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("Hello Triangle")
        .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
        .setPEngineName("No Engine")
        .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
        .setApiVersion(VK_API_VERSION_1_0);
}
```
`setXXX` 函数可以返回结构体本身的引用，完成链式调用的功能。基于 C 的编程语言可以直接访问成员并赋值，但是要注意，这样做的首要步骤是指定结构体中 `sType` 的值，必须与结构体类型对应：
```cpp
void createInstance() {
    VKApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appINfo.apiVersion = VK_API_VERSION_1_0;
}
```
> 这样写也太麻烦了，以后就不这么写，用 C++ 的写法了。

这也是所有具有 `pNext` 成员的结构体中能够在未来指定扩展信息的一个。这里不调用 `.setPNext(const void* pNext)`，默认为 `nullptr`。

### 结构体 `vk::InstanceCreateInfo`

要准备的第二个结构体 `vk::InstanceCreateInfo` 是必须的，此结构体用于通知 Vulkan 驱动程序需要的**全局**扩展和验证层（validation layer）。这里**全局**是指应用于程序整体，而不是特定设备。
```cpp
vk::InstanceCreateInfo createInfo;
createInfo.setPApplicationInfo(&appInfo);
```

注意 Vulkan 是 agnostic API（不可知的 API？什么东西），需要特定的扩展来对接窗口系统。GLFW 有内置函数返回特定的扩展，需要将扩展传递给 `vk::InstanceCreateInfo`：
```cpp
uint32_t glfwExtensionCount = 0;
const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

createInfo.setPEnabledExtensionNames(extensions);
```
> 扩展都是以 const char* 存储的字符串。

这里暂时不考虑验证层，设置验证层数为 0（不设置也可以，默认为 0）：
```cpp
createInfo.setEnabledLayerCount(0);
```

使用结构体创建 `vk::Instance` 对象：
```cpp
m_Instance = vk::createInstance(createInfo);
```
### 总结
这里总结一下创建 Vulkan 对象的流程：
+ 创建具有创建信息的结构体
+ 指定分配空间的分配器的回调函数，通常是 `nullptr`
+ 获得从创建函数返回的对象

创建 `vk::Instance` 失败会抛出异常。如果是 C API，则函数会返回表示创建成功/失败的 `VK_SUCCESS`/`VK_ERROR`。

## 检查支持的扩展

`vk::CreateInstance` 文档中涉及到了 `VK_ERROR_EXTENSION_NOT_PRESENT` 错误，我们可以处理这种错误。对于重要如窗口系统接口的扩展无可厚非，但是对于可选的扩展，则需要做单独的处理。

使用 `vk::enumerateInstanceLayerProperties` 可以在创建 `vk::Instance` 获得所有支持的扩展，返回 `vk::ExtensionProperty` 的数组（类比 `std::vector`）。原文说有接口可以对扩展做过滤，但是没看见。
```cpp
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
```

列表中每个 `vk::LayerProperties` 存储着扩展名和版本。输出看看：
```cpp
std::cout << "available extensions:\n";

for (const auto& layer : availableLayers)
    std::cout << '\t' << layer.layerName << '\n';
```

## 清理并回收

`vk::Instance` 应该只在程序退出前销毁：
```cpp
void HelloTriangleApplication::cleanUp() {
    m_Instance.destroy();

    glfwDestroyWindow(m_pWIndow);

    glfwTerminate();
}
```

`vk::Instance::destroy()` 有一个可选参数，即负责分配和回收的回调函数。之后所有创建的 Vulkan 对象，都应在 `vk::Instance` 之前销毁。