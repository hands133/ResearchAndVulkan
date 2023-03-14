#include "render/render.h"
#include "GLFW/glfw3.h"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan.hpp>
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
    createInstance();
    setupDebugMessenger();
}

void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWIndow)) {
        glfwPollEvents();
    }
}

void HelloTriangleApplication::cleanUp() {
    if (m_EnableValidationLayers)
        m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));

    m_Instance.destroy();

    glfwDestroyWindow(m_pWIndow);

    glfwTerminate();
}

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

std::vector<const char *> HelloTriangleApplication::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensionsRaw;

    glfwExtensionsRaw = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensionsRaw, glfwExtensionsRaw + glfwExtensionCount);

    if (m_EnableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangleApplication::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    std::cerr << "validation layer" << pCallbackData->pMessage << std::endl;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {}
    return VK_FALSE;
}

void HelloTriangleApplication::setupDebugMessenger() {
    if (!m_EnableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(createInfo, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));
}

void HelloTriangleApplication::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT();
    createInfo.setMessageSeverity(
                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback)
        .setPUserData(nullptr); // optional
}

void HelloTriangleApplication::createInstance() {
    if (m_EnableValidationLayers && !checkValidationLayerSupport())
        throw std::runtime_error("validation layers requested, but not available!");

    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("Hello Triangle")
        .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
        .setPEngineName("No Engine")
        .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
        .setApiVersion(VK_API_VERSION_1_0);

    vk::InstanceCreateInfo createInfo;
    createInfo.setPApplicationInfo(&appInfo);

    auto extensions = getRequiredExtensions();

    createInfo.setPEnabledExtensionNames(extensions)
        .setEnabledLayerCount(0);

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