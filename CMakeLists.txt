cmake_minimum_required(VERSION 3.23)

project(learnVulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH vendor/)

find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "VULKAN library not found!")
endif(Vulkan_FOUND)

add_subdirectory(vendor/glm)
add_subdirectory(vendor/glfw)

file(GLOB_RECURSE LearnVK_Headers "include/**.h")
file(GLOB_RECURSE LearnVK_Sources "src/**.cpp" "include/**.cpp")

add_executable(${PROJECT_NAME} ${LearnVK_Headers} ${LearnVK_Sources} ./main.cpp)

target_link_libraries(${PROJECT_NAME}
    PUBLIC glfw
    PUBLIC glm)

target_include_directories(${PROJECT_NAME}
    PUBLIC "include/"
    PUBLIC ${Vulkan_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME}
    PUBLIC ${Vulkan_LIBRARY})

if(WIN32)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
elseif(UNIX)
target_compile_options(${PROJECT_NAME} 
    PUBLIC "-pthread")
endif(UNIX)