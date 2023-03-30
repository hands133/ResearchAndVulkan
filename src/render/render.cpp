#include "render/render.h"
#include "GLFW/glfw3.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <algorithm>
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
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
}

void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
    }
}

void HelloTriangleApplication::cleanUp() {
    m_Device.destroyPipelineLayout(m_PipelineLayout);

    for (auto& imageView : m_vecSwapChainImageViews)
        m_Device.destroyImageView(imageView);

    m_Device.destroySwapchainKHR(m_SwapChain);
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

    for (const auto &layerName : m_vecValidationLayers) {
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
    .setPEnabledExtensionNames(m_vecDeviceExtensions);

    if (m_EnableValidationLayers)
        createInfo.setPEnabledLayerNames(m_vecValidationLayers);

    m_Device = m_PhysicalDevice.createDevice(createInfo);
    if(!m_Device)
        throw std::runtime_error("failed to create logical device!");

    m_GraphicsQueue = m_Device.getQueue(indices.graphicsFamily.value(), 0);
    m_PresentQueue = m_Device.getQueue(indices.presentFamily.value(), 0);
}

void HelloTriangleApplication::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_PhysicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
    if (swapChainSupport.capabilities.maxImageCount > 0
        && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.setSurface(m_Surface)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

        QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value() };
        if (indices.graphicsFamily != indices.presentFamily)
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndices(queueFamilyIndices);
        else
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive);

        createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(presentMode)
            .setClipped(true)
            .setOldSwapchain(nullptr);

    m_SwapChain = m_Device.createSwapchainKHR(createInfo);
    if (!m_SwapChain)   throw std::runtime_error("failed to create swap chain!");

    m_vecSwapChainImages = m_Device.getSwapchainImagesKHR(m_SwapChain);
    m_SwapChainImageFormat = surfaceFormat.format;
    m_SwapChainExtent = extent;
}

void HelloTriangleApplication::createImageViews()
{
    for (auto& image : m_vecSwapChainImages)
    {
        vk::ImageViewCreateInfo createInfo;
        createInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_SwapChainImageFormat)
        .setComponents(vk::ComponentMapping(
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity))
        .setSubresourceRange(vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1));
        auto& view = m_vecSwapChainImageViews.emplace_back(m_Device.createImageView(createInfo));
        if (!view)  throw std::runtime_error("failed to create image views!");
    }
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    auto vertShaderCode = readFile("./src/shaders/vert.spv");
    auto fragShaderCode = readFile("./src/shaders/frag.spv");

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(vertShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(fragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStagesInfo[] =
        { vertShaderStageInfo, fragShaderStageInfo };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.setVertexBindingDescriptions({})
    .setVertexAttributeDescriptions({});

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
    .setPrimitiveRestartEnable(false);

    vk::Viewport viewport{};
    viewport.setX(0.0f)
    .setY(0.0f)
    .setWidth(m_SwapChainExtent.width)
    .setHeight(m_SwapChainExtent.height)
    .setMinDepth(0.0f)
    .setMaxDepth(1.0f);

    vk::Rect2D scissor{};
    scissor.setOffset(vk::Offset2D(0, 0))
        .setExtent(m_SwapChainExtent);

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewports(viewport)
    .setScissors(scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(false)
        .setRasterizerDiscardEnable(false)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setLineWidth(1.0f)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eClockwise)
        .setDepthBiasEnable(false)
        .setDepthBiasConstantFactor(0.0f)   // Optional
        .setDepthBiasClamp(0.0f)            // Optional
        .setDepthBiasSlopeFactor(0.0f);     // Optional

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setSampleShadingEnable(true)
        .setSampleShadingEnable(true)
        .setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setMinSampleShading(1.0f)  // Optional
        .setPSampleMask(nullptr)    // Optional
        .setAlphaToCoverageEnable(false)               // Optional
        .setAlphaToOneEnable(false);    // Optional

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlags(
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA))
        .setBlendEnable(false)
        .setSrcColorBlendFactor(vk::BlendFactor::eOne)  // Optional
        .setDstColorBlendFactor(vk::BlendFactor::eZero) // Optional
        .setColorBlendOp(vk::BlendOp::eAdd)  // Optional
        .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)  // Optional
        .setDstAlphaBlendFactor(vk::BlendFactor::eZero) // Optional
        .setAlphaBlendOp(vk::BlendOp::eAdd);    // Optional

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setLogicOpEnable(false)
    .setLogicOp(vk::LogicOp::eCopy)     // Optional
    .setAttachments(colorBlendAttachment)
    .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f }); // Optional

    std::vector<vk::DynamicState> dynamicStates {
        vk::DynamicState::eViewport,
        vk::DynamicState::eLineWidth
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts({})    // Optional
        .setPushConstantRanges({}); // Optional

    m_PipelineLayout = m_Device.createPipelineLayout(pipelineLayoutInfo);
    if (!m_PipelineLayout)  throw std::runtime_error("failed to create pipeline layout!");


    m_Device.destroy(fragShaderModule);
    m_Device.destroy(vertShaderModule);
}

bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    auto deviceExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(m_vecDeviceExtensions.begin(), m_vecDeviceExtensions.end());

    for (const auto& extension : deviceExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();    
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

HelloTriangleApplication::SwapChainSupportDetails
HelloTriangleApplication::querySwapChainSupport(vk::PhysicalDevice device)
{
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(m_Surface);
    details.formats = device.getSurfaceFormatsKHR(m_Surface);
    details.presentModes = device.getSurfacePresentModesKHR(m_Surface);
    
    return details;
}

vk::SurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
            && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return availableFormat;
    }
    return availableFormats.front();
}

vk::PresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
            return availablePresentMode;

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D HelloTriangleApplication::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;
    else 
    {
        int width, height;
        glfwGetFramebufferSize(m_pWindow, &width, &height);

        vk::Extent2D actualExtent(width, height);

        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
        
        return actualExtent;
    }
}

vk::ShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo;
    createInfo.setCodeSize(code.size())
        .setPCode(reinterpret_cast<const uint32_t*>(code.data()));

    vk::ShaderModule shaderModule = m_Device.createShaderModule(createInfo);
    if (!shaderModule)  throw std::runtime_error("failed to create shader module!");

    return shaderModule;
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
        createInfo.setPEnabledLayerNames(m_vecValidationLayers);

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setPNext(&debugCreateInfo);
    } else
        createInfo.setEnabledLayerCount(0).setPNext(nullptr);

    m_Instance = vk::createInstance(createInfo);
    if (!m_Instance)
        throw std::runtime_error("failed to create instance!");
}