#include <iostream>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vector>
#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

class MandelbrotSetVulkan{
    private:
        uint32_t X = 1920;
        uint32_t Y = 1080;
        double m_currentOffsets[2] = {-2.0, -2.0};
        double m_currentScales[2] = {4.0, 4.0};
        uint32_t m_currentIterations = 100;
        bool isDirty = true;
        bool isButtonPressed = false;
        double lastX = 0.0;
        double lastY = 0.0;
        GLFWwindow* m_window;
        vk::SurfaceKHR m_surface;
        vk::SwapchainKHR m_swapChain;
        std::vector<vk::Image> m_images;
        std::vector<vk::ImageView> m_imageViews;
        std::vector<vk::Framebuffer> m_framebuffers;
        vk::Semaphore m_imageAvailableSemaphore;
        vk::Semaphore m_rendererSemaphore;
        vk::Instance m_instance;
        vk::PhysicalDevice m_physicalDevice;
        vk::Device m_device;
        vk::Queue m_queue;
        vk::CommandPool m_commandPool;
        vk::CommandBuffer m_commandBuffer;
        vk::DescriptorSetLayout m_descriptorSetLayout;
        vk::PipelineLayout m_pipelineLayout;
        vk::RenderPass m_renderPass;
        vk::Pipeline m_pipeline;
        vk::Buffer m_buffer;
        vk::DeviceMemory m_bufferMemory;
        void* m_mappedBuffer;
        uint32_t m_bufferSize;
        void createWindow(){
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            m_window = glfwCreateWindow(X, Y, "Mandelbrot Set", nullptr, nullptr);
            glfwSetWindowUserPointer(m_window, this);
            glfwSetScrollCallback(m_window, scrollCallback);
            glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
            glfwSetCursorPosCallback(m_window, cursorPositionCallback);
            glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
            glfwSetKeyCallback(m_window, keyCallback);
            //glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        void createInstance(){
            vk::ApplicationInfo appInfo{};
            appInfo.pApplicationName = "Mandelbrot Set";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_2;
            vk::InstanceCreateInfo createInfo{};
            createInfo.pApplicationInfo = &appInfo;
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            std::vector<const char*> extensions = {
            };
            for(uint32_t i = 0; i < glfwExtensionCount; i++){
                extensions.push_back(glfwExtensions[i]);
            }
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.ppEnabledExtensionNames = extensions.data();
            std::vector<const char*> layers = {
                //"VK_LAYER_KHRONOS_validation"
            };
            createInfo.enabledLayerCount = layers.size();
            createInfo.ppEnabledLayerNames = layers.data();
            m_instance = vk::createInstance(createInfo);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);
        }
        void pickPhysicalDevice(){
            std::vector<vk::PhysicalDevice> physicalDevices = m_instance.enumeratePhysicalDevices();
            if(physicalDevices.size() == 0){
                throw std::runtime_error("No Vulkan physical devices found");
            }
            for(const auto& physicalDevice : physicalDevices){
                vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
                if(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu){
                    m_physicalDevice = physicalDevice;
                    return;
                }
            }
            m_physicalDevice = physicalDevices[0];
        }
        void createDevice(){
            float queuePriority = 1.0f;
            vk::DeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.queueFamilyIndex = 0;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            vk::DeviceCreateInfo createInfo{};
            createInfo.queueCreateInfoCount = 1;
            createInfo.pQueueCreateInfos = &queueCreateInfo;
            std::vector<const char*> extensions = {
                VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.ppEnabledExtensionNames = extensions.data();
            m_device = m_physicalDevice.createDevice(createInfo);
            m_queue = m_device.getQueue(0, 0);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
        }
        void createCommandPool(){
            vk::CommandPoolCreateInfo createInfo{};
            createInfo.queueFamilyIndex = 0;
            m_commandPool = m_device.createCommandPool(createInfo);
        }
        void createCommandBuffer(){
            vk::CommandBufferAllocateInfo allocateInfo{};
            allocateInfo.commandPool = m_commandPool;
            allocateInfo.level = vk::CommandBufferLevel::ePrimary;
            allocateInfo.commandBufferCount = 1;
            m_commandBuffer = m_device.allocateCommandBuffers(allocateInfo)[0];
        }
        void createDescriptorSetLayout(){
            vk::DescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            binding.descriptorCount = 1;
            binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
            vk::DescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 1;
            createInfo.pBindings = &binding;
            createInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
            m_descriptorSetLayout = m_device.createDescriptorSetLayout(createInfo);
        }
        void createPipelineLayout(){
            vk::PipelineLayoutCreateInfo createInfo{};
            createInfo.setLayoutCount = 1;
            createInfo.pSetLayouts = &m_descriptorSetLayout;
            m_pipelineLayout = m_device.createPipelineLayout(createInfo);
        }
        void createRenderPass(){
            vk::AttachmentDescription attachment{};
            attachment.format = vk::Format::eB8G8R8A8Unorm;
            attachment.samples = vk::SampleCountFlagBits::e1;
            attachment.loadOp = vk::AttachmentLoadOp::eClear;
            attachment.storeOp = vk::AttachmentStoreOp::eStore;
            attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            attachment.initialLayout = vk::ImageLayout::eUndefined;
            attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
            vk::AttachmentReference colorAttachment{};
            colorAttachment.attachment = 0;
            colorAttachment.layout = vk::ImageLayout::eColorAttachmentOptimal;
            vk::SubpassDescription subpass{};
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachment;
            vk::SubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
            dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            vk::RenderPassCreateInfo createInfo{};
            createInfo.attachmentCount = 1;
            createInfo.pAttachments = &attachment;
            createInfo.subpassCount = 1;
            createInfo.pSubpasses = &subpass;
            createInfo.dependencyCount = 1;
            createInfo.pDependencies = &dependency;
            m_renderPass = m_device.createRenderPass(createInfo);
        }
        vk::ShaderModule createShaderModule(const std::string& filename){
            std::ifstream file(filename, std::ios::ate | std::ios::binary);
            if(!file.is_open()){
                throw std::runtime_error("Failed to open file");
            }
            size_t fileSize = file.tellg();
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();
            vk::ShaderModuleCreateInfo createInfo{};
            createInfo.codeSize = buffer.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
            return m_device.createShaderModule(createInfo);
        }
        void createPipeline(){
            vk::ShaderModule vertexShaderModule = createShaderModule("vert.spv");
            vk::ShaderModule fragmentShaderModule = createShaderModule("frag.spv");
            vk::PipelineShaderStageCreateInfo vertexShaderStage{};
            vertexShaderStage.stage = vk::ShaderStageFlagBits::eVertex;
            vertexShaderStage.module = vertexShaderModule;
            vertexShaderStage.pName = "main";
            vk::PipelineShaderStageCreateInfo fragmentShaderStage{};
            fragmentShaderStage.stage = vk::ShaderStageFlagBits::eFragment;
            fragmentShaderStage.module = fragmentShaderModule;
            fragmentShaderStage.pName = "main";
            vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStage, fragmentShaderStage};
            vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
            std::vector<vk::DynamicState> dynamicStates = {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor
            };
            vk::PipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.dynamicStateCount = dynamicStates.size();
            dynamicState.pDynamicStates = dynamicStates.data();
            vk::PipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = vk::PolygonMode::eFill;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = vk::CullModeFlagBits::eBack;
            rasterizer.frontFace = vk::FrontFace::eClockwise;
            rasterizer.depthBiasEnable = VK_FALSE;
            vk::PipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
            vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            vk::PipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = vk::LogicOp::eCopy;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            vk::GraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = m_pipelineLayout;
            pipelineInfo.renderPass = m_renderPass;
            m_pipeline = m_device.createGraphicsPipeline(nullptr, pipelineInfo).value;
            m_device.destroyShaderModule(fragmentShaderModule);
            m_device.destroyShaderModule(vertexShaderModule);
        }
        void createSurface(){
            VkSurfaceKHR surface;
            if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS){
                throw std::runtime_error("Failed to create window surface");
            }
            m_surface = surface;
        }
        void createSwapChain(){
            vk::SurfaceCapabilitiesKHR capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
            vk::SurfaceFormatKHR surfaceFormat = m_physicalDevice.getSurfaceFormatsKHR(m_surface)[0];
            vk::Extent2D extent = capabilities.currentExtent;
            vk::SwapchainCreateInfoKHR createInfo{};
            createInfo.surface = m_surface;
            createInfo.minImageCount = capabilities.minImageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
            createInfo.preTransform = capabilities.currentTransform;
            createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            createInfo.presentMode = vk::PresentModeKHR::eFifo;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = nullptr;
            m_swapChain = m_device.createSwapchainKHR(createInfo);
        }
        void getSwapChainImages(){
            m_images = m_device.getSwapchainImagesKHR(m_swapChain);
            m_imageViews.resize(m_images.size());
        }
        void createSwapChainImageViews(){
            for(size_t i = 0; i < m_images.size(); i++){
                vk::ImageViewCreateInfo createInfo{};
                createInfo.image = m_images[i];
                createInfo.viewType = vk::ImageViewType::e2D;
                createInfo.format = vk::Format::eB8G8R8A8Unorm;
                createInfo.components.r = vk::ComponentSwizzle::eIdentity;
                createInfo.components.g = vk::ComponentSwizzle::eIdentity;
                createInfo.components.b = vk::ComponentSwizzle::eIdentity;
                createInfo.components.a = vk::ComponentSwizzle::eIdentity;
                createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                createInfo.subresourceRange.baseMipLevel = 0;
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1;
                m_imageViews[i] = m_device.createImageView(createInfo);
            }
        }
        void createFrameBuffers(){
            m_framebuffers.resize(m_imageViews.size());
            for(size_t i = 0; i < m_imageViews.size(); i++){
                vk::FramebufferCreateInfo createInfo{};
                createInfo.renderPass = m_renderPass;
                createInfo.attachmentCount = 1;
                createInfo.pAttachments = &m_imageViews[i];
                createInfo.width = X;
                createInfo.height = Y;
                createInfo.layers = 1;
                m_framebuffers[i] = m_device.createFramebuffer(createInfo);
            }
        }
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties){
            vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();
            for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++){
                if((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties){
                    return i;
                }
            }
            throw std::runtime_error("Failed to find suitable memory type");
        }
        void createBuffer(){
            m_bufferSize = sizeof(double)*4 + sizeof(uint32_t);
            vk::BufferCreateInfo createInfo{};
            createInfo.size = m_bufferSize;
            createInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
            createInfo.sharingMode = vk::SharingMode::eExclusive;
            m_buffer = m_device.createBuffer(createInfo);
            vk::MemoryRequirements memoryRequirements = m_device.getBufferMemoryRequirements(m_buffer);
            vk::MemoryAllocateInfo allocateInfo{};
            allocateInfo.allocationSize = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            m_bufferMemory = m_device.allocateMemory(allocateInfo);
            m_device.bindBufferMemory(m_buffer, m_bufferMemory, 0);
            m_mappedBuffer = m_device.mapMemory(m_bufferMemory, 0, m_bufferSize);
        }
        void createSyncObjects(){
            vk::SemaphoreCreateInfo semaphoreInfo{};
            m_imageAvailableSemaphore = m_device.createSemaphore(semaphoreInfo);
            m_rendererSemaphore = m_device.createSemaphore(semaphoreInfo);
        }
        void drawCurrentState(){
            uint32_t imageIndex = m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, nullptr).value;
            vk::CommandBufferBeginInfo beginInfo{};
            m_commandBuffer.begin(beginInfo);
            vk::RenderPassBeginInfo renderPassInfo{};
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = m_framebuffers[imageIndex];
            renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
            renderPassInfo.renderArea.extent = vk::Extent2D{X, Y};
            vk::ClearValue clearColor = vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;
            vk::WriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.dstSet = nullptr;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = 0;
            writeDescriptorSet.descriptorType = vk::DescriptorType::eUniformBuffer;
            writeDescriptorSet.descriptorCount = 1;
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = m_bufferSize;
            writeDescriptorSet.pBufferInfo = &bufferInfo;
            char* charPtr = reinterpret_cast<char*>(m_mappedBuffer);
            std::memcpy(m_mappedBuffer, m_currentScales, sizeof(double)*2);
            std::memcpy(charPtr + sizeof(double)*2, m_currentOffsets, sizeof(double)*2);
            std::memcpy(charPtr + sizeof(double)*4, &m_currentIterations, sizeof(uint32_t));
            m_commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
            m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
            m_commandBuffer.setViewport(0, vk::Viewport{0.0f, 0.0f, float(X), float(Y), 0.0f, 1.0f});
            m_commandBuffer.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{X, Y}});
            m_commandBuffer.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &writeDescriptorSet);
            m_commandBuffer.draw(6, 1, 0, 0);
            m_commandBuffer.endRenderPass();
            m_commandBuffer.end();
            vk::SubmitInfo submitInfo{};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_commandBuffer;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
            vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_rendererSemaphore;
            m_queue.submit(submitInfo);
            vk::PresentInfoKHR presentInfo{};
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_rendererSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_swapChain;
            presentInfo.pImageIndices = &imageIndex;
            vk::Result result = m_queue.presentKHR(presentInfo);
            if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR){
                recreateSwapChain();
            }
            m_queue.waitIdle();

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
            mandelbrotSetVulkan->recreateSwapChain();
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
        void recreateSwapChain(){
            m_device.waitIdle();
            cleanupSwapChain();
            createSwapChain();
            getSwapChainImages();
            createSwapChainImageViews();
            createFrameBuffers();
        }
        void cleanupSwapChain(){
            for (size_t i = 0; i < m_images.size(); i++) {
                m_device.destroyFramebuffer(m_framebuffers[i]);
                m_device.destroyImageView(m_imageViews[i]);
            }
            m_device.destroySwapchainKHR(m_swapChain);
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
        MandelbrotSetVulkan(){
            VULKAN_HPP_DEFAULT_DISPATCHER.init();
            normalizeCoordinates();
            createWindow();
            createInstance();
            pickPhysicalDevice();
            createDevice();
            createCommandPool();
            createCommandBuffer();
            createDescriptorSetLayout();
            createPipelineLayout();
            createRenderPass();
            createPipeline();
            createSurface();
            createSwapChain();
            getSwapChainImages();
            createSwapChainImageViews();
            createFrameBuffers();
            createBuffer();
            createSyncObjects();
        }
        ~MandelbrotSetVulkan(){
            m_device.destroySemaphore(m_imageAvailableSemaphore);
            m_device.destroySemaphore(m_rendererSemaphore);
            m_device.unmapMemory(m_bufferMemory);
            m_device.destroyBuffer(m_buffer);
            m_device.freeMemory(m_bufferMemory);
            cleanupSwapChain();
            m_device.destroyPipeline(m_pipeline);
            m_device.destroyRenderPass(m_renderPass);
            m_device.destroyPipelineLayout(m_pipelineLayout);
            m_device.destroyDescriptorSetLayout(m_descriptorSetLayout);
            m_device.destroyCommandPool(m_commandPool);
            m_device.destroy();
            m_instance.destroySurfaceKHR(m_surface);
            m_instance.destroy();
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
        void run(){
            while(!glfwWindowShouldClose(m_window)){
                if(isDirty){
                    drawCurrentState();
                    isDirty = false;
                }
                glfwPollEvents();
            }
            m_device.waitIdle();
        }
};

int main(){
    MandelbrotSetVulkan mandelbrotSetVulkan;
    mandelbrotSetVulkan.run();
    return 0;
}
