//
// Created by swinston on 4/16/20.
//

#include "Device.h"
#include "CommandBuffers.h"
#include "CommandPool.h"
#include "CommonHelper.h"

namespace Util {
    namespace Renderer {
        CommandBuffers::CommandBuffers(CommandPool &_commandPool, uint32_t size)
                : commandPool(_commandPool) {
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool.Get();
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = size;

            commandBuffers.resize(size);
            VK_CHECK_RESULT(vkAllocateCommandBuffers(commandPool.GetDevice().getLogicalDevice(), &allocInfo, &*commandBuffers.begin()));
        }

        CommandBuffers::~CommandBuffers() {
            if (!commandBuffers.empty())
            {
                vkFreeCommandBuffers(commandPool.GetDevice().getLogicalDevice(), commandPool.Get(), commandBuffers.size(), &*commandBuffers.begin());
                commandBuffers.clear();
            }
        }

        VkCommandBuffer CommandBuffers::Begin(size_t i) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr; // Optional

            VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers[i], &beginInfo));

            return commandBuffers[i];
        }

        void CommandBuffers::End(size_t i) {
            VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers[i]))
        }
    }
}