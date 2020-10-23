//
// Created by Steven Winston on 9/30/19.
//

#include <gli/gli.hpp>
#include "Texture.h"
#include "Device.h"
#include "Initializers.h"
#include "../VulkanUtil.h"
#include <filesystem>

void Texture::updateDescriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = imageLayout;
}

void Texture::destroy() {
    vkDestroyImageView(device->getLogicalDevice(), view, nullptr);
    vkDestroyImage(device->getLogicalDevice(), image, nullptr);
    if (sampler)
    {
        vkDestroySampler(device->getLogicalDevice(), sampler, nullptr);
    }
    vkFreeMemory(device->getLogicalDevice(), deviceMemory, nullptr);
}

void Texture2D::loadFromFile(std::string fileName, VkFormat format, Device *_device, VkQueue copyQueue,
                             VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear) {
    if (!std::filesystem::exists(fileName.c_str())) {
        fprintf(stderr, "Could not load texture from %s", fileName.c_str());
        assert(false);
    }
    gli::texture2d tex2D(gli::load(fileName.c_str()));
    assert(!tex2D.empty());

    this->device = _device;
    width = static_cast<uint32_t>(tex2D[0].extent().x);
    height = static_cast<uint32_t>(tex2D[0].extent().y);
    mipLevels = static_cast<uint32_t>(tex2D.levels());

    // Get device properites for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &formatProperties);

    // Only use linear tiling if requested (and supported by the device)
    // Support for linear tiling is mostly limited, so prefer to use
    // optimal tiling instead
    // On most implementations linear tiling will only support a very
    // limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
    VkBool32 useStaging = !forceLinear;

    VkMemoryAllocateInfo memAllocInfo = Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    if (useStaging)
    {
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = Initializers::bufferCreateInfo();
        bufferCreateInfo.size = tex2D.size();
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateBuffer(device->getLogicalDevice(), &bufferCreateInfo, nullptr, &stagingBuffer))

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(device->getLogicalDevice(), stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &stagingMemory))
        VK_CHECK_RESULT(vkBindBufferMemory(device->getLogicalDevice(), stagingBuffer, stagingMemory, 0))

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK_RESULT(vkMapMemory(device->getLogicalDevice(), stagingMemory, 0, memReqs.size, 0, (void **)&data))
        memcpy(data, tex2D.data(), tex2D.size());
        vkUnmapMemory(device->getLogicalDevice(), stagingMemory);

        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;

        for (uint32_t i = 0; i < mipLevels; i++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[i].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2D[i].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            offset += static_cast<uint32_t>(tex2D[i].size());
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = Initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { width, height, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageCreateInfo, nullptr, &image))

        vkGetImageMemoryRequirements(device->getLogicalDevice(), image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &deviceMemory))
        VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), image, deviceMemory, 0))

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        tools::setImageLayout(
                copyCmd,
                image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
                copyCmd,
                stagingBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(bufferCopyRegions.size()),
                bufferCopyRegions.data()
        );

        // Change texture image layout to shader read after all mip levels have been copied
        this->imageLayout = imageLayout;
        tools::setImageLayout(
                copyCmd,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                imageLayout,
                subresourceRange);

        device->flushCommandBuffer(copyCmd, copyQueue);

        // Clean up staging resources
        vkFreeMemory(device->getLogicalDevice(), stagingMemory, nullptr);
        vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);
    }
    else
    {
        // Prefer using optimal tiling, as linear tiling
        // may support only a small set of features
        // depending on implementation (e.g. no mip maps, only one layer, etc.)

        // Check if this support is supported for linear tiling
        assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

        VkImage mappableImage;
        VkDeviceMemory mappableMemory;

        VkImageCreateInfo imageCreateInfo = Initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent = { width, height, 1 };
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageCreateInfo.usage = imageUsageFlags;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Load mip map level 0 to linear tiling image
        VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageCreateInfo, nullptr, &mappableImage))

        // Get memory requirements for this image
        // like size and alignment
        vkGetImageMemoryRequirements(device->getLogicalDevice(), mappableImage, &memReqs);
        // Set memory allocation size to required memory size
        memAllocInfo.allocationSize = memReqs.size;

        // Get memory type that can be mapped to host memory
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Allocate host memory
        VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &mappableMemory))

        // Bind allocated image for use
        VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), mappableImage, mappableMemory, 0))

        // Get sub resource layout
        // Mip map count, array layer, etc.
        VkImageSubresource subRes = {};
        subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subRes.mipLevel = 0;

        VkSubresourceLayout subResLayout;
        void *data;

        // Get sub resources layout
        // Includes row pitch, size offsets, etc.
        vkGetImageSubresourceLayout(device->getLogicalDevice(), mappableImage, &subRes, &subResLayout);

        // Map image memory
        VK_CHECK_RESULT(vkMapMemory(device->getLogicalDevice(), mappableMemory, 0, memReqs.size, 0, &data))

        // Copy image data into memory
        memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

        vkUnmapMemory(device->getLogicalDevice(), mappableMemory);

        // Linear tiled images don't need to be staged
        // and can be directly used as textures
        image = mappableImage;
        deviceMemory = mappableMemory;
        this->imageLayout = imageLayout;

        // Setup image memory barrier
        tools::setImageLayout(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);

        device->flushCommandBuffer(copyCmd, copyQueue);
    }

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = device->getEnabledFeatures().samplerAnisotropy ? device->getProperties().limits.maxSamplerAnisotropy : 1.0f;
    samplerCreateInfo.anisotropyEnable = device->getEnabledFeatures().samplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device->getLogicalDevice(), &samplerCreateInfo, nullptr, &sampler))

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    // Linear tiling usually won't support mip maps
    // Only set mip map count if optimal tiling is used
    viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
    viewCreateInfo.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->getLogicalDevice(), &viewCreateInfo, nullptr, &view))

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void
Texture2D::fromBuffer(void *buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight,
                      Device *device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags,
                      VkImageLayout imageLayout) {
    assert(buffer);

    this->device = device;
    width = texWidth;
    height = texHeight;
    mipLevels = 1;

    VkMemoryAllocateInfo memAllocInfo = Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Initializers::bufferCreateInfo();
    bufferCreateInfo.size = bufferSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(device->getLogicalDevice(), &bufferCreateInfo, nullptr, &stagingBuffer))

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    vkGetBufferMemoryRequirements(device->getLogicalDevice(), stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &stagingMemory))
    VK_CHECK_RESULT(vkBindBufferMemory(device->getLogicalDevice(), stagingBuffer, stagingMemory, 0))

    // Copy texture data into staging buffer
    uint8_t *data;
    VK_CHECK_RESULT(vkMapMemory(device->getLogicalDevice(), stagingMemory, 0, memReqs.size, 0, (void **)&data))
    memcpy(data, buffer, bufferSize);
    vkUnmapMemory(device->getLogicalDevice(), stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;
    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageCreateInfo, nullptr, &image))

    vkGetImageMemoryRequirements(device->getLogicalDevice(), image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;

    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &deviceMemory))
    VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), image, deviceMemory, 0))

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 1;

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
    );

    // Change texture image layout to shader read after all mip levels have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Clean up staging resources
    vkFreeMemory(device->getLogicalDevice(), stagingMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = filter;
    samplerCreateInfo.minFilter = filter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    VK_CHECK_RESULT(vkCreateSampler(device->getLogicalDevice(), &samplerCreateInfo, nullptr, &sampler))

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = nullptr;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->getLogicalDevice(), &viewCreateInfo, nullptr, &view))

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void Texture2DArray::loadFromFile(const std::string & filename, VkFormat format, Device *device, VkQueue copyQueue,
                                  VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout) {
    if (!std::filesystem::exists(filename.c_str())) {
        fprintf(stderr, "Could not load texture from %s", filename.c_str());
        assert(false);
    }
    gli::texture2d_array tex2DArray(gli::load(filename.c_str()));
    assert(!tex2DArray.empty());

    this->device = device;
    width = static_cast<uint32_t>(tex2DArray.extent().x);
    height = static_cast<uint32_t>(tex2DArray.extent().y);
    layerCount = static_cast<uint32_t>(tex2DArray.layers());
    mipLevels = static_cast<uint32_t>(tex2DArray.levels());

    VkMemoryAllocateInfo memAllocInfo = Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Initializers::bufferCreateInfo();
    bufferCreateInfo.size = tex2DArray.size();
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(device->getLogicalDevice(), &bufferCreateInfo, nullptr, &stagingBuffer))

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    vkGetBufferMemoryRequirements(device->getLogicalDevice(), stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &stagingMemory))
    VK_CHECK_RESULT(vkBindBufferMemory(device->getLogicalDevice(), stagingBuffer, stagingMemory, 0))

    // Copy texture data into staging buffer
    uint8_t *data;
    VK_CHECK_RESULT(vkMapMemory(device->getLogicalDevice(), stagingMemory, 0, memReqs.size, 0, (void **)&data))
    memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));
    vkUnmapMemory(device->getLogicalDevice(), stagingMemory);

    // Setup buffer copy regions for each layer including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t layer = 0; layer < layerCount; layer++)
    {
        for (uint32_t level = 0; level < mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2DArray[layer][level].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2DArray[layer][level].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += tex2DArray[layer][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;
    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    imageCreateInfo.arrayLayers = layerCount;
    imageCreateInfo.mipLevels = mipLevels;

    VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageCreateInfo, nullptr, &image))

    vkGetImageMemoryRequirements(device->getLogicalDevice(), image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &deviceMemory))
    VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), image, deviceMemory, 0))

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = layerCount;

    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

    // Copy the layers and mip levels from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = Initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.maxAnisotropy = device->getEnabledFeatures().samplerAnisotropy ? device->getProperties().limits.maxSamplerAnisotropy : 1.0f;
    samplerCreateInfo.anisotropyEnable = device->getEnabledFeatures().samplerAnisotropy;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)mipLevels;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device->getLogicalDevice(), &samplerCreateInfo, nullptr, &sampler))

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = layerCount;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->getLogicalDevice(), &viewCreateInfo, nullptr, &view))

    // Clean up staging resources
    vkFreeMemory(device->getLogicalDevice(), stagingMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}

void TextureCubeMap::loadFromFile(const std::string &filename, VkFormat format, Device *device, VkQueue copyQueue,
                                  VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout) {
    if (!std::filesystem::exists(filename.c_str())) {
        fprintf(stderr, "Could not load texture from %s", filename.c_str());
        assert(false);
    }
    gli::texture_cube texCube(gli::load(filename.c_str()));
    assert(!texCube.empty());

    this->device = device;
    width = static_cast<uint32_t>(texCube.extent().x);
    height = static_cast<uint32_t>(texCube.extent().y);
    mipLevels = static_cast<uint32_t>(texCube.levels());

    VkMemoryAllocateInfo memAllocInfo = Initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Initializers::bufferCreateInfo();
    bufferCreateInfo.size = texCube.size();
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(device->getLogicalDevice(), &bufferCreateInfo, nullptr, &stagingBuffer))

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    vkGetBufferMemoryRequirements(device->getLogicalDevice(), stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &stagingMemory))
    VK_CHECK_RESULT(vkBindBufferMemory(device->getLogicalDevice(), stagingBuffer, stagingMemory, 0))

    // Copy texture data into staging buffer
    uint8_t *data;
    VK_CHECK_RESULT(vkMapMemory(device->getLogicalDevice(), stagingMemory, 0, memReqs.size, 0, (void **)&data))
    memcpy(data, texCube.data(), texCube.size());
    vkUnmapMemory(device->getLogicalDevice(), stagingMemory);

    // Setup buffer copy regions for each face including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t face = 0; face < 6; face++)
    {
        for (uint32_t level = 0; level < mipLevels; level++)
        {
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = face;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texCube[face][level].extent().x);
            bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].extent().y);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);

            // Increase offset into staging buffer for next level / face
            offset += texCube[face][level].size();
        }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = Initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;
    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    // Cube faces count as array layers in Vulkan
    imageCreateInfo.arrayLayers = 6;
    // This flag is required for cube map images
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


    VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageCreateInfo, nullptr, &image))

    vkGetImageMemoryRequirements(device->getLogicalDevice(), image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &deviceMemory))
    VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), image, deviceMemory, 0))

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 6;

    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

    // Copy the cube map faces from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = Initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.maxAnisotropy = device->getEnabledFeatures().samplerAnisotropy ? device->getProperties().limits.maxSamplerAnisotropy : 1.0f;
    samplerCreateInfo.anisotropyEnable = device->getEnabledFeatures().samplerAnisotropy;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = (float)mipLevels;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device->getLogicalDevice(), &samplerCreateInfo, nullptr, &sampler))

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = Initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.layerCount = 6;
    viewCreateInfo.subresourceRange.levelCount = mipLevels;
    viewCreateInfo.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->getLogicalDevice(), &viewCreateInfo, nullptr, &view))

    // Clean up staging resources
    vkFreeMemory(device->getLogicalDevice(), stagingMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
}
