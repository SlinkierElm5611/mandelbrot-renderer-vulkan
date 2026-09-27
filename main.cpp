#include <iostream>
#include <cstdint>
#include <stdexcept>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#include <chrono>
#include <thread>

#include "pixelKiln.h"

static const uint32_t vertexShaderSpirv[] =
#include "mandelbrot.vert.h"
;
static const uint32_t fragmentShaderSpirv[] =
#include "mandelbrot.frag.h"
;

// The RenderAreaInformation uniform block of mandelbrot.frag (std140).
struct RenderAreaInformation{
    double renderAreaSize[2];
    double renderAreaOffset[2];
    uint32_t numberOfIterations;
};

class MandelbrotSetVulkan{
    private:
        uint32_t X = 800;
        uint32_t Y = 600;
        double m_currentOffsets[2] = {-2.0, -2.0};
        double m_currentScales[2] = {4.0, 4.0};
        uint32_t m_currentIterations = 100;
        bool isDirty = true;
        bool isButtonPressed = false;
        double lastX = 0.0;
        double lastY = 0.0;
        PixelKiln m_kiln;
        GLFWwindow* m_window;
        uint64_t m_swapChain;
        uint64_t m_program;
        static Config createConfig(){
            Config config{};
            config.applicationName = "Mandelbrot Set";
            return config;
        }
        void createWindow(){
            if(!glfwInit()){
                throw std::runtime_error("Failed to initialize GLFW");
            }
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            m_window = glfwCreateWindow(X, Y, "Mandelbrot Set", nullptr, nullptr);
            if(!m_window){
                glfwTerminate();
                throw std::runtime_error("Failed to create window");
            }
            glfwSetWindowUserPointer(m_window, this);
            glfwSetScrollCallback(m_window, scrollCallback);
            glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
            glfwSetCursorPosCallback(m_window, cursorPositionCallback);
            glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
            glfwSetKeyCallback(m_window, keyCallback);
            //glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            int width = 0, height = 0;
            glfwGetFramebufferSize(m_window, &width, &height);
            X = width;
            Y = height;
        }
        NativeWindow getNativeWindow(){
#if defined(_WIN32)
            return {NATIVE_WINDOW_WIN32, GetModuleHandleW(nullptr), glfwGetWin32Window(m_window)};
#elif defined(__APPLE__)
            return {NATIVE_WINDOW_COCOA_VIEW, nullptr, glfwGetCocoaView(m_window)};
#else
            return {NATIVE_WINDOW_XLIB, glfwGetX11Display(), reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(m_window)))};
#endif
        }
        void createSwapChain(){
            SwapchainDesc desc{};
            desc.width = X;
            desc.height = Y;
            m_swapChain = m_kiln.createSwapchain(getNativeWindow(), desc);
        }
        void createProgram(){
            RasterDrawProgram program{};
            program.vertexShader = {vertexShaderSpirv, sizeof(vertexShaderSpirv)};
            program.fragmentShader = {fragmentShaderSpirv, sizeof(fragmentShaderSpirv)};
            program.uniformBindings = {UNIFORM_BINDING_TYPE_BUFFER};
            program.colorFormats = {m_kiln.getSwapchainInfo(m_swapChain).format};
            m_program = m_kiln.loadRasterDrawProgram(program, "Mandelbrot Set");
        }
        // Returns false when there was nothing to draw into (e.g. the window is minimized).
        bool drawCurrentState(){
            uint64_t image = m_kiln.acquireSwapchainImage(m_swapChain);
            if(image == 0){
                return false;
            }
            RenderAreaInformation renderAreaInformation{};
            renderAreaInformation.renderAreaSize[0] = m_currentScales[0];
            renderAreaInformation.renderAreaSize[1] = m_currentScales[1];
            renderAreaInformation.renderAreaOffset[0] = m_currentOffsets[0];
            renderAreaInformation.renderAreaOffset[1] = m_currentOffsets[1];
            renderAreaInformation.numberOfIterations = m_currentIterations;
            ProgramCall call{};
            call.type = PROGRAM_TYPE_RASTER_DRAW;
            call.program = m_program;
            call.bindings = {{&renderAreaInformation, sizeof(renderAreaInformation)}};
            call.colorTargets = {{image, true, {0.0f, 0.0f, 0.0f, 1.0f}}};
            call.vertexCount = 6;
            m_kiln.record(call);
            m_kiln.present(m_swapChain);
            return true;
        };
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset){
            MandelbrotSetVulkan* mandelbrotSetVulkan = reinterpret_cast<MandelbrotSetVulkan*>(glfwGetWindowUserPointer(window));
            double zoomFactor[2] = {(1/(2*11.0f))*mandelbrotSetVulkan->m_currentScales[0], (1/(2*11.0f))*mandelbrotSetVulkan->m_currentScales[1]};
            if(yoffset > 0){
                mandelbrotSetVulkan->m_currentOffsets[0] += zoomFactor[0];
                mandelbrotSetVulkan->m_currentOffsets[1] += zoomFactor[1];
                mandelbrotSetVulkan->m_currentScales[0] -= 2*zoomFactor[0];
                mandelbrotSetVulkan->m_currentScales[1] -= 2*zoomFactor[1];
            }else{
                mandelbrotSetVulkan->m_currentOffsets[0] -= zoomFactor[0];
                mandelbrotSetVulkan->m_currentOffsets[1] -= zoomFactor[1];
                mandelbrotSetVulkan->m_currentScales[0] += 2*zoomFactor[0];
                mandelbrotSetVulkan->m_currentScales[1] += 2*zoomFactor[1];
            }
            mandelbrotSetVulkan->isDirty = true;
        }
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
            MandelbrotSetVulkan* mandelbrotSetVulkan = reinterpret_cast<MandelbrotSetVulkan*>(glfwGetWindowUserPointer(window));
            if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS){
                mandelbrotSetVulkan->isButtonPressed = true;
            }else if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE){
                mandelbrotSetVulkan->isButtonPressed = false;
            }
        }
        static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos){
            MandelbrotSetVulkan* mandelbrotSetVulkan = reinterpret_cast<MandelbrotSetVulkan*>(glfwGetWindowUserPointer(window));
            if(mandelbrotSetVulkan->isButtonPressed){
                mandelbrotSetVulkan->m_currentOffsets[0] += (mandelbrotSetVulkan->lastX-xpos) * mandelbrotSetVulkan->m_currentScales[0] / mandelbrotSetVulkan->X;
                mandelbrotSetVulkan->m_currentOffsets[1] += (mandelbrotSetVulkan->lastY-ypos) * mandelbrotSetVulkan->m_currentScales[1] / mandelbrotSetVulkan->Y;
                mandelbrotSetVulkan->isDirty = true;
            }
            mandelbrotSetVulkan->lastX = xpos;
            mandelbrotSetVulkan->lastY = ypos;
        }
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height){
            MandelbrotSetVulkan* mandelbrotSetVulkan = reinterpret_cast<MandelbrotSetVulkan*>(glfwGetWindowUserPointer(window));
            if(width == 0 || height == 0){
                return;
            }
            mandelbrotSetVulkan->X = width;
            mandelbrotSetVulkan->Y = height;
            mandelbrotSetVulkan->normalizeCoordinates();
            mandelbrotSetVulkan->isDirty = true;
            // Applied by the next acquireSwapchainImage.
            mandelbrotSetVulkan->m_kiln.resizeSwapchain(mandelbrotSetVulkan->m_swapChain, width, height);
        }
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
            MandelbrotSetVulkan* mandelbrotSetVulkan = reinterpret_cast<MandelbrotSetVulkan*>(glfwGetWindowUserPointer(window));
            if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (key == GLFW_KEY_MINUS && action == GLFW_PRESS){
                mandelbrotSetVulkan->m_currentIterations /= 2;
                if(mandelbrotSetVulkan->m_currentIterations == 0){
                    mandelbrotSetVulkan->m_currentIterations = 1;
                }
                mandelbrotSetVulkan->isDirty = true;
            }
            if (key == GLFW_KEY_EQUAL && action == GLFW_PRESS){
                mandelbrotSetVulkan->m_currentIterations *= 2;
                mandelbrotSetVulkan->isDirty = true;
            }
            if (key == GLFW_KEY_R && action == GLFW_PRESS){
                mandelbrotSetVulkan->m_currentOffsets[0] = -2.0;
                mandelbrotSetVulkan->m_currentOffsets[1] = -2.0;
                mandelbrotSetVulkan->m_currentScales[0] = 4.0;
                mandelbrotSetVulkan->m_currentScales[1] = 4.0;
                mandelbrotSetVulkan->m_currentIterations = 100;
                mandelbrotSetVulkan->isDirty = true;
                mandelbrotSetVulkan->normalizeCoordinates();
            }
        }
        void normalizeCoordinates(){
            double aspectRatio = double(X) / double(Y);
            if(aspectRatio > m_currentScales[0] / m_currentScales[1]){
                m_currentScales[0] = m_currentScales[1] * aspectRatio;
            }else{
                m_currentScales[1] = m_currentScales[0] / aspectRatio;
            }
        }
    public:
        MandelbrotSetVulkan() : m_kiln(createConfig()){
            createWindow();
            normalizeCoordinates();
            createSwapChain();
            createProgram();
        }
        ~MandelbrotSetVulkan(){
            m_kiln.unloadProgram(m_program);
            m_kiln.destroySwapchain(m_swapChain);
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
        void run(){
            while(!glfwWindowShouldClose(m_window)){
                if(isDirty){
                    isDirty = !drawCurrentState();
                }
                glfwPollEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
            }
            m_kiln.waitIdle();
        }
};

int main(){
    try{
        MandelbrotSetVulkan mandelbrotSetVulkan;
        mandelbrotSetVulkan.run();
    }catch(const std::exception& exception){
        std::cerr << exception.what() << std::endl;
        return 1;
    }
    return 0;
}
