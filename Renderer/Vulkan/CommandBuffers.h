//
// Created by swinston on 4/16/20.
//

#ifndef LIGHTFIELDFORWARDRENDERER_COMMANDBUFFERS_H
#define LIGHTFIELDFORWARDRENDERER_COMMANDBUFFERS_H

#include <vulkan/vulkan.h>
#include <vector>

namespace Util {
    namespace Renderer {
        class CommandPool;

        class CommandBuffers {
        public:
            CommandBuffers(CommandPool &commandPool, uint32_t size);

            ~CommandBuffers();

            uint32_t Size() const { return static_cast<uint32_t>(commandBuffers.size()); }

            VkCommandBuffer &operator[](const size_t i) { return commandBuffers[i]; }

            VkCommandBuffer Begin(size_t i);

            void End(size_t);

        private:
            const CommandPool &commandPool;
            std::vector<VkCommandBuffer> commandBuffers;
        };
    }
}
#endif //LIGHTFIELDFORWARDRENDERER_COMMANDBUFFERS_H
