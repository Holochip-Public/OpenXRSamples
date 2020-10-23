//
// Created by Steven Winston on 2019-09-12.
//

#include "CommandPool.h"
#include "Device.h"
#include "CommonHelper.h"
#include "CommandBuffers.h"

namespace Util {
    namespace Renderer {
        CommandPool::CommandPool(const class Device &_device, uint32_t queueFamilyIndex, bool allowReset)
                : device(_device), commandPool(nullptr) {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyIndex;
            poolInfo.flags = allowReset ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;

            VK_CHECK_RESULT(vkCreateCommandPool(device.getLogicalDevice(), &poolInfo, nullptr, &commandPool))
        }

        CommandPool::~CommandPool() {
            if (commandPool != nullptr) {
                vkDestroyCommandPool(device.getLogicalDevice(), commandPool, nullptr);
                commandPool = nullptr;
            }
        }

        void SingleTimeCommands::Submit(VkQueue queue, CommandPool &commandPool, const std::function<void(VkCommandBuffer)> &action) {
            CommandBuffers commandBuffers(commandPool, 1);

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffers[0], &beginInfo);

            action(commandBuffers[0]);

            vkEndCommandBuffer(commandBuffers[0]);

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[0];

            vkQueueSubmit(queue, 1, &submitInfo, nullptr);
            vkQueueWaitIdle(queue);
        }

    }
}
