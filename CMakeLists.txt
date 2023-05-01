cmake_minimum_required(VERSION 3.15)

project(learnVulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH vendor/)

find_package(Vulkan REQUIRED)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/glfw)

file(GLOB_RECURSE LearnVK_Headers "include/**.h")
file(GLOB_RECURSE LearnVK_Sources "src/**.cpp" "include/**.cpp")

add_executable(${PROJECT_NAME} ${LearnVK_Headers} ${LearnVK_Sources} ./main.cpp)

target_link_libraries(${PROJECT_NAME}
    PUBLIC glfw
    PUBLIC glm
    PUBLIC vulkan)

target_include_directories(${PROJECT_NAME}
    PUBLIC "include/")

target_compile_options(${PROJECT_NAME} 
    PUBLIC "-pthread")