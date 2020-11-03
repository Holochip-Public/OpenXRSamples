//
// Created by Steven Winston on 9/30/19.
//

#ifndef LIGHTFIELD_INITIALIZERS_H
#define LIGHTFIELD_INITIALIZERS_H


#include <vulkan/vulkan.h>
#include <vector>

class Initializers {
public:
    static VkMemoryAllocateInfo memoryAllocateInfo();
    static VkMappedMemoryRange mappedMemoryRange();
    static VkCommandBufferAllocateInfo commandBufferAllocateInfo (VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount);
    static VkCommandPoolCreateInfo commandPoolCreateInfo();
    static VkCommandBufferBeginInfo commandBufferBeginInfo();
    static VkCommandBufferInheritanceInfo commandBufferInheritanceInfo();
    static VkRenderPassBeginInfo renderPassBeginInfo();
    static VkRenderPassCreateInfo renderPassCreateInfo();
    static VkImageMemoryBarrier imageMemoryBarrier(); // Initialize an image memory barrier with no image transfer ownership
    static VkBufferMemoryBarrier bufferMemoryBarrier(); // Initialize a buffer memory barrier with no image transfer ownership
    static VkMemoryBarrier memoryBarrier();
    static VkImageCreateInfo imageCreateInfo();
    static VkSamplerCreateInfo samplerCreateInfo();
    static VkImageViewCreateInfo imageViewCreateInfo();
    static VkFramebufferCreateInfo framebufferCreateInfo();
    static VkSemaphoreCreateInfo semaphoreCreateInfo();
    static VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
    static VkEventCreateInfo eventCreateInfo();
    static VkSubmitInfo submitInfo();
    static VkViewport viewport( float width, float height, float minDepth, float maxDepth);
    static VkRect2D rect2D( int32_t width, int32_t height, int32_t offsetX, int32_t offsetY);
    static VkBufferCreateInfo bufferCreateInfo();
    static VkBufferCreateInfo bufferCreateInfo( VkBufferUsageFlags usage, VkDeviceSize size);
    static VkDescriptorPoolCreateInfo descriptorPoolCreateInfo( uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes, uint32_t maxSets);
    static VkDescriptorPoolCreateInfo descriptorPoolCreateInfo( const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    static VkDescriptorPoolSize descriptorPoolSize( VkDescriptorType type, uint32_t descriptorCount);
    static VkDescriptorSetLayoutBinding descriptorSetLayoutBinding( VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount = 1);
    static VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo( const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount);
    static VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo( const std::vector<VkDescriptorSetLayoutBinding>& bindings );
    static VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo( const VkDescriptorSetLayout* pSetLayouts, uint32_t setLayoutCount = 1 );
    static VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo( uint32_t setLayoutCount = 1 );
    static VkDescriptorSetAllocateInfo descriptorSetAllocateInfo( VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount );
    static VkDescriptorImageInfo descriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
    static VkWriteDescriptorSet writeDescriptorSet( VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1 );
    static VkWriteDescriptorSet writeDescriptorSet( VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorImageInfo *imageInfo, uint32_t descriptorCount = 1 );
    static VkVertexInputBindingDescription vertexInputBindingDescription( uint32_t binding, uint32_t stride, VkVertexInputRate inputRate );
    static VkVertexInputAttributeDescription vertexInputAttributeDescription( uint32_t binding, uint32_t location, VkFormat format, uint32_t offset );
    static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo();
    static VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo( VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags, VkBool32 primitiveRestartEnable );
    static VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo( VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, VkPipelineRasterizationStateCreateFlags flags = 0 );
    static VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState( VkColorComponentFlags colorWriteMask, VkBool32 blendEnable );
    static VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo( uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState * pAttachments );
    static VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo( VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp );
    static VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo( uint32_t viewportCount, uint32_t scissorCount, VkPipelineViewportStateCreateFlags flags = 0 );
    static VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo( VkSampleCountFlagBits rasterizationSamples, VkPipelineMultisampleStateCreateFlags flags = 0 );
    static VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo( const VkDynamicState * pDynamicStates, uint32_t dynamicStateCount, VkPipelineDynamicStateCreateFlags flags = 0 );
    static VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo( const std::vector<VkDynamicState>& pDynamicStates, VkPipelineDynamicStateCreateFlags flags = 0 );
    static VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(uint32_t patchControlPoints);
    static VkGraphicsPipelineCreateInfo pipelineCreateInfo( VkPipelineLayout layout, VkRenderPass renderPass, VkPipelineCreateFlags flags = 0 );
    static VkGraphicsPipelineCreateInfo pipelineCreateInfo();
    static VkComputePipelineCreateInfo computePipelineCreateInfo( VkPipelineLayout layout, VkPipelineCreateFlags flags = 0 );
    static VkPushConstantRange pushConstantRange( VkShaderStageFlags stageFlags, uint32_t size, uint32_t offset );
    static VkBindSparseInfo bindSparseInfo();
    static VkSpecializationMapEntry specializationMapEntry(uint32_t constantID, uint32_t offset, size_t size); // Initialize a map entry for a shader specialization constant
    static VkSpecializationInfo specializationInfo(uint32_t mapEntryCount, const VkSpecializationMapEntry* mapEntries, size_t dataSize, const void* data); // Initialize a specialization constant info structure to pass to a shader stage
};


#endif //LIGHTFIELD_INITIALIZERS_H
