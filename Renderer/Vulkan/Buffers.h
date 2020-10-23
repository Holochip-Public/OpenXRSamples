//
// Created by Steven Winston on 2019-09-12.
//

#ifndef LIGHTFIELD_BUFFERS_H
#define LIGHTFIELD_BUFFERS_H


#include <vulkan/vulkan.h>
#include <cassert>
#include <cstring>

class Buffers {
public:
    VkDevice device{};
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo descriptor{};
    VkDeviceSize size = 0;
    VkDeviceSize alignment = 0;
    void* mapped = nullptr;

    /** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
    VkBufferUsageFlags usageFlags{};
    /** @brief Memory propertys flags to be filled by external source at buffer creation (to query at some later point) */
    VkMemoryPropertyFlags memoryPropertyFlags{};

    /**
    * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
    *
    * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
    * @param offset (Optional) Byte offset from beginning
    *
    * @return VkResult of the buffer mapping call
    */
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
    * Unmap a mapped memory range
    *
    * @note Does not return a result as vkUnmapMemory can't fail
    */
    void unmap();

    /**
    * Attach the allocated memory block to the buffer
    *
    * @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
    *
    * @return VkResult of the bindBufferMemory call
    */
    VkResult bind(VkDeviceSize offset = 0);

    /**
    * Setup the default descriptor for this buffer
    *
    * @param size (Optional) Size of the memory range of the descriptor
    * @param offset (Optional) Byte offset from beginning
    *
    */
    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
    * Copies the specified data to the mapped buffer
    *
    * @param data Pointer to the data to copy
    * @param size Size of the data to copy in machine units
    *
    */
    void copyTo(void* data, VkDeviceSize size);

    /**
    * Flush a memory range of the buffer to make it visible to the device
    *
    * @note Only required for non-coherent memory
    *
    * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
    * @param offset (Optional) Byte offset from beginning
    *
    * @return VkResult of the flush call
    */
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
    * Invalidate a memory range of the buffer to make it visible to the host
    *
    * @note Only required for non-coherent memory
    *
    * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
    * @param offset (Optional) Byte offset from beginning
    *
    * @return VkResult of the invalidate call
    */
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    /**
    * Release all Vulkan resources held by this buffer
    */
    void destroy();
};


#endif //LIGHTFIELD_BUFFERS_H
