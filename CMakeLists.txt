cmake_minimum_required(VERSION 3.28.3)

project(mandelbrot_vulkan_renderer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED True)

# Only for glslc, which compiles the shaders. Found here rather than left to PixelKiln so that Vulkan::glslc is visible
# to pixelkiln_add_shaders when it is called from this directory.
find_package(Vulkan REQUIRED COMPONENTS glslc)

# PixelKiln (git submodule external/PixelKiln) does all of the Vulkan work. EXCLUDE_FROM_ALL skips its examples, the
# library and GLFW are still built because the renderer links them.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/external/PixelKiln/CMakeLists.txt)
    message(FATAL_ERROR "external/PixelKiln is missing, run: git submodule update --init --recursive")
endif()
add_subdirectory(external/PixelKiln EXCLUDE_FROM_ALL)

# GLFW comes from PixelKiln's own submodule when it could build it, otherwise from the system.
if(NOT TARGET glfw)
    find_package(glfw3 REQUIRED)
endif()

add_executable(mandelbrot_vulkan_renderer main.cpp)

target_link_libraries(mandelbrot_vulkan_renderer PRIVATE PixelKiln glfw)

# Compiled into SPIR-V headers that main.cpp embeds, so the renderer doesn't load any files at runtime.
pixelkiln_add_shaders(mandelbrot_vulkan_renderer
    mandelbrot.vert
    mandelbrot.frag)
