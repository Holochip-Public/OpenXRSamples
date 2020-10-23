//
// Created by Steven Winston on 2019-09-12.
//

#ifndef LIGHTFIELD_GPUSEMAPHORES_H
#define LIGHTFIELD_GPUSEMAPHORES_H

#include <vulkan/vulkan.h>
#include <vector>

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 10000000000000

class GPUSemaphores {
public:
    GPUSemaphores();
    ~GPUSemaphores() = default;
    bool initSemaphores(VkDevice device, uint32_t framesInFlight);
    VkSemaphore * getCurrentAvailableSem(uint32_t currentFrame) { return &imageAvailableSemaphore[currentFrame]; }
    VkSemaphore * getCurrentFinishedSem(uint32_t currentFrame) { return &renderFinishedSemaphore[currentFrame]; }
    VkFence * getCurrentFence(uint32_t currentFrame) { return &inFlightFences[currentFrame]; }

    void destroySemaphores();

private:
    uint32_t maxFramesInFlight;
    VkDevice mDevice;
    std::vector<VkSemaphore> imageAvailableSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
};


#endif //LIGHTFIELD_GPUSEMAPHORES_H
