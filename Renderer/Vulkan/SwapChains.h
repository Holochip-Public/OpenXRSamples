//
// Created by Steven Winston on 2019-09-12.
//

#ifndef LIGHTFIELD_SWAPCHAINS_H
#define LIGHTFIELD_SWAPCHAINS_H

#include <vulkan/vulkan.h>
#include <vector>

namespace Util {
    namespace Renderer {
        class VulkanUtil;
    }
}

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

typedef struct _SwapChainBuffers {
    VkImage image;
    VkImageView view;
} SwapChainBuffer;

class SwapChains {
private:
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    // Function pointers
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
    PFN_vkQueuePresentKHR fpQueuePresentKHR;
    VkExtent2D swapchainExtent;
    Util::Renderer::VulkanUtil * app;
public:
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    /** @brief Handle to the current swap chain, required for recreation */
    VkSwapchainKHR swapChain;
    uint32_t imageCount;
    std::vector<VkImage> images;
    std::vector<SwapChainBuffer> buffers;
    /** @brief Queue family index of the detected graphics and presenting device queue */
    uint32_t queueNodeIndex;
    SwapChains(Util::Renderer::VulkanUtil * _app);
    VkSurfaceKHR getSurface() { return surface; }
    VkExtent2D getExtent() const { return swapchainExtent; }
    void setExtent(VkExtent2D extent) { swapchainExtent = extent; }
    void addImageViews(std::vector<VkImage> image);

    /** @brief Creates the platform specific surface abstraction of the native platform window used for presentation */
    void initSurface(VkSurfaceKHR surface_);
    void initOpenXR(bool useLegacy);
    void connect(VkInstance vkInstance, VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice);
    void create(uint32_t *width, uint32_t *height, bool vsync = false);
    VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex);
    VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
    void cleanup();
};


#endif //LIGHTFIELD_SWAPCHAINS_H
