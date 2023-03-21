#include "render/render.h"
#include "GLFW/glfw3.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <map>
#include <set>
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

    m_pWindow = glfwCreateWindow(m_Width, m_Height, "Vulkan", nullptr, nullptr);
}

void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
    }
}

void HelloTriangleApplication::cleanUp() {
    m_Device.destroy();

    if (m_EnableValidationLayers)
        m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger, nullptr, vk::DispatchLoaderDynamic(m_Instance, vkGetInstanceProcAddr));

    m_Instance.destroySurfaceKHR(m_Surface);
    m_Instance.destroy();

    glfwDestroyWindow(m_pWindow);
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

void HelloTriangleApplication::createSurface()
{
    VkSurfaceKHR tmpSurface;
    VkResult result = glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &tmpSurface);
    if (result != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
    m_Surface = tmpSurface;
}

void HelloTriangleApplication::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT();
    createInfo.setMessageSeverity(
                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback)
        .setPUserData(nullptr); // optional
}

void HelloTriangleApplication::pickPhysicalDevice() {
    auto devices = m_Instance.enumeratePhysicalDevices();
    if (devices.size() == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    std::multimap<int, vk::PhysicalDevice> candidates;

    for (const auto &device : devices) {
        if (isDeviceSuitable(device))
        {
            int score = rateDeviceSuitability(device);
            candidates.insert({ score, device });
        }
    }

    if (candidates.rbegin()->first > 0)
        m_PhysicalDevice = candidates.rbegin()->second;
    else
        throw std::runtime_error("failed to find a suitable GPU!");
}

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

    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo createInfo{};
    createInfo.setQueueCreateInfos(queueCreateInfos)
    .setPEnabledFeatures(&deviceFeatures)
    .setEnabledExtensionCount(0);

    if (m_EnableValidationLayers)
        createInfo.setPEnabledLayerNames(m_VecValidationLayers);

    m_Device = m_PhysicalDevice.createDevice(createInfo);
    if(!m_Device)
        throw std::runtime_error("failed to create logical device!");

    m_GraphicsQueue = m_Device.getQueue(indices.graphicsFamily.value(), 0);
    m_PresentQueue = m_Device.getQueue(indices.presentFamily.value(), 0);
}

bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    return indices.isComplete();
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

HelloTriangleApplication::QueueFamilyIndices
HelloTriangleApplication::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;
    auto queueFamilyProperties = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : queueFamilyProperties)
    {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
            indices.graphicsFamily = i;

        auto presentSupport = device.getSurfaceSupportKHR(i, m_Surface);
        if (presentSupport) indices.presentFamily = i;
        
        if (indices.isComplete())   break;
        ++i;
    }

    return indices;
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