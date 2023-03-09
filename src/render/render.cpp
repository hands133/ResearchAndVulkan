#include "render/render.h"
#include "GLFW/glfw3.h"
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

void HelloTriangleApplication::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
}

void HelloTriangleApplication::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_pWIndow = glfwCreateWindow(m_Width, m_Height, "Vulkan", nullptr, nullptr);
}

void HelloTriangleApplication::initVulkan() {
    vk_createInstance();
}
void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWIndow)) {
        glfwPollEvents();
    }
}
void HelloTriangleApplication::cleanUp() {
    glfwDestroyWindow(m_pWIndow);

    glfwTerminate();
}

void HelloTriangleApplication::vk_createInstance() {
    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("Hello Triangle")
        .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
        .setPEngineName("No Engine")
        .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
        .setApiVersion(VK_API_VERSION_1_0);

    vk::InstanceCreateInfo createInfo;
    createInfo.setPApplicationInfo(&appInfo);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensionsRaw;

    glfwExtensionsRaw = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> glfwExtensions(glfwExtensionsRaw, glfwExtensionsRaw + glfwExtensionCount);

    createInfo.setEnabledExtensionCount(glfwExtensionCount)
        .setPEnabledExtensionNames(glfwExtensions)
        .setEnabledLayerCount(0);

    m_Instance = vk::createInstance(createInfo);
    if (!m_Instance)
        throw std::runtime_error("failed to create instance!");
}
