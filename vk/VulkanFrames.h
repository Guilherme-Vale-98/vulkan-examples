#pragma once

#include "VulkanContext.h"

#include <vector>

class VulkanFrames {
public:
    explicit VulkanFrames(VulkanContext& ctx) : ctx_(ctx) {}
    ~VulkanFrames() { destroy(); }

    VulkanFrames(const VulkanFrames&)            = delete;
    VulkanFrames& operator=(const VulkanFrames&) = delete;

    void create(uint32_t framesInFlight, uint32_t swapchainImageCount);
    void destroy();

    void waitForFrame(uint64_t frameNumber);

    void            resetPool(uint32_t frameIndex);
    VkCommandBuffer commandBuffer(uint32_t frameIndex) const { return commandBuffers_[frameIndex]; }

    VkSemaphore imageAvailable(uint32_t frameIndex) const { return imageAvailable_[frameIndex]; }
    VkSemaphore renderFinished(uint32_t imageIndex) const { return renderFinished_[imageIndex]; }
    VkSemaphore timeline() const { return timeline_; }

    uint32_t framesInFlight() const { return framesInFlight_; }

    void recreateImageSemaphores(uint32_t swapchainImageCount);

private:
    VulkanContext&               ctx_;
    uint32_t                     framesInFlight_ = 0;
    std::vector<VkCommandPool>   pools_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore>     imageAvailable_;
    std::vector<VkSemaphore>     renderFinished_;
    VkSemaphore                  timeline_ = VK_NULL_HANDLE;
};
