/*
* Vulkan Example - glTF skinned animation
*
* Copyright (C) 2020 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
 * Shows how to load and display an animated scene from a glTF file using vertex skinning
 * See the accompanying README.md for a short tutorial on the data structures and functions required for vertex skinning
 *
 * For details on how glTF 2.0 works, see the official spec at https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
 *
 * If you are looking for a complete glTF implementation, check out https://github.com/SaschaWillems/Vulkan-glTF-PBR/
 */

#include <Vulkan/Initializers.h>
#include <filesystem>
#include "computer.h"

/*

	Vulkan Example class

*/

computer::computer()
: VulkanUtil(true)
, pipelineLayout(nullptr)
, descriptorSetLayouts()
, descriptorSet(nullptr)
{
    title        = "computer";
    camera.type  = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -75.0f));
    camera.setRotation(glm::vec3(-15.0f, 45.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
    settings.overlay = false;
}

computer::~computer()
{
    vkDestroyPipeline(device, pipelines.solid, nullptr);
    if (pipelines.wireframe != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipelines.wireframe, nullptr);
    }

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.jointMatrices, nullptr);

    shaderData.buffer.destroy();
}

void computer::getEnabledFeatures()
{
    // Fill mode non solid is required for wire frame display
    if (deviceFeatures.fillModeNonSolid)
    {
        enabledFeatures.fillModeNonSolid = VK_TRUE;
    }
}

void computer::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = Initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo    = Initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass               = renderPass;
    renderPassBeginInfo.renderArea.offset.x      = 0;
    renderPassBeginInfo.renderArea.offset.y      = 0;
    renderPassBeginInfo.renderArea.extent.width = (wantOpenXR)?swapChain.getExtent().width:width;
    renderPassBeginInfo.renderArea.extent.height = (wantOpenXR)?swapChain.getExtent().height:height;
    renderPassBeginInfo.clearValueCount          = 2;
    renderPassBeginInfo.pClearValues             = clearValues;

    const VkViewport viewport = Initializers::viewport((float) width, (float) height, 0.0f, 1.0f);
    const VkRect2D   scissor  = Initializers::rect2D(width, height, 0, 0);

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        renderPassBeginInfo.framebuffer = frameBuffers[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo))
        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
        // Bind scene matrices descriptor to set 0
//        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
//        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);
        glTFModel.draw(drawCmdBuffers[i], 0, pipelineLayout);
        vkCmdEndRenderPass(drawCmdBuffers[i]);
        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]))
    }
}

void computer::setupDescriptors()
{
    /*
        This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
    */

    std::vector<VkDescriptorPoolSize> poolSizes = {
            Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
            // One combined image sampler per material image/texture
//            Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFModel.images.size())),
    };
    // Number of descriptor sets = One for the scene ubo + one per image + one per skin
//    const uint32_t             maxSetCount        = static_cast<uint32_t>(glTFModel.images.size()) + 1;
//    VkDescriptorPoolCreateInfo descriptorPoolInfo = Initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
//    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool))

    // Descriptor set layouts
    VkDescriptorSetLayoutBinding    setLayoutBinding{};
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = Initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);

    // Descriptor set layout for passing matrices
    setLayoutBinding = Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices))

    // Descriptor set layout for passing material textures
    setLayoutBinding = Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures))

    // Descriptor set layout for passing skin joint matrices
    setLayoutBinding = Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.jointMatrices))

    // The pipeline layout uses three sets:
    // Set 0 = Scene matrices (VS)
    // Set 1 = Joint matrices (VS)
    // Set 2 = Material texture (FS)
    std::array<VkDescriptorSetLayout, 3> setLayouts = {
            descriptorSetLayouts.matrices,
            descriptorSetLayouts.jointMatrices,
            descriptorSetLayouts.textures};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));

    // We will use push constants to push the local matrices of a primitive to the vertex shader
    VkPushConstantRange pushConstantRange = Initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    // Push constant ranges are part of the pipeline layout
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges    = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout))

    // Descriptor set for scene matrices
//    VkDescriptorSetAllocateInfo allocInfo = Initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
//    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet))
//    VkWriteDescriptorSet writeDescriptorSet = Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
//    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void computer::preparePipelines()
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI   = Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI   = Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState    blendAttachmentStateCI = Initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo    colorBlendStateCI      = Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
    VkPipelineDepthStencilStateCreateInfo  depthStencilStateCI    = Initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo      viewportStateCI        = Initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo   multisampleStateCI     = Initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState>      dynamicStateEnables    = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo       dynamicStateCI         = Initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            Initializers::vertexInputBindingDescription(0, sizeof(Util::Renderer::vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, color)},
            // POI: Per-Vertex Joint indices and weights are passed to the vertex shader
//            {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, jointIndices)},
//            {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Util::Renderer::vkglTF::Vertex, jointWeights)},
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = Initializers::pipelineVertexInputStateCreateInfo();
    vertexInputStateCI.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputStateCI.pVertexBindingDescriptions           = vertexInputBindings.data();
    vertexInputStateCI.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions         = vertexInputAttributes.data();

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            loadShader("shader-spv/skinnedmodel-vert.spv", "main", VK_SHADER_STAGE_VERTEX_BIT),
            loadShader("shader-spv/skinnedmodel-frag.spv", "main", VK_SHADER_STAGE_FRAGMENT_BIT)};

    VkGraphicsPipelineCreateInfo pipelineCI = Initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
    pipelineCI.pVertexInputState            = &vertexInputStateCI;
    pipelineCI.pInputAssemblyState          = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState          = &rasterizationStateCI;
    pipelineCI.pColorBlendState             = &colorBlendStateCI;
    pipelineCI.pMultisampleState            = &multisampleStateCI;
    pipelineCI.pViewportState               = &viewportStateCI;
    pipelineCI.pDepthStencilState           = &depthStencilStateCI;
    pipelineCI.pDynamicState                = &dynamicStateCI;
    pipelineCI.stageCount                   = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages                      = shaderStages.data();

    // Solid rendering pipeline
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid))

    // Wire frame rendering pipeline
    if (deviceFeatures.fillModeNonSolid)
    {
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
        rasterizationStateCI.lineWidth   = 1.0f;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe))
    }
}

void computer::prepareUniformBuffers()
{
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &shaderData.buffer, sizeof(shaderData.values)))
    VK_CHECK_RESULT(shaderData.buffer.map())
    updateUniformBuffers();
}

void computer::updateUniformBuffers()
{
    shaderData.values.projection = camera.matrices.perspective;
    shaderData.values.model      = camera.matrices.view;
    memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
}

void computer::loadAssets()
{
    std::string workDir;
    workDir = std::filesystem::current_path().string();
    workDir += "/../data/models/texturedPC.glb";
    const uint32_t glTFLoadingFlags = static_cast<uint32_t>(Util::Renderer::vkglTF::FileLoadingFlags::PreTransformVertices) | Util::Renderer::vkglTF::FileLoadingFlags::PreMultiplyVertexColors | Util::Renderer::vkglTF::FileLoadingFlags::FlipY;
    glTFModel.loadFromFile(workDir, vulkanDevice, queue, glTFLoadingFlags);
}

void computer::prepare()
{
    loadAssets();
    prepareUniformBuffers();
    setupDescriptors();
    preparePipelines();
    buildCommandBuffers();
    prepared = true;
}

void computer::render()
{
    VulkanUtil::prepareFrame();
    if (camera.updated)
    {
        updateUniformBuffers();
    }
    // POI: Advance animation
    if (!paused)
    {
        glTFModel.draw(drawCmdBuffers[currentBuffer]);
    }
    std::vector<VkCommandBuffer> buffers { drawCmdBuffers[currentBuffer], uiOverlay.uiCmdBuffers[currentBuffer]};
    submitInfo.commandBufferCount = 2;
    submitInfo.pCommandBuffers = &*buffers.begin();
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE))

    VulkanUtil::submitFrame();
}

void computer::OnUpdateUIOverlay(Util::Renderer::UIOverlay *overlay) {
    if (Util::Renderer::UIOverlay::header("Settings"))
    {
        if (overlay->checkBox("Wireframe", &wireframe))
        {
            buildCommandBuffers();
        }
    }
}

void computer::windowResized() { }
std::vector<const char *> computer::getRequiredExtensions() {
    std::vector<const char *> returnMe = Util::Renderer::GLFWUtil::getRequiredExtensions();
    if (settings.validation)
        returnMe.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return returnMe;
}
