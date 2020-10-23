//
// Created by Steven Winston on 10/1/19.
//

#ifndef LIGHTFIELD_UIOVERLAY_H
#define LIGHTFIELD_UIOVERLAY_H


#include <string>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "Device.h"
#include "Buffers.h"

class Texture2D;

namespace Util {
    namespace Renderer {
        class VulkanUtil;

        class UIOverlay {
        public:
            Device *device;
            VkQueue queue;
            VulkanUtil * appPtr;

            VkSampleCountFlagBits rasterizationSamples;
            uint32_t subpass;

            Buffers vertexBuffer;
            Buffers indexBuffer;
            int32_t vertexCount;
            int32_t indexCount;
            uint32_t scaleFactor;
            std::vector<VkPipelineShaderStageCreateInfo> shaders;

            VkDescriptorPool descriptorPool;
            VkDescriptorSetLayout descriptorSetLayout;
            VkDescriptorSet descriptorSet;
            VkPipelineLayout pipelineLayout;
            VkPipeline pipeline;
            VkRenderPass imGuiRenderPass;
            std::vector<VkFramebuffer> frameBuffers;
            std::vector<VkCommandBuffer> uiCmdBuffers;
            VkCommandPool imGuiCommandPools;

            VkDeviceMemory fontMemory;
            VkImage fontImage;
            VkImageView fontView;
            VkSampler sampler;

            struct PushConstBlock {
                glm::vec2 scale;
                glm::vec2 translate;
            } pushConstBlock;

            bool visible;
            bool updated;
            float scale;

            UIOverlay();
            ~UIOverlay() = default;

            void preparePipeline(VkPipelineCache pipelineCache, VkRenderPass renderPass);
            void prepareResources();

            bool update();
            void draw(uint32_t currentBuffer);
            void setContent();
            void resize(uint32_t width, uint32_t height);
            void freeResources();
            static bool header(const char *caption);
            bool checkBox(const char *caption, bool *value);
            bool checkBox(const char *caption, int32_t *value);
            bool inputFloat(const char *caption, float *value, float step, uint32_t precision);
            bool sliderFloat(const char *caption, float *value, float min, float max);
            bool sliderInt(const char *caption, int32_t *value, int32_t min, int32_t max);
            bool comboBox(const char *caption, int32_t *itemindex, const std::vector<std::string>& items);
            bool button(const char *caption);
            bool ImageButton(Texture2D* buttonTexture, const char *formatstr, ...);
            static void text(const char *formatstr, ...);
            static bool GetWantCapture();
        };
    }
}

#endif //LIGHTFIELD_UIOVERLAY_H
