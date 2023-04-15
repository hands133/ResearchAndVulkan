#pragma once

#include <cstddef>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <array>
#include <optional>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <stdexcept>

struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};

        bindingDescription.setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].setBinding(0)
            .setLocation(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(Vertex, pos));
        
        attributeDescriptions[1].setBinding(0)
            .setLocation(1)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(Vertex, color));
        return attributeDescriptions;
    }
};

// const std::vector<Vertex> vertices = {
//     { {  0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
//     { {  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } },
//     { { -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }
// };

// const std::vector<Vertex> vertices = {
//     { {  0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
//     { {  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } },
//     { { -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }
// };

const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } }
};

const std::vector<uint16_t> indices = {
    0, 1, 2,
    2, 3, 0
};

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
        file.close();
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

class HelloTriangleApplication {
public:
    bool m_FramebufferResized = false;
private:
    // GLFW members
    GLFWwindow *m_pWindow{ nullptr };
    const uint32_t m_Width = 600;
    const uint32_t m_Height = 400;
    // vulkan members
    const std::vector<const char *> m_vecValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> m_vecDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif
    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_CurrentFrame = 0;

    // struct
    struct QueueFamilyIndices{ 
        std::optional<uint32_t> graphicsFamily; 
        std::optional<uint32_t> presentFamily;

        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }    
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    vk::DebugUtilsMessengerEXT m_DebugMessenger;

    vk::Instance m_Instance;
    vk::PhysicalDevice m_PhysicalDevice;
    vk::Device m_Device;

    vk::Queue m_GraphicsQueue;
    vk::Queue m_PresentQueue;

    vk::SurfaceKHR m_Surface;

    vk::SwapchainKHR m_SwapChain;
    std::vector<vk::Image> m_vecSwapChainImages;
    std::vector<vk::ImageView> m_vecSwapChainImageViews;
    vk::Format m_SwapChainImageFormat;
    vk::Extent2D m_SwapChainExtent;
    vk::RenderPass m_RenderPass;
    vk::PipelineLayout m_PipelineLayout;

    vk::Pipeline m_GraphicsPipeline;
    std::vector<vk::Framebuffer> m_vecSwapchainFramebuffers;

    vk::CommandPool m_CommandPool;

    std::vector<vk::CommandBuffer> m_vecCommandBuffers;
    
    std::vector<vk::Semaphore> m_vecImageAvailableSemaphores;
    std::vector<vk::Semaphore> m_vecRenderFinishedSemaphores;
    std::vector<vk::Fence> m_vecInFlightFences;

    vk::Buffer m_VertexBuffer;
    vk::DeviceMemory m_VertexBufferMemory;
    vk::Buffer m_IndexBuffer;
    vk::DeviceMemory m_IndexBufferMemory;

public:
    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanUp();

    // vulkan functions
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData);
    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo);

    bool isDeviceSuitable(vk::PhysicalDevice device);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    int rateDeviceSuitability(vk::PhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    vk::ShaderModule createShaderModule(const std::vector<char>& code);
    void recordCommandBuffer(vk::CommandBuffer, uint32_t imageIndex);

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags propertyFlags);

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);
    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
    
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapChain();
    void cleanupSwapChain();


    // render functions
    void drawFrame();
};