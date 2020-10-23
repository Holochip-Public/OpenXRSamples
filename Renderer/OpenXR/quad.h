//
// Created by swinston on 8/19/20.
//

#ifndef OPENXR_QUAD_H
#define OPENXR_QUAD_H

#include <vulkan/vulkan.h>
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN 1
#endif
#include <openxr/openxr_platform.h>

namespace Util {
    namespace Renderer {
        class quad {
            XrCompositionLayerQuad layer;
            XrSwapchain swapchain;
            uint32_t swapchain_length;
            XrSwapchainImageVulkanKHR* images;
            XrSession session;

        public:
            quad(XrSpace space,
                 XrExtent2Di extent,
                 XrPosef pose,
                 XrExtent2Df size);
            ~quad();
            void acquire_swapchain(uint32_t* buffer_index);
            bool release_swapchain();
        };
    }
}


#endif //OPENXR_QUAD_H
