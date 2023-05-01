# Vulkan Tutorial (C++ Ver.)

## 基础代码

### 基本结构

先实现代码的一个简略版本：
```cpp
#include <vulkan/vulkan.hpp>
#include <stdexcept>
#include <cstdlib>

class HelloTriangleApplication {
public:
    void run() {
        initVulkan();
        mainLoop();
        cleanUp();
    }

private:
    void initVulkan() {}
    void mainLoop() {}
    void cleanUp() {}
}
```
首先要引入的是 Vulkan 的头文件。为使用 C++ 及其语言特性，这里引入的是 `vulkan/vulkan.hpp`。文件中包含大量的函数、结构体和枚举类型。`EXIT_SUCCESS` 和 `EXIT_FAILURE` 定义在 `<cstdlib>` 中。

程序被封装在类 `HelloTriangleApp` 中，此类也将 Vulkan 对象存储为私有对象，并在 `initVulkan` 函数中调用初始化 Vulkan 对象的函数。`mainLoop` 函数包含一个循环，它会一直迭代到窗口关闭。一旦循环结束，`cleanUp` 函数将会回收并释放资源。

如果出现错误，抛出 `std::runtime_error` 并将错误信息输出到命令行。

### 资源管理

正如程序中调用 `free` 释放通过 `malloc` 申请的内存一样，每个创建的 Vulkan 对象都需要被**显式销毁**。在 C++ 中，这个操作可以通过 RAII 机制或者 `<memory>` 头中的智能指针删除。但是为了保证资源的申请和释放，开发中还是采用手动管理。明确对象的生命周期，对学习 API 的工作方式还是有好处的。

在 C++ 中使用类的构造函数申请 Vulkan 对象，并在析构函数中释放对象。或者给 `std::unique_ptr` 或 `std::shared_ptr` 提供析构函数完成资源回收。对于更大规模的 Vulkan 程序，还是推荐 RAII 机制。至于学习 API 嘛，就手动管理好了。

Vulkan 对象使用类似 `vkCreateXXX` 的函数进行管理。但是基于 C++ 的编程为我们提供了 `vk::CreateXXX` 函数。两者相同。同样的，为 Vulkan 对象分配空间时调用类似 `vkAllocateXXX` 和 `vk::AllocateXXX` 的函数。对应销毁对象和回收空间的函数形如 `vkDestroyXXX` 与 `vk::DestroyXXX`，和 `vkFreeXXX` 与 `vk::FreeXXX`。所有的函数有一个相同类型的参数 `pAllocator`（C++ 里是 `Optional<const vk::AllocationCallbacks> allocator`）是可选参数。对于一些管理 Vulkan 对象的第三方库，可以尝试（瞎说）。

### 整合 GLFW

如果只是离线渲染，有 Vulkan 就足够了。但是如果要创建窗口并显示，则需要引入 GLFW。用下面的代码替换 `#include <vulkan/vulkan.hpp>`：
```cpp
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
```
这样 GLFW 会引入自己的定义并自动加载 Vulkan 头文件。添加函数 `initWindow` 并在 `run` 中的所有调用前调用它，它负责 GLFW 的初始化和窗口创建。
```cpp
void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
}

private:
    void initWindow() {}
```

一开始需要调用 `glfwInit()` 来初始化 GLFW 库。GLFW 最初是为 OpenGL 库设计的，这里需要声明在后续调用中不要创建 OpenGL 上下文：
```cpp
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
```
此外暂时不处理窗口缩放，有
```cpp
glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
```
使用 `GLFWwindow* window` 作为类的私有成员，存储创建的 GLFW 窗口指针，并初始化：
```cpp
window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
```
前三个参数表示窗口的宽、高和标题。第四个参数允许指定打开窗口的显示器，最后一个参数只与 OpenGL 相关。

建议使用硬编码常量固定宽高：
```cpp
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
```
并替换
```cpp
window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
```
现在 `initWindow` 函数长这样：
```cpp
void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHInt(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}
```

程序运行需要依靠循环，直到错误出现或者窗口被关闭。在 `mainLoop` 中添加事件循环：
```cpp
void main() {
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}
```

上述代码很容易理解，任何待处理的事件，如按下 `X`，或者关闭窗口，都会结束循环。同时循环也是我们之后调用函数并渲染一帧的位置。

一旦窗口关闭，我们需要回收并销毁资源，并终止 GLFW：
```cpp
void cleanUp()
{
    glfwDestroyWindow(window);
    
    glfwTerminate();
}
```

> 运行程序，有个黑框框。
这就是 Vulkan 程序的骨架了。