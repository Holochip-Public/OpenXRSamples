//
// Created by Steven Winston on 2019-09-12.
//

#include <stdexcept>
#include "GPUSemaphores.h"
#include "CommonHelper.h"

GPUSemaphores::GPUSemaphores()
: mDevice(VK_NULL_HANDLE)
, maxFramesInFlight(0)
{
}

bool GPUSemaphores::initSemaphores(VkDevice device, uint32_t framesInFlight) {
    mDevice = device;
    maxFramesInFlight = framesInFlight;

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i = 0; i < maxFramesInFlight; ++i) {
        VkSemaphore imageSem, renderSem;
        VkFence  fence;
        VK_CHECK_RESULT(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &imageSem));
        imageAvailableSemaphore.push_back(imageSem);
        VK_CHECK_RESULT(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &renderSem));
        renderFinishedSemaphore.push_back(renderSem);
        VK_CHECK_RESULT(vkCreateFence(mDevice, &fenceInfo, nullptr, &fence));
        inFlightFences.push_back(fence);
    }

    return true;
}

void GPUSemaphores::destroySemaphores() {
    for(auto semaphore : imageAvailableSemaphore) {
        vkDestroySemaphore(mDevice, semaphore, nullptr);
    }
    for(auto semaphore : renderFinishedSemaphore) {
        vkDestroySemaphore(mDevice, semaphore, nullptr);
    }
    for(auto fence : inFlightFences) {
        vkDestroyFence(mDevice, fence, nullptr);
    }
}
