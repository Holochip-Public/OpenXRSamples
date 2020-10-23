//
// Created by swinston on 8/19/20.
//


#include <vulkan/vulkan.h>
#include "Initializers.h"

XrSwapchainCreateInfo Initializers::createXrSwapchainCreateInfo() {
    return { XR_TYPE_SWAPCHAIN_CREATE_INFO, nullptr, 0,  0, VK_FORMAT_UNDEFINED};
}
