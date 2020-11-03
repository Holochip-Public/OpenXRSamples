//
// Created by swinston on 11/2/20.
//

#ifndef OPENXR_XRSWAPCHAINS_H
#define OPENXR_XRSWAPCHAINS_H

#include <vulkan/vulkan.h>
#include <vector>
#include "../OpenXRUtil.h"

namespace Util {
    namespace Renderer {
        class VulkanUtil;
    }
}

class XRSwapChains {
private:
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    VkExtent2D swapchainExtent;
    Util::Renderer::VulkanUtil * app;
public:
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    /** @brief Handle to the current swap chain, required for recreation */
    std::vector<XrSwapchain> swapChain;
    std::vector<uint32_t> swapChainLength;
    uint32_t imageCount;
    std::vector<VkImage> images;
//    std::vector<SwapChainBuffer> buffers;
    /** @brief Queue family index of the detected graphics and presenting device queue */
    uint32_t queueNodeIndex;
    XRSwapChains(Util::Renderer::VulkanUtil * _app);
    VkSurfaceKHR getSurface() { return surface; }
    VkExtent2D getExtent() const { return swapchainExtent; }
    void setExtent(VkExtent2D extent) { swapchainExtent = extent; }
    void addImageViews(const std::vector<VkImage>& image);

    /** @brief Creates the platform specific surface abstraction of the native platform window used for presentation */
    void initSurface(VkSurfaceKHR surface_);
    void initOpenXR(bool useLegacy);
    void connect(VkInstance vkInstance, VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice);
    void create(uint32_t *width, uint32_t *height, bool vsync = false);
    VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex, uint32_t viewIndex);
    VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
    void cleanup();
};


#endif //OPENXR_XRSWAPCHAINS_H
