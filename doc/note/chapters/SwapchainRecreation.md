# Vulkan Tutorial (C++ Ver.)

## 交换链重创建

### 介绍

现在已经成功绘制了三角形，但是还需要处理一些情况。窗口表面可能会变化，以至交换链不再适用。情况发生的原因之一是窗口大小改变。我们必须处理这些事件，重新建立交换链。

### 重新创建交换链：

添加成员函数 `recreateSwapChain`，调用 `createSwapChain` 等所有依赖于交换链或者窗口大小的对象的函数。
```cpp
void HelloTriangleApplication::recreateSwapChain()
{
    m_Device.waitIdle();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
}
```

首先调用 `m_Device.waitIdle()`，因为我们不应该触摸可能仍在使用的资源。显然，要做的第一件事是重新创建交换链本身。需要重新创建图像视图，因为它们直接依赖交换链图像。需要重新创建渲染通道，因为它取决于交换链图像的格式。在窗口调整大小等操作期间，交换链图像格式很少发生变化，但仍然应该进行处理。视窗和剪刀矩形的大小是在图形管线创建期间指定的，因此管线也需要重建。可以通过对视窗和剪刀矩形使用动态状态来避免这种情况。最后，帧缓冲直接依赖于交换链图像。

为确保旧对象在重新创建之前清理干净，将清理代码移到一个单独的函数中，以便从 `recreateSwapChain` 函数调用该函数，即 `cleanupSwapChain`：
```cpp
void HelloTriangleApplication::recreateSwapChain()
{
    m_Device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
}

void HelloTriangleApplication::cleanupSwapChain() {}
```

将清理基于交换链创建对象的代码从 `cleanup` 移动到 `cleanupSwapChain` 中：
```cpp
void HelloTriangleApplication::cleanupSwapChain()
{
    for (auto& framebuffer : m_vecSwapchainFramebuffers)
        m_Device.destroyFramebuffer(framebuffer);

    m_Device.destroyPipeline(m_GraphicsPipeline);
    m_Device.destroyPipelineLayout(m_PipelineLayout);
    m_Device.destroyRenderPass(m_RenderPass);

    for (auto& imageView : m_vecSwapChainImageViews)
        m_Device.destroyImageView(imageView);

    m_Device.destroySwapchainKHR(m_SwapChain);
}

void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_Device.destroySemaphore(m_vecImageAvailableSemaphores[i]);
        m_Device.destroySemaphore(m_vecRenderFinishedSemaphores[i]);
        m_Device.destroyFence(m_vecInFlightFences[i]);
    }

    m_Device.destroyCommandPool(m_CommandPool);
    m_Device.destroy();

    if (m_EnableValidationLayers)
        m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));

    m_Instance.destroySurfaceKHR(m_Surface);
    m_Instance.destroy();

    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}
```

注意，在 `chooseSwapExtent` 中，我们已经查询了新的窗口分辨率，以确保交换链图像具有（新的）正确的大小，因此不需要修改 `chooseSwapExtent`（记在创建交换链时，已经使用`glfwGetFramebufferSize` 获得以像素为单位的表面分辨率）。

这种方法的缺点是在创建新的交换链之前停止所有的呈现操作。可以从旧交换链绘制图像命令时创建新的交换链。需要将之前的交换链传递给 `vk::SwapchainCreateInfoKHR` 结构体中的 `setoldswchain` 函数，并在使用完旧的交换链后立即销毁它。

### 次优或过时的交换链

现在只需确定何时需要重新创建交换链，并调用新的`recreateSwapChain` 函数。Vulkan 通常会告诉我们交换链在渲染过程中不适用。`m_Device.acquireNextImageKHR()` 和 `m_PresentQueue.presentKHR()` 函数可以返回以下特殊值来表示这一点。
+ `vk::Result::eErrorOutOfDateKHR`：交换链不再兼容表面，不能再用于渲染，通常发生在调整窗口大小之后
+ `vk::Result::eSuboptimalKHR`：交换链仍然可以用来成功地呈现到表面，但表面属性不再完全匹配

```cpp
const auto& value = m_Device.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_vecImageAvailableSemaphores[m_CurrentFrame], nullptr);
if (value.result == vk::Result::eErrorOutOfDateKHR)
{
    recreateSwapChain();
    return;
}
else if (value.result != vk::Result::eSuccess && value.result != vk::Result::eSuboptimalKHR)
    throw std::runtime_error("failed to acquire swap chain image!");
```

如果交换链在尝试获取图像时已经 out-of-date，就不可能再向其呈现。因此应该立即重新创建交换链，并在下一次 `drawFrame` 调用中再次尝试。

如果交换链不是最优的，用户也可以决定继续进行，因为已经获得了一个图像。`vk::Result::eSuccess` 和 `vk::Result::eSuboptimalKHR` 都被认为是“成功”返回值。
```cpp
auto result = m_PresentQueue.presentKHR(presentInfo);
if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    recreateSwapChain();
else if (result != vk::Result::eSuccess)
    throw std::runtime_error("presentKHR failed!");

m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
```

`m_PresentQueue.presentKHR` 函数返回具有相同含义的相同值。在这种情况下，如果交换链不是最优的，也重新创建交换链，因为我们希望得到最好的结果。

### 解决死锁

现在运行代码可能会遇到死锁。程序到达了 `m_Device.waitForFences`。这是因为当 `m_Device.acquireNextImageKHR` 返回 `vk::Result::eErrorOutOfDateKHR` 时，我们重新创建交换链，然后从 `drawFrame` 返回。在此之前，当前框架的围栏被等待和重置。由于我们立即返回，没有工作提交执行，栅栏永远不会被发出信号，导致 `waitForFences` 永远停止。

有一个简单的办法。延迟重置围栏，直到确定将与它一起提交工作。因此，如果提前返回，围栏仍然是有信号的，并且 `waitForFences` 不会死锁，直到下次使用相同的栅栏对象。
```cpp

void HelloTriangleApplication::drawFrame()
{
    const auto& _ = m_Device.waitForFences(m_vecInFlightFences[m_CurrentFrame], true, std::numeric_limits<uint64_t>::max());

    const auto& value = m_Device.acquireNextImageKHR(m_SwapChain, std::numeric_limits<uint64_t>::max(), m_vecImageAvailableSemaphores[m_CurrentFrame], nullptr);
    if (value.result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapChain();
        return;
    }
    else if (value.result != vk::Result::eSuccess && value.result != vk::Result::eSuboptimalKHR)
        throw std::runtime_error("failed to acquire swap chain image!");
    
    // 只在提交命令时重置围栏
    uint32_t imageIndex = value.value;
    ...
}
```

### 显式处理窗口缩放

尽管许多驱动程序和平台在窗口调整大小后自动触发`vk::Result::eErrorOutOfDateKHR`，但并不保证会发生。这就是添加额外的代码来显式地处理大小调整的原因。首先添加一个新成员变量，标记发生了大小调整：
```cpp
bool m_FramebufferResized = false;
```

修改 `drawFrame` 函数：
```cpp
if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
{
    m_FramebufferResized = false;
    recreateSwapChain();
}
else if (result != vk::Result::eSuccess)
    throw std::runtime_error("presentKHR failed!");
```

在 `m_PresentQueue.presentKHR` 之后执行此操作非常重要，以确保信号量一致，否则发出信号的信号量可能不会被正确等待。现在要真正检测调整大小，可以使用 `GLFW` 框架中的`glfwSetFramebufferSizeCallback` 函数进行设置
回调函数：
```cpp
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) { }

void HelloTriangleApplication::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_pWindow = glfwCreateWindow(m_Width, m_Height, "Vulkan", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
}
```

创建静态函数作为回调函数的原因是 `GLFW` 不知道如何正确调用成员函数，该函数的右 `this` 指针指向`HelloTriangleApplication` 实例。然而，在回调中获得了对 `GLFWwindow` 的引用，并且还有另一个 `GLFW` 函数允许存储任意指针：`glfwSetWindowUserPointer`：
```cpp
void HelloTriangleApplication::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_pWindow = glfwCreateWindow(m_Width, m_Height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(m_pWindow, this);
    glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
}
```

这个值可以通过 `glfwGetWindowUserPointer` 在回调中检索：
```cpp
static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
    app->m_FramebufferResized = true;   
}
```

### 处理最小化窗口

还有另一种情况，交换链可能会过时，这是一种特殊的窗口大小调整：窗口最小化。这种情况很特殊，因为它将导致帧缓冲区大小为0。在本教程中，我们将通过扩展 `recreateSwapChain` 函数来处理这个问题，直到窗口再次出现在前台:
```cpp
void HelloTriangleApplication::recreateSwapChain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    while(width == 0 || height == 0) {
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        glfwWaitEvents();
    }

    m_Device.waitIdle();
    ...
}
```

对 `glfwGetFramebufferSize` 的初始调用处理的是大小已经正确并且 `glfwWaitEvents` 不许等待的情况。现在已经完成了一个运行良好的 Vulkan 程序。