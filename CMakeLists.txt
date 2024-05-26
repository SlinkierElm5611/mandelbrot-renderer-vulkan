cmake_minimum_required(VERSION 3.15)

project(mandelbrot_vulkan_renderer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED True)

add_custom_target(shaders
    COMMAND glslc -o frag.spv -fshader-stage=fragment frag.glsl
    COMMAND glslc -o vert.spv -fshader-stage=vertex vert.glsl
    DEPENDS frag.glsl vert.glsl
    BYPRODUCTS frag.spv vert.spv
    )
add_executable(mandelbrot_vulkan_renderer main.cpp)

find_package(Vulkan REQUIRED)

find_package(glfw3 REQUIRED)

add_dependencies(mandelbrot_vulkan_renderer Vulkan::Vulkan shaders glfw)

target_link_libraries(mandelbrot_vulkan_renderer Vulkan::Vulkan glfw)
