//
// Created by Steven Winston on 2019-09-12.
//

#include "Buffers.h"

VkResult Buffers::map(VkDeviceSize _size, VkDeviceSize offset) {
    return vkMapMemory(device, memory, offset, _size, 0, &mapped);
}

void Buffers::unmap() {
    if (mapped)
    {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
}

VkResult Buffers::bind(VkDeviceSize offset) {
    return vkBindBufferMemory(device, buffer, memory, offset);
}

void Buffers::setupDescriptor(VkDeviceSize _size, VkDeviceSize offset) {
    descriptor.offset = offset;
    descriptor.buffer = buffer;
    descriptor.range = _size;
}

void Buffers::copyTo(void *data, VkDeviceSize _size) {
    assert(mapped);
    memcpy(mapped, data, _size);
}

VkResult Buffers::flush(VkDeviceSize _size, VkDeviceSize offset) {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = offset;
    mappedRange.size = _size;
    return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
}

VkResult Buffers::invalidate(VkDeviceSize _size, VkDeviceSize offset) {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = offset;
    mappedRange.size = _size;
    return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
}

void Buffers::destroy() {
    if (buffer)
    {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory)
    {
        vkFreeMemory(device, memory, nullptr);
    }
}
