//
// Created by swinston on 10/20/20.
//

#ifndef OPENXR_PARTICLEFIRE_H
#define OPENXR_PARTICLEFIRE_H

#include <Vulkan/Texture.h>
#include <Vulkan/GLTFModel.h>
#include <random>
#include "VulkanUtil.h"

#define PARTICLE_COUNT 512
#define PARTICLE_SIZE 10.0f

#define FLAME_RADIUS 8.0f

#define PARTICLE_TYPE_FLAME 0
#define PARTICLE_TYPE_SMOKE 1

struct Particle {
    glm::vec4 pos;
    glm::vec4 color;
    float alpha;
    float size;
    float rotation;
    uint32_t type;
    // Attributes not used in shader
    glm::vec4 vel;
    float rotationSpeed;
};

class particlefire : public Util::Renderer::VulkanUtil {
    struct {
        struct {
            Texture2D smoke;
            Texture2D fire;
            // Use a custom sampler to change sampler attributes required for rotating the uvs in the shader for alpha blended textures
            VkSampler sampler;
        } particles;
        struct {
            Texture2D colorMap;
            Texture2D normalMap;
        } floor;
    } textures;

    Util::Renderer::vkglTF::GLTFModel environmentModel;
//    vkglTF::Model environmentModel;

    glm::vec3 emitterPos = glm::vec3(0.0f, -FLAME_RADIUS + 2.0f, 0.0f);
    glm::vec3 minVel = glm::vec3(-3.0f, 0.5f, -3.0f);
    glm::vec3 maxVel = glm::vec3(3.0f, 7.0f, 3.0f);

    struct {
        VkBuffer buffer;
        VkDeviceMemory memory;
        // Store the mapped address of the particle data for reuse
        void *mappedMemory;
        // Size of the particle buffer in bytes
        size_t size;
    } particles;

    struct {
        Buffers fire;
        Buffers environment;
    } uniformBuffers;

    struct UBOVS {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec2 viewportDim;
        float pointSize = PARTICLE_SIZE;
    } uboVS;

    struct UBOEnv {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::mat4 normal;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    } uboEnv;

    struct {
        VkPipeline particles;
        VkPipeline environment;
    } pipelines;

    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;

    struct {
        VkDescriptorSet particles;
        VkDescriptorSet environment;
    } descriptorSets;

    std::vector<Particle> particleBuffer;

    std::random_device rndDevice;
    std::default_random_engine rndEngine;

public:
    particlefire();
    ~particlefire() override;
    void getEnabledFeatures() override;
    void buildCommandBuffers() override;
    std::vector<const char*> getRequiredExtensions() override;
    void windowResized() override;
    float rnd(float range);
    void initParticle(Particle *particle, glm::vec3 emitterPos);
    void transitionParticle(Particle *particle);
    void prepareParticles();
    void updateParticles();
    void loadAssets();
    void setupDescriptorPool();
    void setupDescriptorSetLayout();
    void setupDescriptorSets();
    void preparePipelines();
    void prepareUniformBuffers();
    void updateUniformBufferLight();
    void updateUniformBuffers();
    void draw();
    void prepare() override;
    void render() override;
    void OnUpdateUIOverlay(Util::Renderer::UIOverlay *overlay) override;
};


#endif //OPENXR_PARTICLEFIRE_H
