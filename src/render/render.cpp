#include "render/render.h"
#include "GLFW/glfw3.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <set>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <utils/stb_image.h>

#include <utils/tiny_obj_loader.h>

void HelloTriangleApplication::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
    app->m_FramebufferResized = true;   
}

void HelloTriangleApplication::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_pWindow = glfwCreateWindow(m_Width, m_Height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(m_pWindow, this);
    glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
}

void HelloTriangleApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFramebuffers();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void HelloTriangleApplication::mainLoop() {
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
        drawFrame();
    }
    
    m_Device.waitIdle();
}

void HelloTriangleApplication::cleanUp() {
    cleanupSwapChain();

    m_Device.destroySampler(m_TextureSampler);
    m_Device.destroyImageView(m_TextureImageView);

    m_Device.destroyImage(m_TextureImage);
    m_Device.freeMemory(m_TextureImageMemory);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_Device.destroyBuffer(m_vecUniformBuffers[i]);
        m_Device.freeMemory(m_vecUniformBuffersMemory[i]);
    }

    m_Device.destroyDescriptorPool(m_DescriptorPool);
    m_Device.destroyDescriptorSetLayout(m_DescriptorSetLayout);

    m_Device.destroyBuffer(m_IndexBuffer);
    m_Device.freeMemory(m_IndexBufferMemory);

    m_Device.destroyBuffer(m_VertexBuffer);
    m_Device.freeMemory(m_VertexBufferMemory);

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
    std::cerr << "[validation] " << pCallbackData->pMessage << std::endl;

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
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            // vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
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
    deviceFeatures.setSamplerAnisotropy(true);

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
    m_vecSwapChainImageViews.clear();
    for (const auto& image : m_vecSwapChainImages)
        m_vecSwapChainImageViews.emplace_back(createImageView(image, m_SwapChainImageFormat, vk::ImageAspectFlagBits::eColor));
}

void HelloTriangleApplication::createRenderPass()
{
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(m_SwapChainImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.setAttachment(0)
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depthAttachment{};
    depthAttachment.setFormat(findDepthFormat())
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference depthAttachmentRef{};
    depthAttachmentRef.setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass{};
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachments(colorAttachmentRef)
        .setPDepthStencilAttachment(&depthAttachmentRef);

    vk::SubpassDependency dependency{};
    dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setSrcAccessMask(vk::AccessFlags{0})
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    std::array<vk::AttachmentDescription, 2> attachments = 
        { colorAttachment, depthAttachment };

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachments(attachments)
        .setSubpasses(subpass)
        .setDependencies(dependency);
    m_RenderPass = m_Device.createRenderPass(renderPassInfo);
    if (!m_RenderPass)  throw std::runtime_error("failed to create render pass!");

}

void HelloTriangleApplication::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);
        // .setPImmutableSamplers(nullptr);    // Optional

    vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.setBinding(1)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setPImmutableSamplers(nullptr);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = 
    {   uboLayoutBinding, samplerLayoutBinding };

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.setBindings(bindings);
    
    m_DescriptorSetLayout = m_Device.createDescriptorSetLayout(layoutInfo);
    if (!m_DescriptorSetLayout)
        throw std::runtime_error("failed to create descriptor set layout!");
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
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(fragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStagesInfo[] =
        { vertShaderStageInfo, fragShaderStageInfo };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.setVertexBindingDescriptions(bindingDescription)
        .setVertexAttributeDescriptions(attributeDescriptions);

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
        .setCullMode(vk::CullModeFlagBits::eNone)
        // .setCullMode(vk::CullModeFlagBits::eBack)
        // .setFrontFace(vk::FrontFace::eCounterClockwise)
        // .setFrontFace(vk::FrontFace::eClockwise)
        .setDepthBiasEnable(false)
        .setDepthBiasConstantFactor(0.0f)   // Optional
        .setDepthBiasClamp(0.0f)            // Optional
        .setDepthBiasSlopeFactor(0.0f);     // Optional

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setSampleShadingEnable(false)
        .setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setMinSampleShading(1.0f)  // Optional
        .setPSampleMask(nullptr)    // Optional
        .setAlphaToCoverageEnable(false)               // Optional
        .setAlphaToOneEnable(false);    // Optional

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setMinDepthBounds(0.0f)    // Optional
        .setMaxDepthBounds(1.0f)    // Optional
        .setStencilTestEnable(false)
        .setFront({})   // Optional
        .setBack({});   // Optional

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
    pipelineLayoutInfo.setSetLayouts(m_DescriptorSetLayout)    // Optional
        .setPushConstantRanges({});     // Optional
    
    m_PipelineLayout = m_Device.createPipelineLayout(pipelineLayoutInfo);
    if (!m_PipelineLayout)  throw std::runtime_error("failed to create pipeline layout!");

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStages(shaderStagesInfo)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depthStencil)
        .setPColorBlendState(&colorBlending)
        .setPDynamicState(nullptr)      // Optional
        .setLayout(m_PipelineLayout)
        .setRenderPass(m_RenderPass)
        .setSubpass(0)
        .setBasePipelineHandle(nullptr) // OPtional
        .setBasePipelineIndex(-1);  // Optional

    auto pipeline  = m_Device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (pipeline.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline!");
    m_GraphicsPipeline = pipeline.value;

    m_Device.destroy(fragShaderModule);
    m_Device.destroy(vertShaderModule);
}

void HelloTriangleApplication::createFramebuffers()
{
    m_vecSwapchainFramebuffers.resize(m_vecSwapChainImageViews.size());
    for (size_t i = 0; i < m_vecSwapChainImageViews.size(); ++i) {
        vk::ImageView attachments[] = { m_vecSwapChainImageViews[i], m_DepthImageView };
                
        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.setRenderPass(m_RenderPass)
            .setAttachments(attachments)
            .setWidth(m_SwapChainExtent.width)
            .setHeight(m_SwapChainExtent.height)
            .setLayers(1);
        
        m_vecSwapchainFramebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
        if (!m_vecSwapchainFramebuffers[i])
            throw std::runtime_error("failed to create framebuffer!");
    }
}

void HelloTriangleApplication::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_PhysicalDevice);

    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

    m_CommandPool = m_Device.createCommandPool(poolInfo);
    if (!m_CommandPool) throw std::runtime_error("failed to create command pool!");
}

void HelloTriangleApplication::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();
    createImage(m_SwapChainExtent.width, m_SwapChainExtent.height,
        depthFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_DepthImage, m_DepthImageMemory);

    m_DepthImageView = createImageView(m_DepthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

    transitionImageLayout(m_DepthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void HelloTriangleApplication::createTextureImage()
{
    int texWidth, texHeight, texChannels;
    // stbi_uc* pixels = stbi_load("./src/texture/redPattern.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
        throw std::runtime_error("failed to load texture image!");

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;

    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);

    void* data;
    const auto&_ = m_Device.mapMemory(stagingBufferMemory, 0, imageSize, vk::MemoryMapFlags{0}, &data);
    memcpy(data, pixels, imageSize);
    m_Device.unmapMemory(stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_TextureImage, m_TextureImageMemory);

    transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, m_TextureImage, texWidth, texHeight);

    transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_Device.destroyBuffer(stagingBuffer);
    m_Device.freeMemory(stagingBufferMemory);
}

void HelloTriangleApplication::createTextureImageView()
{
    m_TextureImageView = createImageView(m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void HelloTriangleApplication::createTextureSampler()
{
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat);
    
    vk::PhysicalDeviceProperties properties = m_PhysicalDevice.getProperties();

    samplerInfo.setAnisotropyEnable(true)
        .setMaxAnisotropy(properties.limits.maxSamplerAnisotropy)
        .setBorderColor(vk::BorderColor::eIntOpaqueWhite)
        .setUnnormalizedCoordinates(false)
        .setCompareEnable(false)
        .setCompareOp(vk::CompareOp::eAlways)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMipLodBias(0.0f)
        .setMinLod(0.0f)
        .setMaxLod(0.0f);

    m_TextureSampler = m_Device.createSampler(samplerInfo);
    if (!m_TextureSampler)  throw std::runtime_error("failed to create texture sampler!");    
}

void HelloTriangleApplication::loadModel()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
        throw std::runtime_error(warn + err);

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for(const auto& shape : shapes)
    {
        for(const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = m_Vertices.size();
                m_Vertices.emplace_back(vertex);
            }

            m_Indices.emplace_back(uniqueVertices[vertex]);
        }
    }
}

void HelloTriangleApplication::createVertexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(Vertex) * m_Vertices.size();
    
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);
    
    void* data;
    const auto&_ = m_Device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags{0}, &data);
    memcpy(data, m_Vertices.data(), bufferSize);
    m_Device.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize,
        vk::BufferUsageFlagBits::eTransferDst |
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_VertexBuffer, m_VertexBufferMemory);
    
    copyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    m_Device.destroyBuffer(stagingBuffer);
    m_Device.freeMemory(stagingBufferMemory);
}

void HelloTriangleApplication::createIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, 
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory);
    
    void* data;
    const auto&_ = m_Device.mapMemory(stagingBufferMemory, 0, bufferSize, vk::MemoryMapFlags{0}, &data);
    memcpy(data, m_Indices.data(), bufferSize);
    m_Device.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize, 
        vk::BufferUsageFlagBits::eTransferDst |
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        m_IndexBuffer, m_IndexBufferMemory);
    
    copyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    m_Device.destroyBuffer(stagingBuffer);
    m_Device.freeMemory(stagingBufferMemory);
}

void HelloTriangleApplication::createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    m_vecUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_vecUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        createBuffer(bufferSize, 
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent,
            m_vecUniformBuffers[i],
            m_vecUniformBuffersMemory[i]);
}

void HelloTriangleApplication::createDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(MAX_FRAMES_IN_FLIGHT);

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setPoolSizes(poolSizes)
        .setMaxSets(MAX_FRAMES_IN_FLIGHT);

    m_DescriptorPool = m_Device.createDescriptorPool(poolInfo);
    if (!m_DescriptorPool)  throw std::runtime_error("failed to create descriptor pool!");
}

void HelloTriangleApplication::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_DescriptorPool)
        .setDescriptorSetCount(static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))
        .setSetLayouts(layouts);

    m_vecDescriptorSets = m_Device.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(m_vecUniformBuffers[i])
            .setOffset(0)
            .setRange(sizeof(UniformBufferObject));

        vk::DescriptorImageInfo imageInfo{};
        imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(m_TextureImageView)
            .setSampler(m_TextureSampler);

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].setDstSet(m_vecDescriptorSets[i])
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setBufferInfo(bufferInfo);
        
        descriptorWrites[1].setDstSet(m_vecDescriptorSets[i])
            .setDstBinding(1)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1)
            .setImageInfo(imageInfo);

        m_Device.updateDescriptorSets(descriptorWrites, nullptr);
    }
}

void HelloTriangleApplication::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(m_CommandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
    m_vecCommandBuffers = m_Device.allocateCommandBuffers(allocInfo);
}

void HelloTriangleApplication::createSyncObjects()
{
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    m_vecImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_vecRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_vecInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_vecImageAvailableSemaphores[i]= m_Device.createSemaphore(semaphoreInfo);
        m_vecRenderFinishedSemaphores[i] = m_Device.createSemaphore(semaphoreInfo);
        m_vecInFlightFences[i] = m_Device.createFence(fenceInfo);
    }
}

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

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
}

void HelloTriangleApplication::cleanupSwapChain()
{
    m_Device.destroyImageView(m_DepthImageView);
    m_Device.destroyImage(m_DepthImage);
    m_Device.freeMemory(m_DepthImageMemory);

    for (auto& framebuffer : m_vecSwapchainFramebuffers)
        m_Device.destroyFramebuffer(framebuffer);

    m_Device.destroyPipeline(m_GraphicsPipeline);
    m_Device.destroyPipelineLayout(m_PipelineLayout);
    m_Device.destroyRenderPass(m_RenderPass);

    for (auto& imageView : m_vecSwapChainImageViews)
        m_Device.destroyImageView(imageView);

    m_Device.destroySwapchainKHR(m_SwapChain);
}

bool HelloTriangleApplication::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
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

void HelloTriangleApplication::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlags{0})  // Optional
        .setPInheritanceInfo(nullptr);  // Optional
        
    commandBuffer.begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(m_RenderPass)
        .setFramebuffer(m_vecSwapchainFramebuffers[imageIndex])
        .setRenderArea(vk::Rect2D({0, 0}, m_SwapChainExtent));

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].setColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    clearValues[1].setDepthStencil({ 1.0f, 0 });
    renderPassInfo.setClearValues(clearValues);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline);

    vk::Buffer vertexBuffers[] = { m_VertexBuffer };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(m_IndexBuffer, 0, vk::IndexType::eUint32);
    
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, m_vecDescriptorSets[imageIndex], nullptr);
    commandBuffer.drawIndexed(m_Indices.size(), 1, 0, 0, 0);

    commandBuffer.endRenderPass();
    commandBuffer.end();
}

uint32_t HelloTriangleApplication::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags propertyFlags)
{
    vk::PhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        if (typeFilter & (1 << i) &&
            (memProperties.memoryTypes[i].propertyFlags & propertyFlags)) 
            { return i; }

    throw std::runtime_error("failed to find suitable memory type!");
}


void HelloTriangleApplication::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(size)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive);

    buffer = m_Device.createBuffer(bufferInfo);
    if (!buffer) throw std::runtime_error("failed to create buffer!");

    vk::MemoryRequirements memRequirements = m_Device.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));

    bufferMemory = m_Device.allocateMemory(allocInfo);
    if (!bufferMemory)  throw std::runtime_error("failed to allocate buffer memory!");

    m_Device.bindBufferMemory(buffer, bufferMemory, 0);
}

void HelloTriangleApplication::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferCopy copyRegin{};
    copyRegin.setSrcOffset(0)
        .setDstOffset(0)
        .setSize(size);

    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegin);

    endSingleTimeCommands(commandBuffer);
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), 
        static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height),
         0.01f, 10.0f);
    ubo.proj[1][1] *= -1;

    void* data;
    const auto&_ = m_Device.mapMemory(m_vecUniformBuffersMemory[currentImage], 0, sizeof(ubo), vk::MemoryMapFlags{0}, &data);
    memcpy(data, &ubo, sizeof(UniformBufferObject));
    m_Device.unmapMemory(m_vecUniformBuffersMemory[currentImage]);
}

void HelloTriangleApplication::createImage(
    uint32_t width, uint32_t height, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties, 
    vk::Image& image, vk::DeviceMemory& memory)
{
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setImageType(vk::ImageType::e2D)
        .setExtent({ width, height, 1 })
        .setMipLevels(1)
        .setArrayLayers(1)
        .setFormat(format)
        .setTiling(tiling)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setUsage(usage)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setSharingMode(vk::SharingMode::eExclusive);

    image = m_Device.createImage(imageInfo);
    if (!image) throw std::runtime_error("failed to create image!");

    vk::MemoryRequirements memRequirements = m_Device.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));

    memory = m_Device.allocateMemory(allocInfo);
    if (!memory) throw std::runtime_error("failed to allocate image memory!");

    m_Device.bindImageMemory(image, memory, 0);
}

vk::CommandBuffer HelloTriangleApplication::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_CommandPool)
        .setCommandBufferCount(1);
    
    vk::CommandBuffer commandBuffer = m_Device.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void HelloTriangleApplication::endSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(commandBuffer);

    m_GraphicsQueue.submit(submitInfo, nullptr);
    m_GraphicsQueue.waitIdle();

    m_Device.freeCommandBuffers(m_CommandPool, commandBuffer);
}

void HelloTriangleApplication::transitionImageLayout(vk::Image image, vk::Format format,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(image)
        .setSubresourceRange(vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1));
        // .setSrcAccessMask(vk::AccessFlags{0})   // TODO
        // .setDstAccessMask(vk::AccessFlags{0});  // TODO

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);
        if (hasStencilComponent(format))
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }

    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlags{0})
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined &&
                newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlags{0})
            .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead |
                vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(
        sourceStage, destinationStage,
        vk::DependencyFlags{0},
        nullptr, nullptr, barrier);
    
    endSingleTimeCommands(commandBuffer);
}

void HelloTriangleApplication::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource(vk::ImageSubresourceLayers(
            vk::ImageAspectFlagBits::eColor,
            0, 0, 1))
        .setImageOffset(vk::Offset3D(0, 0, 0))
        .setImageExtent(vk::Extent3D(width, height, 1));
    
    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    endSingleTimeCommands(commandBuffer);
}

vk::ImageView HelloTriangleApplication::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(format)
        .setSubresourceRange(vk::ImageSubresourceRange(
            aspectFlags,
            0, 1, 0, 1));
        
    vk::ImageView imageView = m_Device.createImageView(viewInfo);
    if (!imageView) throw std::runtime_error("failed to create texture image view!");

    return imageView;
}

vk::Format HelloTriangleApplication::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for (auto format : candidates)
    {
        vk::FormatProperties props = m_PhysicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features)
            return format;
    }
    
    throw std::runtime_error("failed to find unsupported format!");
}

vk::Format HelloTriangleApplication::findDepthFormat()
{
    return findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool HelloTriangleApplication::hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint ||
            format == vk::Format::eD24UnormS8Uint;
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
    
    // only reset the fence if we are submitting work
    uint32_t imageIndex = value.value;

    updateUniformBuffer(m_CurrentFrame);

    m_Device.resetFences(m_vecInFlightFences[m_CurrentFrame]);

    m_vecCommandBuffers[m_CurrentFrame].reset(vk::CommandBufferResetFlags{0});
    recordCommandBuffer(m_vecCommandBuffers[m_CurrentFrame], imageIndex);

    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = { m_vecImageAvailableSemaphores[m_CurrentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.setWaitSemaphores(waitSemaphores)
        .setWaitDstStageMask(waitStages)
        .setCommandBuffers(m_vecCommandBuffers[m_CurrentFrame]);

    vk::Semaphore signalSemaphores[] = { m_vecRenderFinishedSemaphores[m_CurrentFrame] };
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_GraphicsQueue.submit(submitInfo, m_vecInFlightFences[m_CurrentFrame]);

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(signalSemaphores);

    vk::SwapchainKHR swapChains[] = { m_SwapChain };
    presentInfo.setSwapchains(swapChains)
        .setImageIndices(imageIndex);
        // .setResults({});    // Optional

    vk::Result result = vk::Result::eSuccess;
    try
    {
        result = m_PresentQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError const&)
    {
        result = vk::Result::eErrorOutOfDateKHR;
    }
    if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
    {
        m_FramebufferResized = false;
        recreateSwapChain();
    }
    else if (result != vk::Result::eSuccess)
        throw std::runtime_error("presentKHR failed!");

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}