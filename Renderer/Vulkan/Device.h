//
// Created by Steven Winston on 9/30/19.
//

#ifndef LIGHTFIELD_DEVICE_H
#define LIGHTFIELD_DEVICE_H


#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Buffers;

class Device {
public:
    explicit operator VkDevice() { return logicalDevice; };
    explicit Device(VkPhysicalDevice physicalDevice);
    ~Device();
    uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags _properties, VkBool32 *memTypeFound = nullptr);
    uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlags);
    VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, const std::vector<const char*>& enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);
    VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffers *buffer, VkDeviceSize size, void *data = nullptr);
    void copyBuffer(Buffers *src, Buffers *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);
    VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
    bool extensionSupported(const std::string & extension);
    VkDevice getLogicalDevice() const { return logicalDevice; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkPhysicalDeviceFeatures getEnabledFeatures() const { return enabledFeatures; }
    VkPhysicalDeviceProperties getProperties() const { return properties; }
    bool getEnableDebugMarkers() const { return enableDebugMarkers; }
private:
    /** @brief Physical device representation */
    VkPhysicalDevice physicalDevice;
    /** @brief Logical device representation (application's view of the device) */
    VkDevice logicalDevice;
    /** @brief Properties of the physical device including limits that the application can check against */
    VkPhysicalDeviceProperties properties;
    /** @brief Features of the physical device that an application can use to check if a feature is supported */
    VkPhysicalDeviceFeatures features;
    /** @brief Features that have been enabled for use on the physical device */
    VkPhysicalDeviceFeatures enabledFeatures;
    /** @brief Memory types and heaps of the physical device */
    VkPhysicalDeviceMemoryProperties memoryProperties;
    /** @brief Queue family properties of the physical device */
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    /** @brief List of extensions supported by the device */
    std::vector<std::string> supportedExtensions;

    /** @brief Default command pool for the graphics queue family index */
    VkCommandPool commandPool = VK_NULL_HANDLE;

    /** @brief Set to true when the debug marker extension is detected */
    bool enableDebugMarkers = false;
    struct
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    } queueFamilyIndices;
};


#endif //LIGHTFIELD_DEVICE_H
