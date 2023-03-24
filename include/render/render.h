#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <iostream>
#include <stdexcept>

class HelloTriangleApplication {
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
    vk::Format m_SwapChainImageFormat;
    vk::Extent2D m_SwapChainExtent;
    
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

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
};