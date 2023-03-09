#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

class HelloTriangleApplication {
private:
    // GLFW members
    GLFWwindow *m_pWIndow{ nullptr };
    const uint32_t m_Width = 600;
    const uint32_t m_Height = 400;
    // vulkan members
    vk::Instance m_Instance;

public:
    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanUp();

    // vulkan functions
    void vk_createInstance();
};