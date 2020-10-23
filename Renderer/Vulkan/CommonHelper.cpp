//
// Created by Steven Winston on 3/22/20.
//

#include "CommonHelper.h"
#include "Initializers.h"
#ifdef WINDOWS
#include <direct.h>
#define GetCurDir _getcwd
#else
#include <unistd.h>
#include <fstream>

#define GetCurDir getcwd
#endif

std::vector<char> ReadShader(const std::string & fileName) {
    std::vector<char> returnMe;

    char buff[FILENAME_MAX];
    GetCurDir( buff, FILENAME_MAX );
    std::string workDir(buff);
    workDir += "/" + fileName;
    std::fstream file(workDir, std::fstream::in | std::fstream::ate);
    assert(file.is_open());
    uint64_t length = file.tellg();
    file.seekg(0);
    returnMe.resize(length);
    file.read(&*returnMe.begin(), length);
    file.close();
    return returnMe;
}

namespace tools {
    bool errorModeSilent = false;
    std::string physicalDeviceTypeString(VkPhysicalDeviceType type) {
        switch (type)
        {
#define STR_CASE(r) case VK_PHYSICAL_DEVICE_TYPE_ ##r: return #r
            STR_CASE(OTHER);
            STR_CASE(INTEGRATED_GPU);
            STR_CASE(DISCRETE_GPU);
            STR_CASE(VIRTUAL_GPU);
#undef STR_CASE
            default: return "UNKNOWN_DEVICE_TYPE";
        }
    }

    std::string errorString(VkResult errorCode) {
        switch (errorCode)
        {
#define STR_CASE(r) case VK_ ##r: return #r
            STR_CASE(NOT_READY);
            STR_CASE(TIMEOUT);
            STR_CASE(EVENT_SET);
            STR_CASE(EVENT_RESET);
            STR_CASE(INCOMPLETE);
            STR_CASE(ERROR_OUT_OF_HOST_MEMORY);
            STR_CASE(ERROR_OUT_OF_DEVICE_MEMORY);
            STR_CASE(ERROR_INITIALIZATION_FAILED);
            STR_CASE(ERROR_DEVICE_LOST);
            STR_CASE(ERROR_MEMORY_MAP_FAILED);
            STR_CASE(ERROR_LAYER_NOT_PRESENT);
            STR_CASE(ERROR_EXTENSION_NOT_PRESENT);
            STR_CASE(ERROR_FEATURE_NOT_PRESENT);
            STR_CASE(ERROR_INCOMPATIBLE_DRIVER);
            STR_CASE(ERROR_TOO_MANY_OBJECTS);
            STR_CASE(ERROR_FORMAT_NOT_SUPPORTED);
            STR_CASE(ERROR_SURFACE_LOST_KHR);
            STR_CASE(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR_CASE(SUBOPTIMAL_KHR);
            STR_CASE(ERROR_OUT_OF_DATE_KHR);
            STR_CASE(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR_CASE(ERROR_VALIDATION_FAILED_EXT);
            STR_CASE(ERROR_INVALID_SHADER_NV);
#undef STR_CASE
            default:
                return "UNKNOWN_ERROR";
        }
    }

    VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat) {
        // Since all depth formats may be optional, we need to find a suitable depth format to use
        // Start with the highest precision packed format
        std::vector<VkFormat> depthFormats = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
        };

        for (auto& format : depthFormats)
        {
            VkFormatProperties formatProps;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
            // Format must support depth stencil attachment for optimal tiling
            if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                *depthFormat = format;
                return true;
            }
        }

        return false;
    }

    void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout,
                        VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange,
                        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier = Initializers::imageMemoryBarrier();
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch (oldImageLayout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (newImageLayout)
        {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if (imageMemoryBarrier.srcAccessMask == 0)
                {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
                cmdbuffer,
                srcStageMask,
                dstStageMask,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);
    }


    void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask,
                        VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = aspectMask;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        setImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask, dstStageMask);
    }

    void insertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout,
                                  VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask,
                                  VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange) {
        VkImageMemoryBarrier imageMemoryBarrier = Initializers::imageMemoryBarrier();
        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(
                cmdbuffer,
                srcStageMask,
                dstStageMask,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);
    }

    VkShaderModule loadShader(const char *fileName, VkDevice device) {
        std::vector<char> data = ReadShader(fileName);
        VkShaderModule shaderModule;
        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = data.size();
        moduleCreateInfo.pCode = (uint32_t*)&*data.begin();

        VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule))
        return shaderModule;
    }
}
