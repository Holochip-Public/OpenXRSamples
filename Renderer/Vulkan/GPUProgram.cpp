//
// Created by Steven Winston on 2019-09-12.
//

#include <cassert>
#include <vector>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#define GetCurDir _getcwd
#else
#include <unistd.h>
#define GetCurDir getcwd
#endif

#include "GPUProgram.h"

#include "spirv/unified1/spirv.hpp"
#include "CommonHelper.h"

DescriptorInfo::DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
{
    image.sampler = VK_NULL_HANDLE;
    image.imageView = imageView;
    image.imageLayout = imageLayout;
}

DescriptorInfo::DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout) {
    image.sampler = sampler;
    image.imageView = imageView;
    image.imageLayout = imageLayout;
}

DescriptorInfo::DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range) {
    buffer.buffer = buffer_;
    buffer.offset = offset;
    buffer.range = range;
}

DescriptorInfo::DescriptorInfo(VkBuffer buffer_) {
    buffer.buffer = buffer_;
    buffer.offset = 0;
    buffer.range = VK_WHOLE_SIZE;
}

std::vector<char> ReadShaderFile(const std::string & fileName) {
    std::vector<char> returnMe;
    char buff[FILENAME_MAX];
    GetCurDir( buff, FILENAME_MAX );
    std::string workDir(buff);
    workDir += "/" + fileName;
    std::fstream file(workDir, std::fstream::in | std::fstream::ate);
    uint64_t length = file.tellg();
    file.seekg(0);
    returnMe.resize(length);
    file.read(&*returnMe.begin(), length);
    file.close();
    return returnMe;
}

bool GPUProgram::loadShader(GPUProgram &shader, VkDevice device, const char *path) {
    std::vector<char> data = ReadShaderFile(path);
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo moduleCreateInfo{};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = data.size();
    moduleCreateInfo.pCode = (uint32_t*)&*data.begin();

    VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule))

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = data.size() * sizeof(char); // note: this needs to be a number of bytes!
    createInfo.pCode = reinterpret_cast<const uint32_t*>(&*data.begin());

    VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule))

    assert(data.size() % 4 == 0);
    parseShader(shader, reinterpret_cast<const uint32_t*>(&*data.begin()), data.size() / 4);

    shader.module = shaderModule;

    return true;
}

// https://www.khronos.org/registry/spir-v/specs/1.2/SPIRV.html
struct Id
{
    Id() : opcode(0), typeId(0), storageClass(0), binding(0), set(0) {}
    uint32_t opcode;
    uint32_t typeId;
    uint32_t storageClass;
    uint32_t binding;
    uint32_t set;
};

static VkShaderStageFlagBits getShaderStage(spv::ExecutionModel executionModel)
{
    switch (executionModel)
    {
        case spv::ExecutionModelVertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case spv::ExecutionModelFragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case spv::ExecutionModelGLCompute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case spv::ExecutionModelTaskNV:
            return VK_SHADER_STAGE_TASK_BIT_NV;
        case spv::ExecutionModelMeshNV:
            return VK_SHADER_STAGE_MESH_BIT_NV;
        case spv::ExecutionModelRayGenerationNV: // note: ExecutionModelRayGenerationKHR when move to vulkan 1.3
            return VK_SHADER_STAGE_RAYGEN_BIT_NV;
        case spv::ExecutionModelIntersectionNV:
            return VK_SHADER_STAGE_INTERSECTION_BIT_NV;
        case spv::ExecutionModelAnyHitNV:
            return VK_SHADER_STAGE_ANY_HIT_BIT_NV;
        case spv::ExecutionModelClosestHitNV:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        case spv::ExecutionModelMissNV:
            return VK_SHADER_STAGE_MISS_BIT_NV;
        case spv::ExecutionModelCallableNV:
            return VK_SHADER_STAGE_CALLABLE_BIT_NV;

        default:
            assert(!"Unsupported execution model");
            return VkShaderStageFlagBits(0);
    }
}

void GPUProgram::parseShader(GPUProgram& shader, const uint32_t* code, uint32_t codeSize)
{
    assert(code[0] == spv::MagicNumber);

    uint32_t idBound = code[3];

    std::vector<Id> ids;
    ids.resize(idBound);

    const uint32_t* insn = code + 5;

    while (insn != code + codeSize)
    {
        auto opcode = uint16_t(insn[0]);
        auto wordCount = uint16_t(insn[0] >> 16u);

        switch (opcode)
        {
            case spv::OpEntryPoint:
            {
                assert(wordCount >= 2);
                shader.stage = getShaderStage(spv::ExecutionModel(insn[1]));
                std::string string((char *)&(insn[3]));
                shader.entryPoint = string;
            } break;
            case spv::OpExecutionMode:
            {
                assert(wordCount >= 3);
                uint32_t mode = insn[2];

                if(mode == spv::ExecutionModeLocalSize)
                {
                    assert(wordCount == 6);
                    shader.localSize = {
                            static_cast<float>(insn[3]),
                            static_cast<float>(insn[4]),
                            static_cast<float>(insn[5])
                    };
                }
            } break;
            case spv::OpDecorate:
            {
                assert(wordCount >= 3);

                uint32_t id = insn[1];
                assert(id < idBound);

                switch (insn[2])
                {
                    case spv::DecorationDescriptorSet:
                        assert(wordCount == 4);
                        ids[id].set = insn[3];
                        break;
                    case spv::DecorationBinding:
                        assert(wordCount == 4);
                        ids[id].binding = insn[3];
                        break;
                }
            } break;
            case spv::OpTypeStruct:
            case spv::OpTypeImage:
            case spv::OpTypeSampler:
            case spv::OpTypeSampledImage:
            {
                assert(wordCount >= 2);

                uint32_t id = insn[1];
                assert(id < idBound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
            } break;
            case spv::OpTypePointer:
            {
                assert(wordCount == 4);

                uint32_t id = insn[1];
                assert(id < idBound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
                ids[id].typeId = insn[3];
                ids[id].storageClass = insn[2];
            } break;
            case spv::OpVariable:
            {
                assert(wordCount >= 4);

                uint32_t id = insn[2];
                assert(id < idBound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
                ids[id].typeId = insn[1];
                ids[id].storageClass = insn[3];
            } break;
            default: break;
        }

        assert(insn + wordCount <= code + codeSize);
        insn += wordCount;
    }

    for (auto& id : ids)
    {
        if (id.opcode == spv::OpVariable && (id.storageClass == spv::StorageClassUniform || id.storageClass == spv::StorageClassUniformConstant || id.storageClass == spv::StorageClassStorageBuffer))
        {
            assert(id.set == 0);
            assert(id.binding < 32);
            assert(ids[id.typeId].opcode == spv::OpTypePointer);

            assert((shader.resourceMask & (1u << id.binding)) == 0);

            uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;

            switch (typeKind)
            {
                case spv::OpTypeStruct:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    shader.resourceMask |= 1u << id.binding;
                    break;
                case spv::OpTypeImage:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    shader.resourceMask |= 1u << id.binding;
                    break;
                case spv::OpTypeSampler:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLER;
                    shader.resourceMask |= 1u << id.binding;
                    break;
                case spv::OpTypeSampledImage:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    shader.resourceMask |= 1u << id.binding;
                    break;
                default:
                    assert(!"Unknown resource type");
            }
        }

        if (id.opcode == spv::OpVariable && id.storageClass == spv::StorageClassPushConstant)
        {
            shader.usesPushConstants = true;
        }
    }
}

VkSpecializationInfo GPUProgram::fillSpecializationInfo(std::vector<VkSpecializationMapEntry>& entries, const GPUProgram::Constants& constants)
{
    for (size_t i = 0; i < constants.size(); ++i) {
        VkSpecializationMapEntry entry = {uint32_t(i), uint32_t(i * 4), 4};
        entries.push_back(entry);
    }

    VkSpecializationInfo result = {};
    result.mapEntryCount = uint32_t(entries.size());
    result.pMapEntries = &*entries.begin();
    result.dataSize = constants.size() * sizeof(int);
    result.pData = constants.begin();

    return result;
}

VkPipeline GPUProgram::createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass,
                                              Shaders shaders, VkPipelineLayout layout,
                                              GPUProgram::Constants constants) {
    std::vector<VkSpecializationMapEntry> specializationEntries;
    VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (const GPUProgram* shader : shaders)
    {
        VkPipelineShaderStageCreateInfo stage_loc = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage_loc.stage = shader->stage;
        stage_loc.module = shader->module;
        stage_loc.pName = shader->entryPoint.c_str();
        stage_loc.pSpecializationInfo = &specializationInfo;

        stages.push_back(stage_loc);
    }

    createInfo.stageCount = uint32_t(stages.size());
    createInfo.pStages = &*stages.begin();

    VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    createInfo.pVertexInputState = &vertexInput;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    createInfo.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    createInfo.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.lineWidth = 1.f;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    createInfo.pRasterizationState = &rasterizationState;

    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.pMultisampleState = &multisampleState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = true;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;
    createInfo.pDepthStencilState = &depthStencilState;

    VkPipelineColorBlendAttachmentState colorAttachmentState = {};
    colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorAttachmentState;
    createInfo.pColorBlendState = &colorBlendState;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;
    createInfo.pDynamicState = &dynamicState;

    createInfo.layout = layout;
    createInfo.renderPass = renderPass;

    VkPipeline pipeline = nullptr;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

    return pipeline;
}

VkPipeline GPUProgram::createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const GPUProgram &shader,
                                             VkPipelineLayout layout, GPUProgram::Constants constants) {
    assert(shader.stage == VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    std::vector<VkSpecializationMapEntry> specializationEntries;
    VkSpecializationInfo specializationInfo = fillSpecializationInfo(specializationEntries, constants);

    VkPipelineShaderStageCreateInfo stageLoc = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stageLoc.stage = shader.stage;
    stageLoc.module = shader.module;
    stageLoc.pName = shader.entryPoint.c_str();
    stageLoc.pSpecializationInfo = &specializationInfo;

    createInfo.stage = stageLoc;
    createInfo.layout = layout;

    VkPipeline pipeline = nullptr;
    VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &createInfo, nullptr, &pipeline))

    return pipeline;
}

static uint32_t gatherResources(GPUProgram::Shaders shaders, VkDescriptorType(&resourceTypes)[32])
{
    uint32_t resourceMask = 0;

    for (const GPUProgram* shader : shaders)
    {
        for (uint32_t i = 0; i < 32; ++i)
        {
            if (shader->resourceMask & (1u << i))
            {
                if (resourceMask & (1u << i))
                {
                    assert(resourceTypes[i] == shader->resourceTypes[i]);
                }
                else
                {
                    resourceTypes[i] = shader->resourceTypes[i];
                    resourceMask |= 1u << i;
                }
            }
        }
    }

    return resourceMask;
}

static VkDescriptorSetLayout createSetLayout(VkDevice device, GPUProgram::Shaders shaders, bool pushDescriptorsSupported)
{
    std::vector<VkDescriptorSetLayoutBinding> setBindings;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResources(shaders, resourceTypes);

    for (uint32_t i = 0; i < 32; ++i)
        if (resourceMask & (1u << i))
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = i;
            binding.descriptorType = resourceTypes[i];
            binding.descriptorCount = 1;

            binding.stageFlags = 0;
            for (const GPUProgram* shader : shaders)
                if (shader->resourceMask & (1u << i))
                    binding.stageFlags |= shader->stage;

            setBindings.push_back(binding);
        }

    VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setCreateInfo.flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
    setCreateInfo.bindingCount = uint32_t(setBindings.size());
    setCreateInfo.pBindings = &*setBindings.begin();

    VkDescriptorSetLayout setLayout = nullptr;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setCreateInfo, nullptr, &setLayout))

    return setLayout;
}

static VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout, VkShaderStageFlags pushConstantStages, size_t pushConstantSize)
{
    VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &setLayout;

    VkPushConstantRange pushConstantRange = {};

    if (pushConstantSize)
    {
        pushConstantRange.stageFlags = pushConstantStages;
        pushConstantRange.size = uint32_t(pushConstantSize);

        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges = &pushConstantRange;
    }

    VkPipelineLayout layout = nullptr;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &createInfo, nullptr, &layout))

    return layout;
}

static VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, GPUProgram::Shaders shaders, bool pushDescriptorsSupported)
{
    std::vector<VkDescriptorUpdateTemplateEntry> entries;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResources(shaders, resourceTypes);

    for (uint32_t i = 0; i < 32; ++i)
        if (resourceMask & (1u << i))
        {
            VkDescriptorUpdateTemplateEntry entry = {};
            entry.dstBinding = i;
            entry.dstArrayElement = 0;
            entry.descriptorCount = 1;
            entry.descriptorType = resourceTypes[i];
            entry.offset = sizeof(DescriptorInfo) * i;
            entry.stride = sizeof(DescriptorInfo);

            entries.push_back(entry);
        }

    VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };

    createInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
    createInfo.pDescriptorUpdateEntries = &*entries.begin();

    createInfo.templateType = pushDescriptorsSupported ? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR : VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    createInfo.descriptorSetLayout = pushDescriptorsSupported ? nullptr : setLayout;
    createInfo.pipelineBindPoint = bindPoint;
    createInfo.pipelineLayout = layout;

    VkDescriptorUpdateTemplate updateTemplate = nullptr;
    VK_CHECK_RESULT(vkCreateDescriptorUpdateTemplate(device, &createInfo, 0, &updateTemplate))

    return updateTemplate;
}

Program
GPUProgram::createProgram(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize,
                          bool pushDescriptorsSupported) {
    VkShaderStageFlags pushConstantStages = 0;
    for (const GPUProgram* shader : shaders)
        if (shader->usesPushConstants)
            pushConstantStages |= shader->stage;

    Program program = {};

    program.bindPoint = bindPoint;

    program.setLayout = createSetLayout(device, shaders, pushDescriptorsSupported);
    assert(program.setLayout);

    program.layout = createPipelineLayout(device, program.setLayout, pushConstantStages, pushConstantSize);
    assert(program.layout);

    program.updateTemplate = createUpdateTemplate(device, bindPoint, program.layout, program.setLayout, shaders, pushDescriptorsSupported);
    assert(program.updateTemplate);

    program.pushConstantStages = pushConstantStages;

    return program;
}

void GPUProgram::destroyProgram(VkDevice device, const Program &program) {
    vkDestroyDescriptorUpdateTemplate(device, program.updateTemplate, nullptr);
    vkDestroyPipelineLayout(device, program.layout, nullptr);
    vkDestroyDescriptorSetLayout(device, program.setLayout, nullptr);
}

GPUProgram::GPUProgram()
: entryPoint("main")
{
}
