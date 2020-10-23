//
// Created by Steven Winston on 3/22/20.
//

#ifndef LIGHTFIELDFORWARDRENDERER_COMMONHELPER_H
#define LIGHTFIELDFORWARDRENDERER_COMMONHELPER_H

#include <vulkan/vulkan.h>
#include <cassert>
#include <string>

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		fprintf(stderr, "Fatal : VkResult is \"%s\" file \"%s\" line %d", tools::errorString(res).c_str(), __FILE__, __LINE__);                 \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

namespace tools
{
    /** @brief Disable message boxes on fatal errors */
    extern bool errorModeSilent;

    /** @brief Returns an error code as a string */
    std::string errorString(VkResult errorCode);

    /** @brief Returns the device type as a string */
    std::string physicalDeviceTypeString(VkPhysicalDeviceType type);

    // Selected a suitable supported depth format starting with 32 bit down to 16 bit
    // Returns false if none of the depth formats in the list is supported by the device
    VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);

    // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    void setImageLayout(
            VkCommandBuffer cmdbuffer,
            VkImage image,
            VkImageLayout oldImageLayout,
            VkImageLayout newImageLayout,
            VkImageSubresourceRange subresourceRange,
            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // Uses a fixed sub resource layout with first mip level and layer
    void setImageLayout(
            VkCommandBuffer cmdbuffer,
            VkImage image,
            VkImageAspectFlags aspectMask,
            VkImageLayout oldImageLayout,
            VkImageLayout newImageLayout,
            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    /** @brief Inser an image memory barrier into the command buffer */
    void insertImageMemoryBarrier(
            VkCommandBuffer cmdbuffer,
            VkImage image,
            VkAccessFlags srcAccessMask,
            VkAccessFlags dstAccessMask,
            VkImageLayout oldImageLayout,
            VkImageLayout newImageLayout,
            VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask,
            VkImageSubresourceRange subresourceRange);

    // Load a SPIR-V shader (binary)
    VkShaderModule loadShader(const char *fileName, VkDevice device);
}



#endif //LIGHTFIELDFORWARDRENDERER_COMMONHELPER_H
