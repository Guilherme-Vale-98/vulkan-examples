#include "VulkanFrames.h"

namespace {

VkSemaphore createBinarySemaphore(VkDevice device) {
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
}

}

void VulkanFrames::create(uint32_t framesInFlight, uint32_t swapchainImageCount) {
    framesInFlight_ = framesInFlight;

    pools_.resize(framesInFlight);
    commandBuffers_.resize(framesInFlight);
    imageAvailable_.resize(framesInFlight);

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = ctx_.graphicsFamily();
        VK_CHECK(vkCreateCommandPool(ctx_.device(), &poolInfo, nullptr, &pools_[i]));

        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool        = pools_[i];
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx_.device(), &alloc, &commandBuffers_[i]));

        imageAvailable_[i] = createBinarySemaphore(ctx_.device());
    }

    recreateImageSemaphores(swapchainImageCount);

    VkSemaphoreTypeCreateInfo typeInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeInfo.initialValue  = 0;

    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semInfo.pNext = &typeInfo;
    VK_CHECK(vkCreateSemaphore(ctx_.device(), &semInfo, nullptr, &timeline_));
}

void VulkanFrames::recreateImageSemaphores(uint32_t swapchainImageCount) {
    for (VkSemaphore s : renderFinished_) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(ctx_.device(), s, nullptr);
    }
    renderFinished_.assign(swapchainImageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        renderFinished_[i] = createBinarySemaphore(ctx_.device());
    }
}

void VulkanFrames::waitForFrame(uint64_t frameNumber) {
    const uint64_t target =
        (frameNumber + 1 > framesInFlight_) ? (frameNumber + 1 - framesInFlight_) : 0;

    VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait.semaphoreCount = 1;
    wait.pSemaphores    = &timeline_;
    wait.pValues        = &target;
    VK_CHECK(vkWaitSemaphores(ctx_.device(), &wait, UINT64_MAX));
}

void VulkanFrames::resetPool(uint32_t frameIndex) {
    VK_CHECK(vkResetCommandPool(ctx_.device(), pools_[frameIndex], 0));
}

void VulkanFrames::destroy() {
    if (timeline_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(ctx_.device(), timeline_, nullptr);
        timeline_ = VK_NULL_HANDLE;
    }
    for (VkSemaphore s : renderFinished_) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(ctx_.device(), s, nullptr);
    }
    renderFinished_.clear();
    for (VkSemaphore s : imageAvailable_) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(ctx_.device(), s, nullptr);
    }
    imageAvailable_.clear();
    for (VkCommandPool p : pools_) {
        if (p != VK_NULL_HANDLE) vkDestroyCommandPool(ctx_.device(), p, nullptr);
    }
    pools_.clear();
    commandBuffers_.clear();
    framesInFlight_ = 0;
}
