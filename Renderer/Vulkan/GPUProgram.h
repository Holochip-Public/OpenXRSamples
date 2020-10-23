//
// Created by Steven Winston on 2019-09-12.
//

#ifndef LIGHTFIELD_GPUPROGRAM_H
#define LIGHTFIELD_GPUPROGRAM_H

#include <vulkan/vulkan.h>
#include <initializer_list>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct Program {
    VkPipelineBindPoint bindPoint;
    VkPipelineLayout layout;
    VkDescriptorSetLayout setLayout;
    VkDescriptorUpdateTemplate updateTemplate;
    VkShaderStageFlags pushConstantStages;
};

struct DescriptorInfo {
    union {
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
    };

    DescriptorInfo() = default;
    DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout);
    DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
    DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range);
    explicit DescriptorInfo(VkBuffer buffer_);
};

struct GPUProgram
{
    GPUProgram();
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    std::string entryPoint;

    VkDescriptorType resourceTypes[32];
    uint32_t resourceMask;

    glm::vec3 localSize;

    bool usesPushConstants;

    static bool loadShader(GPUProgram& shader, VkDevice device, const char* path);

    using Shaders = std::initializer_list<const GPUProgram*>;
    using Constants = std::initializer_list<int>;

    static VkSpecializationInfo fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const Constants& constants);
    VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, Shaders shaders, VkPipelineLayout layout, Constants constants = {});
    VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const GPUProgram& shader, VkPipelineLayout layout, Constants constants = {});

    static Program createProgram(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize, bool pushDescriptorsSupported);
    void destroyProgram(VkDevice device, const Program& program);

    static inline uint32_t getGroupCount(uint32_t threadCount, uint32_t localSize)
    {
        return (threadCount + localSize - 1) / localSize;
    }
private:
    static void parseShader(GPUProgram& shader, const uint32_t* code, uint32_t codeSize);
};

#endif //LIGHTFIELD_GPUPROGRAM_H
