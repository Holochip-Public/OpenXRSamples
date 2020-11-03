/*
* OpenXR Example
* Vulkan Example - CPU based fire particle system
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
* Changes to enable OpenXR Copyright (C) 2020 by Holochip Inc. - Steven Winston
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/


#ifndef LIGHTFIELD_VULKANUTIL_H
#define LIGHTFIELD_VULKANUTIL_H

#include <vulkan/vulkan.h>
#include "Vulkan/SwapChains.h"
#include "Vulkan/UIOverlay.h"
#include "Vulkan/Device.h"
#include "Vulkan/CommonHelper.h"
#include "Vulkan/GPUSemaphores.h"
#include "Camera.hpp"
#include "GLFWUtil.h"
#include "OpenXRUtil.h"
#include "OpenXR/XRSwapChains.h"

#include <chrono>

// Custom define for better code readability
#define VK_FLAGS_NONE 0

namespace Util {
    namespace Renderer {
        struct QueueFamilyIndices {
            uint32_t graphicsFamily;
            uint32_t computeFamily;
            uint32_t presentFamily;
            bool wasSet;
            bool graphicsSet;
            bool computeSet;
            bool presentSet;
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        class VulkanUtil {
            friend class GLFWUtil;
            friend class UIOverlay;
        private:
            int rateDeviceSuitability(VkPhysicalDevice _device);
            static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice _device, VkSurfaceKHR surface);
            GPUSemaphores syncDevices;
        protected:
            struct {
                // Swap chain image presentation
                VkSemaphore presentComplete;
                // Command buffer submission and execution
                VkSemaphore renderComplete;
            } semaphores;
            std::vector<VkFence> waitFences;
            // Get window title with example name, device, et.
            std::string getWindowTitle() const;
            // Destination dimensions for resizing the window
            uint32_t destWidth;
            uint32_t destHeight;
            /** brief Indicates that the view (position, rotation) has changed and buffers containing camera matrices need to be updated */
            bool viewUpdated;
            // Called if the window is resized and some resources have to be recreated
            void windowResize();
            void handleMouseMove(int32_t x, int32_t y);
            uint32_t maxFramesInflight;
            // Frame counter to display fps
            uint32_t frameCounter = 0;
            std::chrono::steady_clock::time_point lastTimestamp;
            // Vulkan instance, stores all per-application states
            VkInstance instance;
            // Physical device (GPU) that Vulkan will ise
            VkPhysicalDevice physicalDevice;
            // Stores physical device properties (for e.g. checking device limits)
            VkPhysicalDeviceProperties deviceProperties;
            // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
            VkPhysicalDeviceFeatures deviceFeatures;
            // Stores all available memory (type) properties for the physical device
            VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
            /** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
            VkPhysicalDeviceFeatures enabledFeatures{};
            /** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
            std::vector<const char*> enabledDeviceExtensions;
            std::vector<const char*> enabledInstanceExtensions;
            /** @brief Optional pNext structure for passing extension structures to device creation */
            void* deviceCreatepNextChain = nullptr;
            /** @brief Logical device, application's view of the physical device (GPU) */
            VkDevice device;
            // Handle to the device graphics queue that command buffers are submitted to
            VkQueue queue;
            // Depth buffer format (selected during Vulkan initialization)
            VkFormat depthFormat;
            // Command buffer pool
            VkCommandPool cmdPool;
            /** @brief Pipeline stages used to wait at for graphics queue submissions */
            VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            // Contains command buffers and semaphores to be presented to the queue
            VkSubmitInfo submitInfo;
            // Command buffers used for rendering
            std::vector<VkCommandBuffer> drawCmdBuffers;
            // Global render pass for frame buffer writes
            VkRenderPass renderPass;
            // List of available frame buffers (same as number of swap chain images)
            std::vector<VkFramebuffer> frameBuffers;
            // Active frame buffer index
            uint32_t currentBuffer = 0;
            // Descriptor set pool
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
            // List of shader modules created (stored for cleanup)
            std::vector<VkShaderModule> shaderModules;
            // Pipeline cache object
            VkPipelineCache pipelineCache;
            // Wraps the swap chain to present images (framebuffers) to the windowing system
            SwapChains swapChain;
            XRSwapChains xrSwapChains;
            // surface
            VkSurfaceKHR surface;
            Util::Renderer::GLFWUtil * glfwUtil;

        public:
            OpenXRUtil openXrUtil;
            std::vector<const char*> openXRExtensions;
            float lastFPS = 0;
            bool prepared = false;
            uint32_t width = 1280;
            uint32_t height = 720;

            UIOverlay uiOverlay;
            XrInstanceCreateInfo xrInstanceCreateInfo;
            bool wantOpenXR;
            bool useLegacyOpenXR;

            /** @brief Last frame time measured using a high performance timer (if available) */
            float frameTimer = 1.0f;

//            vks::vkBenchmark benchmark;  /// @TODO investigate this.

            /** @brief Encapsulated physical and logical vulkan device */
            Device *vulkanDevice;

            /** @brief Example settings that can be changed e.g. by command line arguments */
            struct Settings {
                /** @brief Activates validation layers (and message output) when set to true */
                bool validation = false;
                /** @brief Set to true if fullscreen mode has been requested via command line */
                bool fullscreen = false;
                /** @brief Set to true if v-sync will be forced for the swapchain */
                bool vsync = false;
                /** @brief Enable UI overlay */
                bool overlay = false;
            } settings;

            VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

            float zoom = 0;

            static std::vector<const char*> args;

            // Defines a frame rate independent timer value clamped from -1.0...1.0
            // For use in animations, rotations, etc.
            float timer = 0.0f;
            // Multiplier for speeding up (or slowing down) the global timer
            float timerSpeed = 0.25f;

            bool paused = false;
            bool displayFPS = true;

            // Use to adjust mouse rotation speed
            float rotationSpeed = 1.0f;
            // Use to adjust mouse zoom speed
            float zoomSpeed = 1.0f;

            Camera camera;

            glm::vec3 rotation;
            glm::vec3 cameraPos;
            glm::vec2 mousePos;

            std::string title = "Vulkan Engine";
            std::string name = "vulkanEngine";
            uint32_t apiVersion = VK_API_VERSION_1_1;

            struct
            {
                VkImage image;
                VkDeviceMemory mem;
                VkImageView view;
            } depthStencil;

            struct {
                glm::vec2 axisLeft;
                glm::vec2 axisRight;
            } gamePadState;

            struct {
                bool left = false;
                bool right = false;
                bool middle = false;
            } mouseButtons;

            void* view;
            // Default ctor
            explicit VulkanUtil(bool enableValidation = false);

            // dtor
            virtual ~VulkanUtil();

            // copy constructor
            VulkanUtil(const VulkanUtil& rhs);

            // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
            bool initVulkan();
            bool prepVulkan();
            bool destroyVulkan();

            /**
            * Create the application wide Vulkan instance
            *
            */
            VkResult createInstance(bool enableValidation);

            void GiveTime(const std::chrono::high_resolution_clock::time_point& start);
            // Pure virtual render function (override in derived class)
            virtual void render() = 0;
            // Called when view change occurs
            // Can be overriden in derived class to e.g. update uniform buffers
            // Containing view dependant matrices
            virtual void viewChanged();
            /** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
            virtual void keyPressed(uint32_t);
            /** @brief (Virtual) Called after th mouse cursor moved and before internal events (like camera rotation) is handled */
            virtual void mouseMoved(double x, double y, bool &handled);
            // Called when the window has been resized
            // Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
            virtual void windowResized() { }
            // Pure virtual function to be over-ridden by the device class
            // Called in case of an event where e.g. the framebuffer has to be rebuild and thus
            // all command buffers that may reference this
            virtual void buildCommandBuffers() = 0;

            void createSynchronizationPrimitives();

            // Creates a new (graphics) command pool object storing command buffers
            void createCommandPool();
            // Setup default depth and stencil views
            virtual void setupDepthStencil();
            // Create framebuffers for all requested swap chain images
            // Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
            virtual void setupFrameBuffer();
            // Setup a default render pass
            // Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
            virtual void setupRenderPass();

            /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
            virtual void getEnabledFeatures() {}

            // Create swap chain images
            void setupSwapChain();

            // Check if command buffers are valid (!= VK_NULL_HANDLE)
            bool checkCommandBuffers();
            // Create command buffers for drawing commands
            void createCommandBuffers();
            // Destroy all command buffers and set their handles to VK_NULL_HANDLE
            // May be necessary during runtime if options are toggled
            void destroyCommandBuffers();

            // Command buffer creation
            // Creates and returns a new command buffer
            VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
            // End the command buffer, submit it to the queue and free (if requested)
            // Note : Waits for the queue to become idle
            void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

            // Create a cache pool for rendering pipelines
            void createPipelineCache();

            // Prepare commonly used Vulkan functions
            virtual void prepare() = 0;

            // Load a SPIR-V shader
            VkPipelineShaderStageCreateInfo loadShader(const std::string& fileName, const char* EntryPoint, VkShaderStageFlagBits stage) {
                VkPipelineShaderStageCreateInfo shaderStage = {};
                shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
                shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device);
#else
                shaderStage.module = tools::loadShader(fileName.c_str(), device);
#endif
                shaderStage.pName = EntryPoint; // todo : make param
                assert(shaderStage.module != VK_NULL_HANDLE);
                shaderModules.push_back(shaderStage.module);
                return shaderStage;
            }

            void updateOverlay();
            void drawUI(uint32_t i);

            // Prepare the frame for workload submission
            // - Acquires the next image from the swap chain
            // - Sets the default wait and signal semaphores
            void prepareFrame();

            // Submit the frames' workload
            void submitFrame();

            /** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
            virtual void OnUpdateUIOverlay(UIOverlay *overlay) {}

            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
            virtual std::vector<const char*> getRequiredExtensions() = 0; // must ensure that at least the surface creation extension is set.
            void createOpenXR();
            void createVulkan();
            void createOpenXRSystem(XrFormFactor formFactor);
            void setSurface(VkSurfaceKHR _surface) {surface = _surface; }
            void finalizeSetup();
            VkInstance getInstance() const { return instance; }
            void setGLFWUtil(Util::Renderer::GLFWUtil* util) { glfwUtil = util; }
            GLFWUtil* getGLFWUtil() const { return glfwUtil; }

        };
    }
}
#endif //LIGHTFIELD_VULKANUTIL_H
