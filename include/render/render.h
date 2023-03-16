#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
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
    GLFWwindow *m_pWIndow{ nullptr };
    const uint32_t m_Width = 600;
    const uint32_t m_Height = 400;
    // vulkan members
    const std::vector<const char *> m_VecValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
#ifdef NDEBUG
    const bool m_EnableValidationLayers = false;
#else
    const bool m_EnableValidationLayers = true;
#endif

    // struct
    struct QueueFamilyIndices{ 
        std::optional<uint32_t> graphicsFamily; 
        bool isComplete() { return graphicsFamily.has_value(); }    
    };

    vk::DebugUtilsMessengerEXT m_DebugMessenger;

    vk::Instance m_Instance;
    vk::PhysicalDevice m_PhysicalDevice;

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
    int rateDeviceSuitability(vk::PhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

    void createInstance();
    void pickPhysicalDevice();
    void setupDebugMessenger();
};