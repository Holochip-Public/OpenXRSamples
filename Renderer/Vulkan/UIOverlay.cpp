//
// Created by Steven Winston on 10/1/19.
//

#include <imgui.h>
#include "UIOverlay.h"
#include "../VulkanUtil.h"
#include "Initializers.h"
#include "../imguiExtras.h"
#include "Texture.h"

#ifdef WINDOWS
#include <direct.h>
#define GetCurDir _getcwd
#else
#include <unistd.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GetCurDir getcwd
#endif

namespace Util {
    namespace Renderer {

UIOverlay::UIOverlay()
    : scale(1.0f)
    , updated(false)
    , visible(true)
    , device(nullptr)
    , queue(nullptr)
    , rasterizationSamples(VK_SAMPLE_COUNT_1_BIT)
    , subpass(0)
    , vertexBuffer()
    , indexBuffer()
    , vertexCount(0)
    , indexCount(0)
    , descriptorPool()
    , descriptorSet()
    , descriptorSetLayout()
    , pipelineLayout()
    , pipeline()
    , fontMemory(VK_NULL_HANDLE)
    , fontImage(VK_NULL_HANDLE)
    , fontView(VK_NULL_HANDLE)
    , sampler()
    , appPtr(nullptr)
{
    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    // Dimensions
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scale;
}

void UIOverlay::prepareResources() {
    assert(appPtr != nullptr);
    // Initialise descriptor pool and render pass for ImGui.
    // Descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
            Initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = Initializers::descriptorPoolCreateInfo(poolSizes, 2);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->getLogicalDevice(), &descriptorPoolInfo, nullptr, &descriptorPool))

    VkAttachmentDescription attachment = {};
    attachment.format = appPtr->swapChain.colorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpassDesc;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    if (vkCreateRenderPass(device->getLogicalDevice(), &info, nullptr, &imGuiRenderPass) != VK_SUCCESS) {
        fprintf(stderr, "Could not create Dear ImGui's render pass");
        assert(false);
    }

    VkImageView attachmentImgView[1];
    VkFramebufferCreateInfo FBinfo = {};
    FBinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FBinfo.renderPass =imGuiRenderPass;
    FBinfo.attachmentCount = 1;
    FBinfo.pAttachments = attachmentImgView;
    FBinfo.width = appPtr->swapChain.getExtent().width;
    FBinfo.height = appPtr->swapChain.getExtent().height;
    FBinfo.layers = 1;
    frameBuffers.resize(appPtr->swapChain.imageCount);
    for (uint32_t i = 0; i < frameBuffers.size(); i++)
    {
        attachmentImgView[0] = appPtr->swapChain.buffers[i].view;
        VK_CHECK_RESULT(vkCreateFramebuffer(device->getLogicalDevice(), &FBinfo, nullptr, &frameBuffers[i]))
    }

    // Color scheme
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.8f, 0.8f, 0.8f, 0.1f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.8f, 0.8f, 0.8f, 0.4f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    // Dimensions
    ImGuiIO &io = ImGui::GetIO();

    // No ini file.
    io.IniFilename = nullptr;

    // Create font texture
    unsigned char* fontData;
    int texWidth, texHeight;
    char buff[FILENAME_MAX];
    GetCurDir( buff, FILENAME_MAX );
    std::string workDir(buff);
    workDir += "/../";
    workDir += "data/Roboto-Medium.ttf";

    // Window scaling.
    float xscale;
    float yscale;
    if(!appPtr->wantOpenXR)
        glfwGetWindowContentScale(appPtr->getGLFWUtil()->getWindow(), &xscale, &yscale);
    else {xscale = 1; yscale = 1;}

    scale = xscale;

    ImGui::GetStyle().ScaleAllSizes(scale);

    // Upload ImGui fonts
    io.Fonts->AddFontFromFileTTF(workDir.c_str(), 60.0f);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    VkDeviceSize uploadSize = texWidth*texHeight * 4 * sizeof(char);

    // Create target image for copy
    VkImageCreateInfo imageInfo = Initializers::imageCreateInfo();
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = texWidth;
    imageInfo.extent.height = texHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &fontImage))
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device->getLogicalDevice(), fontImage, &memReqs);
    VkMemoryAllocateInfo memAllocInfo = Initializers::memoryAllocateInfo();
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->getLogicalDevice(), &memAllocInfo, nullptr, &fontMemory))
    VK_CHECK_RESULT(vkBindImageMemory(device->getLogicalDevice(), fontImage, fontMemory, 0))

    // Image view
    VkImageViewCreateInfo viewInfo = Initializers::imageViewCreateInfo();
    viewInfo.image = fontImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &fontView))

    // Staging buffers for font data upload
    Buffers stagingBuffer;

    VK_CHECK_RESULT(device->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            uploadSize))

    stagingBuffer.map();
    memcpy(stagingBuffer.mapped, fontData, uploadSize);
    stagingBuffer.unmap();

    // Copy buffer data to font image
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Prepare for transfer
    tools::setImageLayout(
            copyCmd,
            fontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = texWidth;
    bufferCopyRegion.imageExtent.height = texHeight;
    bufferCopyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer.buffer,
            fontImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
    );

    // Prepare for shader read
    tools::setImageLayout(
            copyCmd,
            fontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    device->flushCommandBuffer(copyCmd, queue, true);

    stagingBuffer.destroy();

    // Font texture Sampler
    VkSamplerCreateInfo samplerInfo = Initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device->getLogicalDevice(), &samplerInfo, nullptr, &sampler))

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            Initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayout = Initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->getLogicalDevice(), &descriptorLayout, nullptr, &descriptorSetLayout))

    // Descriptor set
    VkDescriptorSetAllocateInfo allocInfo = Initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->getLogicalDevice(), &allocInfo, &descriptorSet))
    VkDescriptorImageInfo fontDescriptor = Initializers::descriptorImageInfo(
            sampler,
            fontView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            Initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
    };
    vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), &*writeDescriptorSets.begin(), 0, nullptr);



    // Create one command buffer for each swap chain image and reuse for rendering
    QueueFamilyIndices queueFamilyIndices = VulkanUtil::findQueueFamilies(device->getPhysicalDevice(), appPtr->swapChain.getSurface());
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device->getLogicalDevice(), &poolInfo, nullptr, &imGuiCommandPools) != VK_SUCCESS) {
        fprintf(stderr, "failed to create command pool!");
        assert(false);
    }
    uiCmdBuffers.resize(appPtr->swapChain.imageCount);

    for(int i = 0; i < appPtr->swapChain.imageCount; ++i) {
        VkCommandBufferAllocateInfo cmdBufAllocateInfo =
                Initializers::commandBufferAllocateInfo(
                        imGuiCommandPools,
                        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                        static_cast<uint32_t>(uiCmdBuffers.size()));

                VK_CHECK_RESULT(vkAllocateCommandBuffers(device->getLogicalDevice(), &cmdBufAllocateInfo, &*uiCmdBuffers.begin()))
    }
}

void UIOverlay::preparePipeline(VkPipelineCache pipelineCache, VkRenderPass renderPass) {
// Pipeline layout
    // Push constants for UI rendering parameters
    VkPushConstantRange pushConstantRange = Initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout))

    // Setup graphics pipeline for UI rendering
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            Initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
            Initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Enable blending
    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.blendEnable = VK_TRUE;
    blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendState =
            Initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
            Initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

    VkPipelineViewportStateCreateInfo viewportState =
            Initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
            Initializers::pipelineMultisampleStateCreateInfo(rasterizationSamples);

    std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
            Initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Initializers::pipelineCreateInfo(pipelineLayout, imGuiRenderPass);

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaders.size());
    pipelineCreateInfo.pStages = &*shaders.begin();
    pipelineCreateInfo.subpass = subpass;

    // Vertex bindings an attributes based on ImGui vertex definition
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            Initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            Initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
            Initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
            Initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = Initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = &*vertexInputBindings.begin();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = &*vertexInputAttributes.begin();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->getLogicalDevice(), nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline))
}

bool UIOverlay::update() {
    ImDrawData* imDrawData = ImGui::GetDrawData();
    bool updateCmdBuffers = false;

    if (!imDrawData) { return false; }

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Update buffers only if vertex or index count has been changed compared to current buffer size
    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
        return false;
    }

    // Vertex buffer
    if ((vertexBuffer.buffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
        vertexBuffer.unmap();
        vertexBuffer.destroy();
        VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &vertexBuffer, vertexBufferSize))
        vertexCount = imDrawData->TotalVtxCount;
        vertexBuffer.unmap();
        vertexBuffer.map();
        updateCmdBuffers = true;
    }

    // Index buffer
//    VkDeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);
    if ((indexBuffer.buffer == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
        indexBuffer.unmap();
        indexBuffer.destroy();
        VK_CHECK_RESULT(device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &indexBuffer, indexBufferSize))
        indexCount = imDrawData->TotalIdxCount;
        indexBuffer.map();
        updateCmdBuffers = true;
    }

    // Upload data
    auto vtxDst = (ImDrawVert*)vertexBuffer.mapped;
    auto idxDst = (ImDrawIdx*)indexBuffer.mapped;

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    vertexBuffer.flush();
    indexBuffer.flush();

    return updateCmdBuffers;
//
//            const auto& io = ImGui::GetIO();
//            const float distance = 10.0f;
//            const ImVec2 pos = ImVec2(io.DisplaySize.x - distance, distance);
//            const ImVec2 posPivot = ImVec2(1.0f, 0.0f);
//            ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);
//            ImGui::SetNextWindowBgAlpha(0.3f); // Transparent background
//
//            return true;
}

void UIOverlay::draw(uint32_t currentBuffer) {
    ImDrawData* imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    std::vector<VkClearValue> clearValues = {{{ 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }}};

    VkRenderPassBeginInfo renderPassBeginInfo = Initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = imGuiRenderPass;
    renderPassBeginInfo.renderArea.offset = { 0, 0};
    renderPassBeginInfo.renderArea.extent = appPtr->swapChain.getExtent();
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = &*clearValues.begin();
    VkCommandBufferBeginInfo cmdBufInfo = Initializers::commandBufferBeginInfo();

    // Set target frame buffer
    renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];


    vkBeginCommandBuffer(uiCmdBuffers[currentBuffer], &cmdBufInfo);
    vkCmdBeginRenderPass(uiCmdBuffers[currentBuffer], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport viewport = Initializers::viewport((float)renderPassBeginInfo.renderArea.extent.width, (float)renderPassBeginInfo.renderArea.extent.height, 0.0f, 1.0f);
    const VkRect2D scissor = Initializers::rect2D(renderPassBeginInfo.renderArea.extent.width, renderPassBeginInfo.renderArea.extent.height, 0, 0);
    vkCmdSetViewport(uiCmdBuffers[currentBuffer], 0, 1, &viewport);
    vkCmdSetScissor(uiCmdBuffers[currentBuffer], 0, 1, &scissor);


    ImGuiIO& io = ImGui::GetIO();
    vkCmdBindPipeline(uiCmdBuffers[currentBuffer], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(uiCmdBuffers[currentBuffer], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pushConstBlock.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(uiCmdBuffers[currentBuffer], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

    if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
        vkCmdEndRenderPass(uiCmdBuffers[currentBuffer]);
        vkEndCommandBuffer(uiCmdBuffers[currentBuffer]);
        return;
    }

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(uiCmdBuffers[currentBuffer], 0, 1, &vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(uiCmdBuffers[currentBuffer], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = imDrawData->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(uiCmdBuffers[currentBuffer], 0, 1, &scissorRect);
            vkCmdDrawIndexed(uiCmdBuffers[currentBuffer], pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }
        vertexOffset += cmd_list->VtxBuffer.Size;
    }


    vkCmdEndRenderPass(uiCmdBuffers[currentBuffer]);
    vkEndCommandBuffer(uiCmdBuffers[currentBuffer]);
}

void UIOverlay::resize(uint32_t width, uint32_t height) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(width), (float)(height));
    for(auto frameBuffer : frameBuffers) {
        vkDestroyFramebuffer(device->getLogicalDevice(), frameBuffer, nullptr);
    }
    VkImageView attachmentImgView[1];
    VkFramebufferCreateInfo FBinfo = {};
    FBinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FBinfo.renderPass =imGuiRenderPass;
    FBinfo.attachmentCount = 1;
    FBinfo.pAttachments = attachmentImgView;
    FBinfo.width = appPtr->swapChain.getExtent().width;
    FBinfo.height = appPtr->swapChain.getExtent().height;
    FBinfo.layers = 1;
    frameBuffers.resize(appPtr->swapChain.imageCount);
    for (uint32_t i = 0; i < frameBuffers.size(); i++)
    {
        attachmentImgView[0] = appPtr->swapChain.buffers[i].view;
        VK_CHECK_RESULT(vkCreateFramebuffer(device->getLogicalDevice(), &FBinfo, nullptr, &frameBuffers[i]))
    }
}

void UIOverlay::freeResources() {
    ImGui::DestroyContext();
    vertexBuffer.destroy();
    indexBuffer.destroy();
    vkDestroyImageView(device->getLogicalDevice(), fontView, nullptr);
    vkDestroyImage(device->getLogicalDevice(), fontImage, nullptr);
    vkFreeMemory(device->getLogicalDevice(), fontMemory, nullptr);
    vkDestroySampler(device->getLogicalDevice(), sampler, nullptr);
    vkDestroyDescriptorSetLayout(device->getLogicalDevice(), descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);
    vkDestroyPipelineLayout(device->getLogicalDevice(), pipelineLayout, nullptr);
    vkDestroyPipeline(device->getLogicalDevice(), pipeline, nullptr);
    if(!uiCmdBuffers.empty()) {
        vkFreeCommandBuffers(device->getLogicalDevice(), imGuiCommandPools,
                                 uiCmdBuffers.size(), &*uiCmdBuffers.begin());
        uiCmdBuffers.clear();
    }
    vkDestroyCommandPool(device->getLogicalDevice(), imGuiCommandPools, nullptr);
    for(auto frameBuffer : frameBuffers) {
        vkDestroyFramebuffer(device->getLogicalDevice(), frameBuffer, nullptr);
    }
    vkDestroyRenderPass(device->getLogicalDevice(), imGuiRenderPass, nullptr);
}

bool UIOverlay::header(const char *caption) {
    return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
}

bool UIOverlay::sliderFloat(const char *caption, float *value, float min, float max) {
    bool res = ImGui::SliderFloat(caption, value, min, max);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::sliderInt(const char *caption, int32_t *value, int32_t min, int32_t max) {
    bool res = ImGui::SliderInt(caption, value, min, max);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::comboBox(const char *caption, int32_t *itemindex, const std::vector<std::string>& items) {
    if (items.empty()) {
        return false;
    }
    std::vector<const char *> charitems;
    for(const auto& item : items) {
        charitems.push_back(item.c_str());
    }
    auto itemCount = static_cast<uint32_t>(charitems.size());
    bool res = ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::checkBox(const char *caption, bool *value) {
    bool res = ImGui::Checkbox(caption, value);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::checkBox(const char *caption, int32_t *value) {
    bool val = (*value == 1);
    bool res = ImGui::Checkbox(caption, &val);
    *value = val;
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::inputFloat(const char *caption, float *value, float step, uint32_t precision) {
    bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, precision);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::button(const char *caption) {
    bool res = ImGui::Button(caption);
    if (res) { updated = true; }
    return res;
}

bool UIOverlay::ImageButton(Texture2D* buttonTexture, const char *formatstr, ...) {
    std::vector<char> string;
    va_list args;
    va_start(args, formatstr);
    auto lengthNeeded = vsnprintf(&*string.begin(), 0, formatstr, args);
    string.resize(lengthNeeded + 1);
    vsnprintf(&*string.begin(), lengthNeeded + 1, formatstr, args);
    va_end(args);
    ImTextureID textureId = ImGui::AddTexture(device->getLogicalDevice(), descriptorSetLayout, descriptorPool,
            buttonTexture->sampler, buttonTexture->view, buttonTexture->imageLayout);
    bool res = ImGui::ImageButtonWithText(textureId, &*string.begin());
    if(res) { updated = true; }
    return res;
}

void UIOverlay::text(const char *formatstr, ...) {
    std::vector<char> string;
    va_list args;
    va_start(args, formatstr);
    auto lengthNeeded = vsnprintf(&*string.begin(), 0, formatstr, args);
    string.resize(lengthNeeded + 1);
    vsnprintf(&*string.begin(), lengthNeeded + 1, formatstr, args);
    va_end(args);
    ImGui::Text("%s", &*string.begin());
}

bool UIOverlay::GetWantCapture() {
    ImGuiIO &io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

        void UIOverlay::setContent() {
            ImGuiIO& io = ImGui::GetIO();

            io.DisplaySize = ImVec2((float)appPtr->swapChain.getExtent().width, (float)appPtr->swapChain.getExtent().height);
            io.DeltaTime = (appPtr->frameTimer == 0)? 0.0001f: appPtr->frameTimer;

            io.MousePos = ImVec2(appPtr->mousePos.x, appPtr->mousePos.y);
            io.MouseDown[0] = appPtr->mouseButtons.left;
            io.MouseDown[1] = appPtr->mouseButtons.right;

            ImGui::NewFrame();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2));
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::Begin(appPtr->getWindowTitle().c_str(), nullptr, ImGuiWindowFlags_NoTitleBar);
//            ImGui::TextUnformatted(appPtr->deviceProperties.deviceName);
//            ImGui::Text("%.2f ms/frame (%.1f fps)", (1000.0f / appPtr->lastFPS), appPtr->lastFPS);
            ImGui::Button("Launch the Hello World Demo");
            ImGui::Button("Launch the gears");
            ImGui::Button("Launch the Computer");


#if defined(VK_USE_PLATFORM_ANDROID_KHR)
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f * scale));
#endif
            ImGui::PushItemWidth(110.0f * scale);
            appPtr->OnUpdateUIOverlay(this);
            ImGui::PopItemWidth();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
            ImGui::PopStyleVar();
#endif

            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::Render();

            if (update() || updated) {
                appPtr->buildCommandBuffers();
                updated = false;
            }
        }
    }
}
