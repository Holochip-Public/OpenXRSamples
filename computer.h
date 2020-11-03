//
// Created by swinston on 10/15/20.
//

#ifndef OPENXR_COMPUTER_H
#define OPENXR_COMPUTER_H

#include <Vulkan/Texture.h>
#include "VulkanUtil.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Vulkan/GLTFModel.h>

class computer : public Util::Renderer::VulkanUtil {
private:
    bool wireframe = false;

    struct ShaderData
    {
        Buffers buffer;
        struct Values
        {
            glm::mat4 projection;
            glm::mat4 model;
            glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f);
        } values;
    } shaderData;

    VkPipelineLayout pipelineLayout;
    struct Pipelines
    {
        VkPipeline solid;
        VkPipeline wireframe = VK_NULL_HANDLE;
    } pipelines;

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout matrices;
        VkDescriptorSetLayout textures;
        VkDescriptorSetLayout jointMatrices;
    } descriptorSetLayouts;
    VkDescriptorSet descriptorSet;

    Util::Renderer::vkglTF::GLTFModel glTFModel;

public:
    computer();
    ~computer() override;
    void render() override;
    void windowResized() override;

    std::vector<const char*> getRequiredExtensions() override;
    void prepare() override;

    void getEnabledFeatures() override;
    void buildCommandBuffers() override;
    void         loadAssets();
    void         setupDescriptors();
    void         preparePipelines();
    void         prepareUniformBuffers();
    void         updateUniformBuffers();
    void OnUpdateUIOverlay(Util::Renderer::UIOverlay *overlay) override;
};


#endif //OPENXR_COMPUTER_H
