//
// Created by swinston on 11/2/20.
//

#include <stdexcept>
#include "XRSwapChains.h"
#include "../VulkanUtil.h"

void XRSwapChains::connect(VkInstance vkInstance, VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice) {
    instance = vkInstance;
    physicalDevice = vkPhysicalDevice;
    device = vkDevice;
}

void XRSwapChains::create(uint32_t *width, uint32_t *height, bool vsync) {
    // Get physical device surface properties and formats
    if(app->useLegacyOpenXR) {
        XrResult result;
        uint32_t swapchainFormatCount;
        if (XR_FAILED(xrEnumerateSwapchainFormats(app->openXrUtil.getSession(), 0, &swapchainFormatCount, nullptr))) {
            fprintf(stderr, "Failed to get number of supported swapchain formats\n");
            return;
        }

        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        if (XR_FAILED(xrEnumerateSwapchainFormats(app->openXrUtil.getSession(), swapchainFormatCount,
                                                  &swapchainFormatCount, swapchainFormats.data()))) {
            fprintf(stderr, "failed to enumerate swapchain formats\n");
            return;
        }

        // Get available present modes
        swapchainExtent = {};
        /* First create swapchains and query the length for each swapchain. */
        swapChain.resize(app->openXrUtil.getViewCount());
        swapChainLength.resize(app->openXrUtil.getViewCount());
        for (auto &len : swapChainLength) {
            len = 0;
        }

        for (uint32_t i = 0; i < app->openXrUtil.getViewCount(); i++) {
            // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
            // there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
            if ((swapchainFormatCount == 1) && (swapchainFormats[0] == VK_FORMAT_UNDEFINED)) {
                colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
            } else {
                // iterate over the list of available surface format and
                // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
                bool found_B8G8R8A8_UNORM = false;
                for (auto &&swapFormat : swapchainFormats) {
                    if (swapFormat == VK_FORMAT_B8G8R8A8_UNORM) {
                        colorFormat = static_cast<VkFormat>(swapFormat);
                        found_B8G8R8A8_UNORM = true;
                        break;
                    }
                }

                // in case VK_FORMAT_B8G8R8A8_UNORM is not available
                // select the first available color format
                if (!found_B8G8R8A8_UNORM) {
                    colorFormat = static_cast<VkFormat>(swapchainFormats[0]);
                }
            }

            XrSwapchainCreateInfo swapchainCreateInfo = {
                    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                    .createFlags = 0,
                    .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                                  XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                    // just use the first enumerated format
                    .format = colorFormat,
                    .sampleCount = 1,
                    .width = app->openXrUtil.getConfigViews()[i].recommendedImageRectWidth,
                    .height = app->openXrUtil.getConfigViews()[i].recommendedImageRectHeight,
                    .faceCount = 1,
                    .arraySize = 1,
                    .mipCount = 1,
            };

            printf("Swapchain %d dimensions: %dx%d\n", i,
                   app->openXrUtil.getConfigViews()[i].recommendedImageRectWidth,
                   app->openXrUtil.getConfigViews()[i].recommendedImageRectHeight);
            setExtent({app->openXrUtil.getConfigViews()[i].recommendedImageRectWidth,
                       app->openXrUtil.getConfigViews()[i].recommendedImageRectHeight});

            if (XR_FAILED(xrCreateSwapchain(app->openXrUtil.getSession(), &swapchainCreateInfo, &swapChain[i]))) {
                fprintf(stderr, "Failed to create swapchain %d!\n", i);
                return;
            }

            if (XR_FAILED(xrEnumerateSwapchainImages(swapChain[i], 0,
                                                     &swapChainLength[i], nullptr))) {
                fprintf(stderr, "Failed to enumerate swapchains\n");
                return;
            }

            *width = getExtent().width;
            *height = getExtent().height;

        }


        uint32_t maxSwapchainLength = 0;
        for (uint32_t i = 0; i < app->openXrUtil.getViewCount(); i++) {
            if (swapChainLength[i] > maxSwapchainLength) {
                maxSwapchainLength = swapChainLength[i];
            }
        }

        images.resize(app->openXrUtil.getViewCount());

        for (uint32_t i = 0; i < app->openXrUtil.getViewCount(); i++) {
            if (XR_FAILED(xrEnumerateSwapchainImages(
                    swapChain[i], swapChainLength[i],
                    &swapChainLength[i], (XrSwapchainImageBaseHeader *) &images[i]))) {
                fprintf(stderr, "Failed to enumerate swapchains\n");
                assert(false);
                return;
            }
            printf("xrEnumerateSwapchainImages: swapchain_length[%d] %d\n", i,
                   swapChainLength[i]);
        }

        app->openXrUtil.createProjectionViews(this);
    }
}

VkResult XRSwapChains::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex, uint32_t view_index) {
    // Each view has a separate swapchain which is acquired, rendered to, and released.
    const auto viewSwapchain = swapChain[view_index];

    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

    uint32_t swapchainImageIndex;
//    if(XR_FAILED(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex)))
//        return VK_ERROR_UNKNOWN;

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
//    if(XR_FAILED(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo)))
//        return VK_ERROR_UNKNOWN;

    app->openXrUtil.projectionView[view_index] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    app->openXrUtil.projectionView[view_index].pose = app->openXrUtil.getXRViews()[view_index].pose;
    app->openXrUtil.projectionView[view_index].fov = app->openXrUtil.getXRViews()[view_index].fov;
//    app->openXrUtil.projectionView[view_index].subImage.swapchain = //viewSwapchain->swapChain[view_index];
    app->openXrUtil.projectionView[view_index].subImage.imageRect.offset = {0, 0};
    app->openXrUtil.projectionView[view_index].subImage.imageRect.extent = {static_cast<int32_t>(getExtent().width), static_cast<int32_t>(getExtent().height)};

    const XrSwapchainImageBaseHeader* const swapchainImage = (XrSwapchainImageBaseHeader *) &images[swapchainImageIndex];



    return VK_SUCCESS;
}

VkResult XRSwapChains::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
//    VkPresentInfoKHR presentInfo = {};
//    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//    presentInfo.pNext = nullptr;
//    presentInfo.swapchainCount = 1;
//    presentInfo.pSwapchains = &;
//    presentInfo.pImageIndices = &imageIndex;
//    // Check if a wait semaphore has been specified to wait for before presenting the image
//    if (waitSemaphore != VK_NULL_HANDLE)
//    {
//        presentInfo.pWaitSemaphores = &waitSemaphore;
//        presentInfo.waitSemaphoreCount = 1;
//    }
//    return fpQueuePresentKHR(queue, &presentInfo);
    return VK_SUCCESS;
}

void XRSwapChains::cleanup() {
    if (!swapChain.empty())
    {
        swapChain.clear();
    }
}

XRSwapChains::XRSwapChains(Util::Renderer::VulkanUtil *_app)
        : surface(VK_NULL_HANDLE)
        , instance()
        , device()
        , physicalDevice()
        , colorFormat()
        , colorSpace()
        , swapChain(VK_NULL_HANDLE)
        , imageCount(0)
        , queueNodeIndex(UINT32_MAX)
        , swapchainExtent()
        , app(_app)
{

}

void XRSwapChains::addImageViews(const std::vector<VkImage>& image) {
    for(auto im : images) {
        images.push_back(im);
    }
    imageCount = images.size();
    // Get the swap chain buffers containing the image and imageview
//    buffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = nullptr;
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
        };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags = 0;

//        buffers[i].image = images[i];
//
//        colorAttachmentView.image = buffers[i].image;
//
//        VK_CHECK_RESULT(vkCreateImageView(device, &colorAttachmentView, nullptr, &buffers[i].view))
    }
}
