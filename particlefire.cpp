//
// Created by swinston on 10/20/20.
//

#include <Vulkan/Initializers.h>
#include <filesystem>
#include "particlefire.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

particlefire::particlefire()
: Util::Renderer::VulkanUtil(true)
, textures()
, particles()
, pipelines()
, pipelineLayout()
, descriptorSetLayout()
, descriptorSets()
, rndEngine(rndDevice())
{
    title = "CPU based particle system";
    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -75.0f));
    camera.setRotation(glm::vec3(-15.0f, 45.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
    settings.overlay = true;
    timerSpeed *= 8.0f;
}

particlefire::~particlefire() {
    textures.particles.smoke.destroy();
    textures.particles.fire.destroy();
    textures.floor.colorMap.destroy();
    textures.floor.normalMap.destroy();

    vkDestroyPipeline(device, pipelines.particles, nullptr);
    vkDestroyPipeline(device, pipelines.environment, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    vkUnmapMemory(device, particles.memory);
    vkDestroyBuffer(device, particles.buffer, nullptr);
    vkFreeMemory(device, particles.memory, nullptr);

    uniformBuffers.environment.destroy();
    uniformBuffers.fire.destroy();

    vkDestroySampler(device, textures.particles.sampler, nullptr);
}

void particlefire::getEnabledFeatures() {
    if (deviceFeatures.samplerAnisotropy) {
        enabledFeatures.samplerAnisotropy = VK_TRUE;
    }
}

void particlefire::buildCommandBuffers() {
    VkCommandBufferBeginInfo cmdBufInfo = Initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = Initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = (wantOpenXR)?swapChain.getExtent().width:width;
    renderPassBeginInfo.renderArea.extent.height = (wantOpenXR)?swapChain.getExtent().height:height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        // Set target frame buffer
        renderPassBeginInfo.framebuffer = frameBuffers[i];

        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo))

        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = Initializers::viewport((float)renderPassBeginInfo.renderArea.extent.width, (float)height, 0.0f, 1.0f);
        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = Initializers::rect2D(renderPassBeginInfo.renderArea.extent.width, renderPassBeginInfo.renderArea.extent.height, 0,0);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        VkDeviceSize offsets[1] = { 0 };

        // Environment
        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.environment, 0, nullptr);
        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.environment);
        environmentModel.draw(drawCmdBuffers[i]);

        // Particle system (no index buffer)
        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.particles, 0, nullptr);
        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.particles);
        vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &particles.buffer, offsets);
        vkCmdDraw(drawCmdBuffers[i], PARTICLE_COUNT, 1, 0, 0);

        vkCmdEndRenderPass(drawCmdBuffers[i]);

        drawUI(i);

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]))
    }
}

std::vector<const char *> particlefire::getRequiredExtensions() {
    std::vector<const char *> returnMe = Util::Renderer::GLFWUtil::getRequiredExtensions();
    if (settings.validation)
        returnMe.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return returnMe;
}

void particlefire::windowResized() { }

float particlefire::rnd(float range) {
    std::uniform_real_distribution<float> rndDist(0.0f, range);
    return rndDist(rndEngine);
}

void particlefire::initParticle(Particle *particle, glm::vec3 _emitterPos) {
    particle->vel = glm::vec4(0.0f, minVel.y + rnd(maxVel.y - minVel.y), 0.0f, 0.0f);
    particle->alpha = rnd(0.75f);
    particle->size = 1.0f + rnd(0.5f);
    particle->color = glm::vec4(1.0f);
    particle->type = PARTICLE_TYPE_FLAME;
    particle->rotation = rnd(2.0f * float(M_PI));
    particle->rotationSpeed = rnd(2.0f) - rnd(2.0f);

    // Get random sphere point
    float theta = rnd(2.0f * float(M_PI));
    float phi = rnd(float(M_PI)) - float(M_PI) / 2.0f;
    float r = rnd(FLAME_RADIUS);

    particle->pos.x = r * cosf(theta) * cosf(phi);
    particle->pos.y = r * sinf(phi);
    particle->pos.z = r * sinf(theta) * cosf(phi);

    particle->pos += glm::vec4(_emitterPos, 0.0f);
}

void particlefire::transitionParticle(Particle *particle) {
    switch (particle->type)
    {
        case PARTICLE_TYPE_FLAME:
            // Flame particles have a chance of turning into smoke
            if (rnd(1.0f) < 0.05f)
            {
                particle->alpha = 0.0f;
                particle->color = glm::vec4(0.25f + rnd(0.25f));
                particle->pos.x *= 0.5f;
                particle->pos.z *= 0.5f;
                particle->vel = glm::vec4(rnd(1.0f) - rnd(1.0f), (minVel.y * 2) + rnd(maxVel.y - minVel.y), rnd(1.0f) - rnd(1.0f), 0.0f);
                particle->size = 1.0f + rnd(0.5f);
                particle->rotationSpeed = rnd(1.0f) - rnd(1.0f);
                particle->type = PARTICLE_TYPE_SMOKE;
            }
            else
            {
                initParticle(particle, emitterPos);
            }
            break;
        case PARTICLE_TYPE_SMOKE:
            // Respawn at end of life
            initParticle(particle, emitterPos);
            break;
    }
}

void particlefire::prepareParticles() {
    particleBuffer.resize(PARTICLE_COUNT);
    for (auto& particle : particleBuffer)
    {
        initParticle(&particle, emitterPos);
        particle.alpha = 1.0f - (fabsf(particle.pos.y) / (FLAME_RADIUS * 2.0f));
    }

    particles.size = particleBuffer.size() * sizeof(Particle);

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            particles.size,
            &particles.buffer,
            &particles.memory,
            particleBuffer.data()))

    // Map the memory and store the pointer for reuse
    VK_CHECK_RESULT(vkMapMemory(device, particles.memory, 0, particles.size, 0, &particles.mappedMemory))
}

void particlefire::updateParticles() {
    float particleTimer = frameTimer * 0.045f;
    for (auto& particle : particleBuffer)
    {
        switch (particle.type)
        {
            case PARTICLE_TYPE_FLAME:
                particle.pos.y -= particle.vel.y * particleTimer * 3.5f;
                particle.alpha += particleTimer * 2.5f;
                particle.size -= particleTimer * 0.5f;
                break;
            case PARTICLE_TYPE_SMOKE:
                particle.pos -= particle.vel * frameTimer * 0.8f;
                particle.alpha += particleTimer * 1.25f;
                particle.size += particleTimer * 0.125f;
                particle.color -= particleTimer * 0.05f;
                break;
        }
        particle.rotation += particleTimer * particle.rotationSpeed;
        // Transition particle state
        if (particle.alpha > 2.0f)
        {
            transitionParticle(&particle);
        }
    }
    size_t size = particleBuffer.size() * sizeof(Particle);
    memcpy(particles.mappedMemory, particleBuffer.data(), size);
}

void particlefire::loadAssets() {

    std::string workDir = std::filesystem::current_path().native();
    workDir += "/../data/";
    // Particles
    textures.particles.smoke.loadFromFile(workDir + "textures/particle_smoke.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
    textures.particles.fire.loadFromFile(workDir + "textures/particle_fire.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

    // Floor
    textures.floor.colorMap.loadFromFile(workDir + "textures/fireplace_colormap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
    textures.floor.normalMap.loadFromFile(workDir + "textures/fireplace_normalmap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

    // Create a custom sampler to be used with the particle textures
    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = Initializers::samplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // Different address mode
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    // Both particle textures have the same number of mip maps
    samplerCreateInfo.maxLod = float(textures.particles.fire.mipLevels);

    if (vulkanDevice->getEnabledFeatures().samplerAnisotropy)
    {
        // Enable anisotropic filtering
        samplerCreateInfo.maxAnisotropy = 8.0f;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
    }

    // Use a different border color (than the normal texture loader) for additive blending
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &textures.particles.sampler))

    const uint32_t glTFLoadingFlags = static_cast<uint32_t >(Util::Renderer::vkglTF::FileLoadingFlags::PreTransformVertices) | Util::Renderer::vkglTF::FileLoadingFlags::PreMultiplyVertexColors | Util::Renderer::vkglTF::FileLoadingFlags::FlipY;
    environmentModel.loadFromFile(workDir + "models/fireplace.gltf", vulkanDevice, queue, glTFLoadingFlags);
}

void particlefire::setupDescriptorPool() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
            Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
            Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4)
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = Initializers::descriptorPoolCreateInfo(poolSizes, 2);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool))
}

void particlefire::setupDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            // Binding 1 : Fragment shader image sampler
            Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            // Binding 1 : Fragment shader image sampler
            Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,2)
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout = Initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout))

    VkPipelineLayoutCreateInfo pipelineLayoutCI = Initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout))
}

void particlefire::setupDescriptorSets() {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    VkDescriptorSetAllocateInfo allocInfo = Initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.particles))

    // Image descriptor for the color map texture
    VkDescriptorImageInfo texDescriptorSmoke =
            Initializers::descriptorImageInfo(
                    textures.particles.sampler,
                    textures.particles.smoke.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo texDescriptorFire =
            Initializers::descriptorImageInfo(
                    textures.particles.sampler,
                    textures.particles.fire.view,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    writeDescriptorSets = {
            // Binding 0: Vertex shader uniform buffer
            Initializers::writeDescriptorSet(descriptorSets.particles, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.fire.descriptor),
            // Binding 1: Smoke texture
            Initializers::writeDescriptorSet(descriptorSets.particles, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorSmoke),
            // Binding 1: Fire texture array
            Initializers::writeDescriptorSet(descriptorSets.particles, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorFire)
    };
    vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

    // Environment
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.environment))

    writeDescriptorSets = {
            // Binding 0: Vertex shader uniform buffer
            Initializers::writeDescriptorSet(descriptorSets.environment, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.environment.descriptor),
            // Binding 1: Color map
            Initializers::writeDescriptorSet(descriptorSets.environment, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.floor.colorMap.descriptor),
            // Binding 2: Normal map
            Initializers::writeDescriptorSet(descriptorSets.environment, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.floor.normalMap.descriptor),
    };
    vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void particlefire::preparePipelines() {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = Initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState = Initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = Initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

    VkGraphicsPipelineCreateInfo pipelineCI = Initializers::pipelineCreateInfo(pipelineLayout, renderPass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = shaderStages.size();
    pipelineCI.pStages = shaderStages.data();

    // Particle rendering pipeline
    {
        // Vertex input state
        VkVertexInputBindingDescription vertexInputBinding =
                Initializers::vertexInputBindingDescription(0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX);

        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
                Initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(Particle, pos)),	// Location 0: Position
                Initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT,	offsetof(Particle, color)),	// Location 1: Color
                Initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32_SFLOAT, offsetof(Particle, alpha)),			// Location 2: Alpha
                Initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32_SFLOAT, offsetof(Particle, size)),			// Location 3: Size
                Initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(Particle, rotation)),		// Location 4: Rotation
                Initializers::vertexInputAttributeDescription(0, 5, VK_FORMAT_R32_SINT, offsetof(Particle, type)),				// Location 5: Particle type
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState = Initializers::pipelineVertexInputStateCreateInfo();
        vertexInputState.vertexBindingDescriptionCount = 1;
        vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
        vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

        pipelineCI.pVertexInputState = &vertexInputState;

        // Don t' write to depth buffer
        depthStencilState.depthWriteEnable = VK_FALSE;

        // Premulitplied alpha
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        shaderStages[0] = loadShader("shader-spv/particle-vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader("shader-spv/particle-frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.particles))
    }

    // Environment rendering pipeline (normal mapped)
    {
        // Vertex input state is taken from the glTF model loader
        pipelineCI.pVertexInputState = Util::Renderer::vkglTF::Vertex::getPipelineVertexInputState(
                {
                        Util::Renderer::vkglTF::VertexComponent::Position,
                        Util::Renderer::vkglTF::VertexComponent::UV,
                        Util::Renderer::vkglTF::VertexComponent::Normal,
                        Util::Renderer::vkglTF::VertexComponent::Tangent
                });

        blendAttachmentState.blendEnable = VK_FALSE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        shaderStages[0] = loadShader("shader-spv/normalmap-vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader( "shader-spv/normalmap-frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.environment))
    }
}

void particlefire::prepareUniformBuffers() {
    // Vertex shader uniform buffer block
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.fire,
            sizeof(uboVS)))

    // Vertex shader uniform buffer block
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.environment,
            sizeof(uboEnv)))

    // Map persistent
    VK_CHECK_RESULT(uniformBuffers.fire.map())
    VK_CHECK_RESULT(uniformBuffers.environment.map())

    updateUniformBuffers();
}

void particlefire::updateUniformBufferLight() {
    // Environment
    uboEnv.lightPos.x = sinf(timer * 2.0f * float(M_PI)) * 1.5f;
    uboEnv.lightPos.y = 0.0f;
    uboEnv.lightPos.z = cosf(timer * 2.0f * float(M_PI)) * 1.5f;
    memcpy(uniformBuffers.environment.mapped, &uboEnv, sizeof(uboEnv));
}

void particlefire::updateUniformBuffers() {
    // Particle system fire
    uboVS.projection = camera.matrices.perspective;
    uboVS.modelView = camera.matrices.view;
    uboVS.viewportDim = glm::vec2((float)width, (float)height);
    memcpy(uniformBuffers.fire.mapped, &uboVS, sizeof(uboVS));

    // Environment
    uboEnv.projection = camera.matrices.perspective;
    uboEnv.modelView = camera.matrices.view;
    uboEnv.normal = glm::inverseTranspose(uboEnv.modelView);
    memcpy(uniformBuffers.environment.mapped, &uboEnv, sizeof(uboEnv));
}

void particlefire::draw() {
    VulkanUtil::prepareFrame();

    // Command buffer to be submitted to the queue
    std::vector<VkCommandBuffer> buffers { drawCmdBuffers[currentBuffer] };
    if(settings.overlay)
        buffers.push_back(uiOverlay.uiCmdBuffers[currentBuffer]);
    submitInfo.commandBufferCount = (settings.overlay)?2:1;
    submitInfo.pCommandBuffers = &*buffers.begin();
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE))

    VulkanUtil::submitFrame();
}

void particlefire::prepare() {
    loadAssets();
    prepareParticles();
    prepareUniformBuffers();
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSets();
    buildCommandBuffers();
    prepared = true;
}

void particlefire::render() {
    if (!prepared)
        return;
    draw();
    if (!paused)
    {
        updateUniformBufferLight();
        updateParticles();
    }
    if (camera.updated)
    {
        updateUniformBuffers();
    }
}

void particlefire::OnUpdateUIOverlay(Util::Renderer::UIOverlay *overlay) {
}
