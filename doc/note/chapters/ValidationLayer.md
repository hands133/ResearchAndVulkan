# Vulkan Tutorial (C++ Ver.)

## Vulkan 验证层（Validation layers）

### 验证层是啥？

Vulkan API 的设计围绕最小化驱动开销的原则，表现之一就是默认情况下 API 的错误检查非常有限。即使是将枚举设置为错误值或将 `nullptr` 指针传递给所需参数这样简单的错误通常也不会显式处理，进而导致崩溃或未定义行为。Vulkan 要求明确说明正在执行的所有操作，编写代码时很容易犯错误。

Vulkan 引入了**验证层**，验证层是可选的，它们挂接到 Vulkan 函数调用以适配其他操作。验证层中的常见操作包括：
+ 根据规范检查参数值以检测误用
+ 追踪对象的创建和销毁，并查找资源泄漏
+ 通过跟踪调用源自的线程检查线程安全性
+ 将每个调用及其参数记录到标准输出
+ 追踪 Vulkan 需要分析和重放

比如有个例子：
```cpp
VkResult vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* instance) {

    if (pCreateInfo == nullptr || instance == nullptr) {
        log("Null pointer passed to required parameter!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return real_vkCreateInstance(pCreateInfo, pAllocator, instance);
}
```

验证层可以自定义地堆叠，Vulkan 本身不具有任何内置验证层，但是 LunarG Vulkan SDK 提供了检查通用错误的层级集合，都是开源的，但是只有安装了才能用，例如 LunarG 验证层只有装了 Vulkan SDK 的电脑才能用。

以前在 Vulkan 中有两种不同类型的验证层：实例和特定于设备的验证层。实例层只检查与全局 Vulkan 对象（如实例）相关的调用，设备特定层只检查与特定 GPU 相关的调用。设备特定层现在已弃用，实例验证层适用于所有 Vulkan 调用。标准文档建议在设备级启用验证层以实现兼容性。

### 使用验证层

验证层需要通过名字启用，所有有用的验证层都被打包进了 Vulkan SDK，名字是 `VK_LAYER_KHRONOS_validation`。向类中添加两个成员控制验证层，根据 `NDEBUG` 宏定义（Debug 模式下为 0，Release 模式下为 1）判断验证层的开启与关闭：
```cpp
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> m_vecValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
```
> 虽然想用 `std::vector<std::string>`，但是后面会发现某些接口不支持 `std::string`。

添加函数 `checkValidatioinLayerSupport` 检查需要的层是否可用。使用 `vk::enumerateInstanceLayerProperties` 获取所有可用的层，用法和 `vk::enumerateInstanceExtensionProperties` 一样：
```cpp
bool HelloTriangleApplication::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
    
    return false;
}
```
然后检查一下 `m_VecValidationLayers` 中的层是否存在于 `availableLayers` 中：
```cpp
bool HelloTriangleApplication::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

    for (const auto &layerName : m_VecValidationLayers) {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) return false;
    }
    return true;
}
```

在 `createInstance` 函数中调用：
```cpp
void HelloTriangleApplication::createInstance() {
    if (m_EnableValidationLayers && !checkValidationLayerSupport())
        throw std::runtime_error("validation layers requested, but not available!");
    ...
}
```
如果程序用到的层都存在，`createInstance` 函数不会抛出异常。后续需要让 `vk::InstanceCreateInfo` 指定层：
```cpp
void HelloTriangleApplication::createInstance() {
    if (m_EnableValidationLayers)
        createInfo.setPEnabledLayerNames(m_VecValidationLayers);
}
```
成功的话 `vk::CreateInstance` 不会抛出异常。如果写错 `m_vecValidationLayers` 中的层名且跳过了 `checkValidationLayerSupport` 检查，`vk::CreateInstance` 将会抛出异常。

### 信息回调函数

默认情况下，验证层会将 debug 信息打印到标准输出，也可以根据用户自定义的显式回调控制输出，过滤不重要的错误，只显示致命的错误。

设置回调函数来处理消息，必须使用 `VK_EXT_debug_utils` 扩展并设置带有回调的发信方。添加 `getRequiredExtensions` 函数，根据 `m_EnableValidationLayers` 获取需要的层扩展列表：
```cpp
std::vector<const char *> HelloTriangleApplication::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensionsRaw;

    glfwExtensionsRaw = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensionsRaw, glfwExtensionsRaw + glfwExtensionCount);

    if (m_EnableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}
```

GLFW 需要的扩展必须在列表之内，但是 debug 发信方扩展是按条件添加的。这里使用 `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` 宏定义，等价于字面量 `"VK_EXT_debug_utils"`，避免出错。此扩展不需要作检查，它应该是所有验证层都支持的。

添加一个静态成员函数 `debugCallBack`，类型为 `PFN_vkDebugUtilsMessengerCallBackEXT`，
```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData);
```

原文说 `VKAPI_ATTR` 和 `VKAPI_CALL` 是保证函数签名以供 Vulkan 调用的。四个参数的作用分别是：
+ `VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity` 包括
    + `VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT`：诊断信息
    + `VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT`：如创建对象等信息
    + `VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT`：非错误行为的信息，但极可能是程序中的 bug
    + `VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT`：非法的、可能引起崩溃的信息
    信息的严重程度是可以比较的，从最不严重到最严重：
    ```cpp
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        // 重要到必须显示的信息
    }
    ```
+ `VkDebugUtilsMessageTypeFlagsEXT                  messageTypes` 包括
    + `VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT`：已经发生的、与标准和性能无关的事件
    + `VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT`：已经发生的、违反标准或表明潜在错误的事件
    + `VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT`：潜在的 Vulkan 非最优使用
+ `const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData`：包含信息细节的结构体，重要的成员有
    + `pMessage`：debug 信息，以 `\0` 结尾的字符串
    + `pObjects`：一个存储与信息相关的 Vulkan 对象句柄的数组
    + `objectCount`：上述数组的长度
+ `void* pUserData`：包含一个在设置回调时指定的指针，允许用户传递数据

回调函数返回的 `VkBool32` 表示触发验证层信息的 Vulkan 调用是否中止。如果返回 `true`，调用会抛出 `VK_ERROR_VALIDATION_FAILED_EXT` 错误并中止。通常只在测试验证层本身时用，所以应该返回 `VK_FALSE`。

最后一步是告诉 Vulkan 我们声明的回调函数。麻烦的是即便是回调函数也需要显式地创建和销毁。回调函数是**调试发信方**（debug messenger）的一部分，用户可以指定任意数量的回调。在 `vk::Instance` 下添加一个类成员
```cpp
vk::DebugUtilsMessengerEXT m_DebugMessenger;
```
添加 `setupDebugMessenger` 函数，并在 `initVulkan` 函数中的 `createInstance` 之后被调用：
```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
}

void setupDebugMessenger() {
    if (!m_EnableValidationLayers) return;
}
```

创建结构体并填入信息和回调函数：
```cpp
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    createInfo.setMessageSeverity(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | 
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback)
        .setPUserData(nullptr); // 可选的
```
`setMessageSeverity` 允许用户指定在自定义回调函数中的严重性类型。这里标注了除 `vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo` 之外的所有类型。

`setMessageType` 允许用户过滤回调函数中的指定信息类型。这里指定了所有类型。

`setPfnUserCallback` 指定回调函数的函数指针，传递给 `setPUserData` 的指针是可选的，可以把指向 `HelloTriangleApplication` 的指针传递给回调函数。

将结构体传递给 `m_Instance.createDebugUtilsMessengerEXT` 中，得到 `vk::DebugUtilsMessengerEXT` 对象。注意，此函数是扩展函数，不会被自动加载，这里需要查找函数地址，不同于原文使用 `vkGetInstanceProcAddr` 查找函数入口并强制转换为所需类型的形式，这里使用 `vk::DispatchLoaderDynamic` 查找所需的函数入口，否则会报找不到 `createDebugUtilsMessengerEXT` 函数的错误：
```cpp
m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(createInfo, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));
```

函数 `createDebugUtilsMessengerEXT` 的第二个参数是可选的 allocator，这里传递 `nullptr`。

`vk::DebugUtilsMessengerEXT` 对象需要在程序推出熏黑i前销毁。同样的，需要显式加载函数入口：
```cpp
m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));
```

于是 `cleanUp` 函数修改为
```cpp
void HelloTriangleApplication::cleanUp() {
    if (m_EnableValidationLayers)
        m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));

    m_Instance.destroy();

    glfwDestroyWindow(m_pWIndow);

    glfwTerminate();
}
```

### debug `vk::Instance` 的创建和销毁

需要注意的是，`vk::DebugUtilsMessengerEXT` 在 `vk::Instance` 之后被创建，在 `vk::Instance` 之前被销毁，所以它不能覆盖到 `vk::Instance` 的创建和销毁过程。查看扩展文档可知，对于 `vk::Instance` 的创建和销毁，有另外的、独立的 debug 发信方创建方式。只需要给 `vk::InstanceCreateInfo` 的 `pNext` 成员赋值为 `vk::DebugUtilsMessengerCreateInfoEXT` 的指针即可。首先将发信方的创建过程抽离为函数：
```cpp
void HelloTriangleApplication::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT();
    createInfo.setMessageSeverity(
                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback);
}
```
然后替换 `setDebugMessenger()` 中的 `vk::DebugUtilsMessengerCreateInfoEXT` 的初始化部分：
```cpp
void HelloTriangleApplication::setupDebugMessenger() {
    if (!m_EnableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(createInfo, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));
}
```

最后在 `createInstance` 中传递 `vk::DebugUtilsMessengerEXT` 的指针给 `vk::InstanceCreateInfo` 的 `pNext` 成员：
```cpp
void HelloTriangleApplication::createInstance() {
    ...
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;

    if (m_EnableValidationLayers) {
        createInfo.setPEnabledLayerNames(m_VecValidationLayers);

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setPNext(&debugCreateInfo);
    } else
        createInfo.setEnabledLayerCount(0).setPNext(nullptr);

    m_Instance = vk::createInstance(createInfo);
    if (!m_Instance)
        throw std::runtime_error("failed to create instance!");
}
```

注意到上面的代码中，`vk::DebugUtilsMessengerCreateInfoEXT` 的变量生命在了 `if` 语句的外面，保证在 `vk::CreateInstance` 调用之前不会被销毁。通过创建额外的 debug 发信方，程序将会自动的在 `vk::Instance` 的创建和销毁时调用它。

> 注意在 `createInstance` 函数中，传递给 `vk::InstanceCreateInfo` 的 `pNext` 字段的是 `DebugUtilsMessengerCreateInfoEXT` 结构体指针，并不是 `vk::DebugUtilsMessengerEXT`。

> 测试一下，如果忘记销毁 `vk::DebugUtilsMessengerEXT` 会报错，比如
```bash
validation layerValidation Error: [ VUID-vkDestroyInstance-instance-00629 ] Object 0: handle = 0x555555684700, type = VK_OBJECT_TYPE_INSTANCE; Object 1: handle = 0xfd5b260000000001, type = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT; | MessageID = 0x8b3d8e18 | OBJ ERROR : For VkInstance 0x555555684700[], VkDebugUtilsMessengerEXT 0xfd5b260000000001[] has not been destroyed. The Vulkan spec states: All child objects created using instance must have been destroyed prior to destroying instance (https://vulkan.lunarg.com/doc/view/1.3.239.0/linux/1.3-extensions/vkspec.html#VUID-vkDestroyInstance-instance-00629)
```

### 配置

当然关于验证层还有其他配置了，Vulkan SDK 的 config 目录下的 `vk_layer_settings.txt` 文件详细解释了如何配置层。这里就不展开了。