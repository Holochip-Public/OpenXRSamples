//
// Created by swinston on 8/19/20.
//

#include "quad.h"
#include "Initializers.h"
#include "CommonHelper.h"

namespace Util {
    namespace Renderer {
        quad::quad(XrSpace space, XrExtent2Di extent, XrPosef pose, XrExtent2Df size)
        {
            uint32_t swapchainFormatCount;
            XR_CHECK_RESULT(xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, nullptr))

            int64_t swapchainFormats[swapchainFormatCount];
            XR_CHECK_RESULT(xrEnumerateSwapchainFormats(session, swapchainFormatCount,
                                                 &swapchainFormatCount, swapchainFormats))

            XrSwapchainCreateInfo swapchainCreateInfo = Initializers::createXrSwapchainCreateInfo();
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                                             XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                                             XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

            // just use the first enumerated format
            swapchainCreateInfo.format = swapchainFormats[0]; // VK_FORMAT_R8G8B8A8_SRGB  // alternative
            swapchainCreateInfo.sampleCount = 1;
            swapchainCreateInfo.faceCount = 1;
            swapchainCreateInfo.arraySize = 1;
            swapchainCreateInfo.mipCount = 1;
            swapchainCreateInfo.width = extent.width;
            swapchainCreateInfo.height = extent.height;

            XR_CHECK_RESULT(xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain))
            XR_CHECK_RESULT(xrEnumerateSwapchainImages(swapchain, 0, &swapchain_length, nullptr))

            printf("quad_swapchain_length %d", swapchain_length);

            images = new XrSwapchainImageVulkanKHR[swapchain_length];

            XR_CHECK_RESULT(xrEnumerateSwapchainImages( swapchain, swapchain_length, &swapchain_length,
                    (XrSwapchainImageBaseHeader*)images))

            printf("new quad_swapchain_length %d", swapchain_length);

            layer = {
                    .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
                    .space = space,
                    .subImage = {
                            .swapchain = swapchain,
                            .imageRect = {
                                    .offset = { .x = 0, .y = 0 },
                                    .extent = extent,
                            },
                            .imageArrayIndex = 0,
                    },
                    .pose = pose,
                    .size = size,
            };
        }

        quad::~quad() {
            delete [] images;
        }

        void quad::acquire_swapchain(uint32_t* buffer_index) {
            XrSwapchainImageAcquireInfo swapchainImageAcquireInfo = {
                    .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
            };

            XR_CHECK_RESULT(xrAcquireSwapchainImage(swapchain, &swapchainImageAcquireInfo,
                                             buffer_index))

            XrSwapchainImageWaitInfo swapchainImageWaitInfo = {
                    .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
                    .timeout = INT64_MAX,
            };
            XR_CHECK_RESULT(xrWaitSwapchainImage(swapchain, &swapchainImageWaitInfo))
        }

        bool quad::release_swapchain() {
            XrSwapchainImageReleaseInfo swapchainImageReleaseInfo = {
                    .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
            };
            XR_CHECK_RESULT(xrReleaseSwapchainImage(swapchain, &swapchainImageReleaseInfo))
        }
    }
}