//
// Created by lightfield on 3/22/19.
//

#include <stdexcept>
#include <imgui.h>
#include <chrono>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <imgui_internal.h>
#include "VulkanUtil.h"
#include "Vulkan/Initializers.h"
#include "Vulkan/Debug.h"

namespace Util {
    namespace Renderer {
        VulkanUtil::VulkanUtil(bool enableValidation)
            : settings()
            , viewUpdated(false)
            , renderPass(VK_NULL_HANDLE)
            , pipelineCache(VK_NULL_HANDLE)
            , cmdPool(VK_NULL_HANDLE)
            , destWidth(0)
            , destHeight(0)
            , instance(nullptr)
            , physicalDevice(nullptr)
            , deviceFeatures()
            , deviceProperties()
            , deviceMemoryProperties()
            , device()
            , queue()
            , depthFormat()
            , submitInfo()
            , vulkanDevice(nullptr)
            , depthStencil()
            , view(nullptr)
            , maxFramesInflight(2)
            , semaphores()
            , surface()
            , glfwUtil(nullptr)
            , rotation()
            , cameraPos()
            , mousePos()
            , gamePadState()
            , xrInstanceCreateInfo({
                    .type = XR_TYPE_INSTANCE_CREATE_INFO,
                    .createFlags = 0,
                    .applicationInfo = {
                            .applicationVersion = 1,
                            .engineVersion = 1,
                            .apiVersion = XR_CURRENT_API_VERSION,
                    },
                    .enabledApiLayerCount = 0,
                    .enabledApiLayerNames = nullptr,
            })
            , openXrUtil(this, xrInstanceCreateInfo)
            , wantOpenXR(true)
            , useLegacyOpenXR(false)
        {
            depthStencil.view = VK_NULL_HANDLE;
            depthStencil.image = VK_NULL_HANDLE;
            depthStencil.mem = VK_NULL_HANDLE;
            settings.validation = enableValidation;
#ifndef XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
            useLegacyOpenXR = true;
#endif
        }

        VulkanUtil::~VulkanUtil() {
            destroyVulkan();
        }

        VulkanUtil::VulkanUtil(const VulkanUtil& rhs)
                : settings(rhs.settings)
                , viewUpdated(false)
                , renderPass(rhs.renderPass)
                , pipelineCache(rhs.pipelineCache)
                , cmdPool(rhs.cmdPool)
                , destWidth(rhs.destWidth)
                , destHeight(rhs.destHeight)
                , instance(rhs.instance)
                , physicalDevice(rhs.physicalDevice)
                , deviceFeatures(rhs.deviceFeatures)
                , deviceProperties(rhs.deviceProperties)
                , deviceMemoryProperties(rhs.deviceMemoryProperties)
                , device(rhs.device)
                , queue(rhs.queue)
                , depthFormat(rhs.depthFormat)
                , submitInfo(rhs.submitInfo)
                , vulkanDevice(rhs.vulkanDevice)
                , depthStencil(rhs.depthStencil)
                , view(rhs.view)
                , maxFramesInflight(rhs.maxFramesInflight)
                , semaphores(rhs.semaphores)
                , surface(rhs.surface)
                , glfwUtil(rhs.glfwUtil)
                , rotation(rhs.rotation)
                , cameraPos(rhs.cameraPos)
                , mousePos(rhs.mousePos)
                , gamePadState(rhs.gamePadState)
                , xrInstanceCreateInfo(rhs.xrInstanceCreateInfo)
                , openXrUtil(rhs.openXrUtil)
                , wantOpenXR(rhs.wantOpenXR)
                , useLegacyOpenXR(rhs.useLegacyOpenXR)
        {

        }


        VkResult VulkanUtil::createInstance(bool enableValidation) {
            settings.validation = enableValidation;
            VkApplicationInfo appInfo = {};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = name.c_str();
            appInfo.pEngineName = name.c_str();
            appInfo.apiVersion = apiVersion;

            std::vector<const char*> instanceExtensions = getRequiredExtensions();

            instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

            if (!enabledInstanceExtensions.empty()) {
                for (auto enabledExtension : enabledInstanceExtensions) {
                    instanceExtensions.push_back(enabledExtension);
                }
            }

            VkInstanceCreateInfo instanceCreateInfo = {};
            instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instanceCreateInfo.pNext = nullptr;
            instanceCreateInfo.pApplicationInfo = &appInfo;
            if (!instanceExtensions.empty())
            {
                if (settings.validation)
                {
                    instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
                }
                instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
                instanceCreateInfo.ppEnabledExtensionNames = &*instanceExtensions.begin();
            }
            if (settings.validation)
            {
                instanceCreateInfo.enabledLayerCount = debug::validationlayers.size();
                instanceCreateInfo.ppEnabledLayerNames = &*debug::validationlayers.begin();
            }
            VkResult retMe = VK_SUCCESS;
            if(!wantOpenXR || useLegacyOpenXR)
                retMe = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
            else if(wantOpenXR) {
                if (!openXrUtil.initOpenXR(useLegacyOpenXR)) {
                    retMe = VK_ERROR_INITIALIZATION_FAILED;
                    assert(false);
                }
            }
            return retMe;
        }

        std::string VulkanUtil::getWindowTitle() const {
            std::string _device(deviceProperties.deviceName);
            std::string windowTitle = title + " - " + _device;
            return windowTitle;
        }

        bool VulkanUtil::checkCommandBuffers() {
            for (auto& cmdBuffer : drawCmdBuffers)
            {
                if (cmdBuffer == VK_NULL_HANDLE)
                {
                    return false;
                }
            }
            return true;
        }

        void VulkanUtil::createCommandBuffers() {
            // Create one command buffer for each swap chain image and reuse for rendering
            drawCmdBuffers.resize((!wantOpenXR)?swapChain.imageCount:openXrUtil.images.size());

            VkCommandBufferAllocateInfo cmdBufAllocateInfo =
                    Initializers::commandBufferAllocateInfo(
                            cmdPool,
                            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                            static_cast<uint32_t>(drawCmdBuffers.size()));

            VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &*drawCmdBuffers.begin()))
        }

        void VulkanUtil::destroyCommandBuffers() {
            if(device)
                vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), &*drawCmdBuffers.begin());
        }

        VkCommandBuffer VulkanUtil::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
            VkCommandBuffer cmdBuffer;

            VkCommandBufferAllocateInfo cmdBufAllocateInfo =
                    Initializers::commandBufferAllocateInfo(
                            cmdPool,
                            level,
                            1);

            VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer))

            // If requested, also start the new command buffer
            if (begin)
            {
                VkCommandBufferBeginInfo cmdBufInfo = Initializers::commandBufferBeginInfo();
                VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo))
            }

            return cmdBuffer;
        }

        void VulkanUtil::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue vkQueue, bool free) {
            if (commandBuffer == VK_NULL_HANDLE)
            {
                return;
            }

            VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer))

            VkSubmitInfo vkSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            vkSubmitInfo.commandBufferCount = 1;
            vkSubmitInfo.pCommandBuffers = &commandBuffer;

            VK_CHECK_RESULT(vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, VK_NULL_HANDLE))
            VK_CHECK_RESULT(vkQueueWaitIdle(vkQueue))

            if (free)
            {
                vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
            }
        }

        void VulkanUtil::createPipelineCache() {
            VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
            VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache))

        }

        bool VulkanUtil::prepVulkan() {

            if (vulkanDevice->getEnableDebugMarkers()) {
                debugmarker::setup(device);
            }
            createCommandPool();
            setupSwapChain();
            createCommandBuffers();
            createSynchronizationPrimitives();
            setupDepthStencil();
            setupRenderPass();
            createPipelineCache();
            setupFrameBuffer();
            if (settings.overlay) {
                uiOverlay.appPtr = this;
                uiOverlay.device = vulkanDevice;
                uiOverlay.queue = queue;
                uiOverlay.shaders = {
                        loadShader("Renderer/shader-spv/uioverlay-vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT),
                        loadShader("Renderer/shader-spv/uioverlay-frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT),
                };
                uiOverlay.prepareResources();
                uiOverlay.preparePipeline(pipelineCache, renderPass);
            }
            return true;
        }

        void VulkanUtil::GiveTime(const std::chrono::high_resolution_clock::time_point& start) {
            if (viewUpdated)
            {
                viewUpdated = false;
                viewChanged();
            }

            render();
            frameCounter++;
            auto tEnd = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
            frameTimer = tEnd / 1000.0f;
            camera.update(frameTimer);
            if (camera.moving())
            {
                viewUpdated = true;
            }
            if(!paused) {
                timer += timerSpeed * frameTimer;
                if(timer > 1.0)
                {
                    timer -= 1.0f;
                }
            }
            // TODO: Cap UI overlay update rates
            updateOverlay();
        }

        // renderLoop()

        void VulkanUtil::updateOverlay() {
            if (!settings.overlay)
                return;
            uiOverlay.setContent();

            if (uiOverlay.update() || uiOverlay.updated) {
                buildCommandBuffers();
                uiOverlay.updated = false;
            }
        }

        void VulkanUtil::drawUI(uint32_t i) {
            if (settings.overlay) {
                uiOverlay.draw(i);
            }
        }

        void VulkanUtil::prepareFrame() {
            if(wantOpenXR)
                openXrUtil.beginFrame();
            else {
                // Acquire the next image from the swap chain
                VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
                // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
                if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR || ImGui::GetCurrentContext()->MovingWindow != nullptr)) {
                    windowResize();
                } else {
                    VK_CHECK_RESULT(result)
                }
            }
        }

        void VulkanUtil::submitFrame() {
            VkResult result = VK_SUCCESS;
            if(!wantOpenXR)
                result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
            if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
                if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                    // Swap chain is no longer compatible with the surface and needs to be recreated
                    windowResize();
                    return;
                } else {
                    VK_CHECK_RESULT(result)
                }
            }
            VK_CHECK_RESULT(vkQueueWaitIdle(queue))
            if(wantOpenXR)
                openXrUtil.endFrame();
        }

        bool VulkanUtil::initVulkan() {
            VkResult err;

            // If requested, we enable the default validation layers for debugging
            if (settings.validation) {
                // The report flags determine what type of messages for the layers will be displayed
                // For validating (debugging) an appplication the error and warning bits should suffice
                VkDebugReportFlagsEXT debugReportFlags =
                        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
                // Additional flags include performance info, loader and layer debug messages, etc.
                debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
            }

            // Physical device
            uint32_t gpuCount = 0;
            // Get number of available physical devices
            VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr))
            assert(gpuCount > 0);
            // Enumerate devices
            std::vector<VkPhysicalDevice> physicalDevices;
            physicalDevices.resize(gpuCount);
            err = vkEnumeratePhysicalDevices(instance, &gpuCount, &*physicalDevices.begin());
            if (err) {
                fprintf(stderr, "Could not enumerate physical devices : %s errcode = %d",
                               tools::errorString(err).c_str(), err);
                return false;
            }

            int CurBest = -1;
            for (auto _device : physicalDevices) {
                int score = rateDeviceSuitability(_device);
                if(score > CurBest) {
                    physicalDevice = _device;
                    CurBest = score;
                }
                printf(" score has %d\n", score);
            }
            // GPU selection

            // Select physical device to be used:
            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("failed to find a suitable GPU!");
            }

            // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
            vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

            printf("Selected %s as it has the highest score\n", deviceProperties.deviceName);

            // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
            getEnabledFeatures();

            // Vulkan device creation
            // This is handled by a separate class that gets a logical device representation
            // and encapsulates functions related to a device
            vulkanDevice = new Device(physicalDevice);
            VkResult res_ = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions,
                                                              deviceCreatepNextChain);
            if (res_ != VK_SUCCESS) {
                fprintf(stderr, "Could not create Vulkan device: %s, %d \n",
                               tools::errorString(res_).c_str(), res_);
                return false;
            }
            device = vulkanDevice->getLogicalDevice();

            // Get a graphics queue from the device
            vkGetDeviceQueue(device, vulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT), 0, &queue);

            // Find a suitable depth format
            VkBool32 validDepthFormat = tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
            assert(validDepthFormat);

            swapChain.connect(instance, physicalDevice, device);

            // Create synchronization objects
            syncDevices.initSemaphores(device, maxFramesInflight);
            VkSemaphoreCreateInfo semaphoreCreateInfo = Initializers::semaphoreCreateInfo();
            // Create a semaphore used to synchronize image presentation
            // Ensures that the image is displayed before we start submitting new commands to the queu
            VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete))
            // Create a semaphore used to synchronize command submission
            // Ensures that the image is not presented until all commands have been sumbited and executed
            VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete))


            // Set up submit info structure
            // Semaphores will stay the same during application lifetime
            // Command buffer submission info is set by each example
            submitInfo = Initializers::submitInfo();
            submitInfo.pWaitDstStageMask = &submitPipelineStages;
            if(!wantOpenXR) {
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &semaphores.presentComplete;
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = &semaphores.renderComplete;
            } else {
                submitInfo.waitSemaphoreCount = 0;
                submitInfo.pWaitSemaphores = nullptr;
                submitInfo.signalSemaphoreCount = 0;
                submitInfo.pSignalSemaphores = nullptr;
            }
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
            // Get Android device name and manufacturer (to display along GPU name)
    std::string androidProduct = "";
    char prop[PROP_VALUE_MAX+1];
    int len = __system_property_get("ro.product.manufacturer", prop);
    if (len > 0) {
        androidProduct += std::string(prop) + " ";
    };
    len = __system_property_get("ro.product.model", prop);
    if (len > 0) {
        androidProduct += std::string(prop);
    };
    LOGD("androidProduct = %s", androidProduct.c_str());
#endif
            return true;
        }

        void VulkanUtil::viewChanged() {

        }

        void VulkanUtil::keyPressed(uint32_t) { }

        void VulkanUtil::mouseMoved(double, double, bool &handled) { }

        void VulkanUtil::createSynchronizationPrimitives() {
            // Wait fences to sync command buffer access
            VkFenceCreateInfo fenceCreateInfo = Initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
            waitFences.resize(drawCmdBuffers.size());
            for (auto& fence : waitFences) {
                VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence))
            }
        }

        void VulkanUtil::createCommandPool() {
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice, swapChain.getSurface());

            VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
//            poolInfo.flags = 0; // Optional
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            if (vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool) != VK_SUCCESS) {
               fprintf(stderr, "failed to create command pool!");
               assert(false);
            }
        }

        void VulkanUtil::setupDepthStencil() {
            VkImageCreateInfo imageCI{};
            imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCI.imageType = VK_IMAGE_TYPE_2D;
            imageCI.format = depthFormat;
            imageCI.extent = { swapChain.getExtent().width, swapChain.getExtent().height, 1 };
            imageCI.mipLevels = 1;
            imageCI.arrayLayers = 1;
            imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image))
            VkMemoryRequirements memReqs{};
            vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

            VkMemoryAllocateInfo memAllloc{};
            memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memAllloc.allocationSize = memReqs.size;
            memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem))
            VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0))

            VkImageViewCreateInfo imageViewCI{};
            imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.image = depthStencil.image;
            imageViewCI.format = depthFormat;
            imageViewCI.subresourceRange.baseMipLevel = 0;
            imageViewCI.subresourceRange.levelCount = 1;
            imageViewCI.subresourceRange.baseArrayLayer = 0;
            imageViewCI.subresourceRange.layerCount = 1;
            imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
            if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
                imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view))
        }

        void VulkanUtil::setupFrameBuffer() {
            VkImageView attachments[2];

            // Depth/Stencil attachment is the same for all frame buffers
            attachments[1] = depthStencil.view;

            VkFramebufferCreateInfo frameBufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            frameBufferCreateInfo.pNext = nullptr;
            frameBufferCreateInfo.renderPass = renderPass;
            frameBufferCreateInfo.attachmentCount = 2;
            frameBufferCreateInfo.pAttachments = attachments;
            frameBufferCreateInfo.width = swapChain.getExtent().width;
            frameBufferCreateInfo.height = swapChain.getExtent().height;
            frameBufferCreateInfo.layers = 1;

            // Create frame buffers for every swap chain image
            if(wantOpenXR)
                swapChain.addImageViews(openXrUtil.getImages());
            frameBuffers.resize(swapChain.imageCount);
            for (uint32_t i = 0; i < frameBuffers.size(); i++)
            {
                attachments[0] = swapChain.buffers[i].view;
                VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]))
            }
        }

        void VulkanUtil::setupRenderPass() {
            std::vector<VkAttachmentDescription> attachments;
            attachments.resize(2);
            // Color attachment
            attachments[0].format = swapChain.colorFormat;
            attachments[0].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
            attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout = (settings.overlay)?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            // Depth attachment
            attachments[1].format = depthFormat;
            attachments[1].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
            attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference colorReference = {};
            colorReference.attachment = 0;
            colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthReference = {};
            depthReference.attachment = 1;
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpassDescription = {};
            subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescription.colorAttachmentCount = 1;
            subpassDescription.pColorAttachments = &colorReference;
            subpassDescription.pDepthStencilAttachment = &depthReference;
            subpassDescription.inputAttachmentCount = 0;
            subpassDescription.pInputAttachments = nullptr;
            subpassDescription.preserveAttachmentCount = 0;
            subpassDescription.pPreserveAttachments = nullptr;
            subpassDescription.pResolveAttachments = nullptr;

            // Subpass dependencies for layout transitions
            std::vector<VkSubpassDependency> dependencies;
            dependencies.resize(2);
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = &*attachments.begin();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpassDescription;
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = &*dependencies.begin();

            VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass))
        }

        void VulkanUtil::windowResize() {
            if (!prepared)
            {
                return;
            }
            prepared = false;

            // Ensure all operations on the device have been finished before destroying resources
            vkDeviceWaitIdle(device);

            // Recreate swap chain
            width = destWidth;
            height = destHeight;
            setupSwapChain();

            // Recreate the frame buffers
            vkDestroyImageView(device, depthStencil.view, nullptr);
            vkDestroyImage(device, depthStencil.image, nullptr);
            vkFreeMemory(device, depthStencil.mem, nullptr);
            setupDepthStencil();
            for(auto fb : frameBuffers) {
                vkDestroyFramebuffer(device, fb, nullptr);
            }
            setupFrameBuffer();

            if ((width > 0.0f) && (height > 0.0f)) {
                if (settings.overlay) {
                    uiOverlay.resize(swapChain.getExtent().width, swapChain.getExtent().height);
                }
            }

            // Command buffers need to be recreated as they may store
            // references to the recreated frame buffer
            destroyCommandBuffers();
            createCommandBuffers();
            buildCommandBuffers();

            vkDeviceWaitIdle(device);

            if ((width > 0.0f) && (height > 0.0f)) {
                camera.updateAspectRatio((float)swapChain.getExtent().width / (float)swapChain.getExtent().height);
            }

            // Notify derived class
            windowResized();
            viewChanged();

            prepared = true;
        }

        void VulkanUtil::handleMouseMove(int32_t x, int32_t y) {
            int32_t dx = (int32_t)mousePos.x - x;
            int32_t dy = (int32_t)mousePos.y - y;
            bool handled = false;

            if (settings.overlay) {
                handled = UIOverlay::GetWantCapture();
            }
            mouseMoved((float)x, (float)y, handled);

            if (handled) {
                mousePos = glm::vec2((float)x, (float)y);
                return;
            }
            if (mouseButtons.left) {
                rotation.x = rotation.x + dy * 1.25f * rotationSpeed;
                rotation.y = rotation.y - dx * 1.25f * rotationSpeed;
                if(camera.firstperson)
                    camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
                viewUpdated = true;
            }
            if (mouseButtons.right) {
                zoom += dy * .005f * zoomSpeed;
                camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
                viewUpdated = true;
            }
            if (mouseButtons.middle) {
                cameraPos.y = cameraPos.y - dx * 0.01f;
                cameraPos.y = cameraPos.y - dy * 0.01f;
                camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
                viewUpdated = true;
            }
            mousePos = glm::vec2((float)x, (float)y);
        }

        void VulkanUtil::setupSwapChain() {
            if(!wantOpenXR)
                swapChain.create(&width, &height, settings.vsync);
        }

        bool VulkanUtil::destroyVulkan() {
            // Clean up Vulkan resources
            syncDevices.destroySemaphores();
            swapChain.cleanup();
            if (descriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            }
            destroyCommandBuffers();
            if(renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }
            for(auto fb : frameBuffers) {
                vkDestroyFramebuffer(device, fb, nullptr);
            }

            for (auto& shaderModule : shaderModules)
            {
                vkDestroyShaderModule(device, shaderModule, nullptr);
            }
            if(depthStencil.view != nullptr) {
                vkDestroyImageView(device, depthStencil.view, nullptr);
            }
            if(depthStencil.image != nullptr) {
                vkDestroyImage(device, depthStencil.image, nullptr);
            }
            if(depthStencil.mem != nullptr) {
                vkFreeMemory(device, depthStencil.mem, nullptr);
            }

            if(pipelineCache != nullptr) {
                vkDestroyPipelineCache(device, pipelineCache, nullptr);
            }

            if(cmdPool != nullptr) {
                vkDestroyCommandPool(device, cmdPool, nullptr);
            }
            if(device) {
                vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
                vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
                for (auto &fence : waitFences) {
                    vkDestroyFence(device, fence, nullptr);
                }
            }
            if (settings.overlay) {
                uiOverlay.freeResources();
            }

            delete vulkanDevice;
            vulkanDevice = nullptr;

            if (settings.validation)
            {
                debug::freeDebugCallback(instance);
            }

            vkDestroyInstance(instance, nullptr);
            instance = nullptr;
            return true;
        }

        int VulkanUtil::rateDeviceSuitability(VkPhysicalDevice _device) {
            int score = 0;
            VkPhysicalDeviceProperties deviceProperties_{};
            VkPhysicalDeviceFeatures deviceFeatures_{};
            vkGetPhysicalDeviceProperties(_device, &deviceProperties_);
            vkGetPhysicalDeviceFeatures(_device, &deviceFeatures_);

            printf("evaluating %s", deviceProperties_.deviceName);

            // Discrete GPUs have a significant performance advantage
            if (deviceProperties_.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000.0f;
            }
            // Maximum possible size of textures affects graphics quality
            score += deviceProperties_.limits.maxImageDimension2D;

            if(!findQueueFamilies(_device, surface).wasSet && surface != VK_NULL_HANDLE)
                score = 0;

            if(score != 0 && surface != VK_NULL_HANDLE) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(_device);
                bool swapChainAdaquate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
                if(!swapChainAdaquate)
                    score = 0;
            }

            return score;
        }

        QueueFamilyIndices VulkanUtil::findQueueFamilies(VkPhysicalDevice _device, VkSurfaceKHR surface) {
            QueueFamilyIndices indicesFamilies{};
            uint32_t queueFamilyCount = 0;
            if(surface == VK_NULL_HANDLE)
                return indicesFamilies;
            vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies;
            queueFamilies.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, &*queueFamilies.begin());

            for(int i = 0; i < queueFamilies.size(); i++) {
                if(queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indicesFamilies.graphicsFamily = i;
                    indicesFamilies.graphicsSet = true;
                }
                if(queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    indicesFamilies.computeFamily = i;
                    indicesFamilies.computeSet = true;
                }
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, surface, &presentSupport);
                if(queueFamilies[i].queueCount > 0 && presentSupport) {
                    indicesFamilies.presentFamily = i;
                    indicesFamilies.presentSet = true;
                }
                if(indicesFamilies.graphicsSet && indicesFamilies.presentSet) {
                    indicesFamilies.wasSet = true;
                    break;
                }
            }
            return indicesFamilies;
        }

        SwapChainSupportDetails VulkanUtil::querySwapChainSupport(VkPhysicalDevice _device) {
            SwapChainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, nullptr);

            if (formatCount != 0) {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, &*details.formats.begin());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, &*details.presentModes.begin());
            }
            return details;
        }

        void VulkanUtil::createOpenXR() {
            // OpenXR instance
            openXRExtensions = {
                    (useLegacyOpenXR)?XR_KHR_VULKAN_ENABLE_EXTENSION_NAME:XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
            };
            std::size_t len = title.copy(xrInstanceCreateInfo.applicationInfo.applicationName, (title.size() > 127)?127:title.size());
            xrInstanceCreateInfo.applicationInfo.applicationName[len] = '\0';
            len = name.copy(xrInstanceCreateInfo.applicationInfo.engineName, (name.size() > 127)?127:name.size());
            xrInstanceCreateInfo.applicationInfo.engineName[len] = '\0';
            xrInstanceCreateInfo.enabledExtensionCount = openXRExtensions.size();
            xrInstanceCreateInfo.enabledExtensionNames = openXRExtensions.data();

            openXrUtil.instanceCreateInfo = xrInstanceCreateInfo;
            if(wantOpenXR)
                if(!openXrUtil.createOpenXRInstance()) {
                    fprintf(stderr, "couldn't create openXR instance, switching to just vulkan.\n");
                    wantOpenXR = false;
                }
        }


        void VulkanUtil::createVulkan() {
            // Vulkan instance
            VkResult err = createInstance(settings.validation);
            if (err) {
                fprintf(stderr, "Could not create Vulkan instance : %s, err %d\n",
                               tools::errorString(err).c_str(), err);
                assert(false);
            }
        }

        void VulkanUtil::finalizeSetup() {
            initVulkan();
            if(!wantOpenXR) {
                swapChain.initSurface(surface);
                surface = swapChain.getSurface();
            }
            if(wantOpenXR) {
                if(useLegacyOpenXR) {
                    if (!openXrUtil.initOpenXR(useLegacyOpenXR)) {
                        fprintf(stderr, "OpenXR initialization failed.\n");
                        assert(false);
                    }
                }

                if(!openXrUtil.setupSwapChain(swapChain)) {
                    fprintf(stderr, "OpenXR swap chain failed\n");
                    assert(false);
                }
            }
//            if (!xr_init(&xr, instance, device->physicalDevice, device->vulkanDevice,
//                         vk_device->queue_family_indices.graphics, 0)) {
//                xrg_log_e("OpenXR initialization failed.");
//            }

            prepVulkan();
            prepare();
        }

        void VulkanUtil::createOpenXRSystem(XrFormFactor formFactor) {
            openXrUtil.formFactor = formFactor;
            openXrUtil.createOpenXRSystem();
        }
    }
}
