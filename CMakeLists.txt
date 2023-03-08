cmake_minimum_required(VERSION 3.15)

project(learnVulkan)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} ./main.cpp)