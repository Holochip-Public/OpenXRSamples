//
// Created by Steven Winston on 2019-09-12.
//

#ifndef LIGHTFIELD_COMMANDPOOL_H
#define LIGHTFIELD_COMMANDPOOL_H


#include <functional>
#include <vulkan/vulkan.h>

class Device;

namespace Util {
    namespace Renderer {
        class CommandPool;

        class Buffers;

        class SingleTimeCommands final {
        public:

            static void Submit(VkQueue queue, CommandPool &commandPool, const std::function<void(VkCommandBuffer)> &action);
        };

        class CommandPool final {
        public:
            CommandPool(const Device &_device, uint32_t queueFamilyIndex, bool allowReset);

            ~CommandPool();

            const Device &GetDevice() const { return device; }

            VkCommandPool Get() const { return commandPool; }

        private:
            const Device &device;
            VkCommandPool commandPool;
        };
    }
}

#endif //LIGHTFIELD_COMMANDPOOL_H
