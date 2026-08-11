#pragma once

#include "VulkanContext.h"
#include "VulkanFrames.h"
#include "VulkanPipeline.h"
#include "VulkanResources.h"
#include "VulkanSwapchain.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

struct ExampleConfig {
    std::string      name           = "example";
    std::string      title          = "Vulkan Example";
    uint32_t         width          = 1280;
    uint32_t         height         = 720;
    bool             depth          = true;
    VkImageLayout    renderLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkPresentModeKHR presentMode    = VK_PRESENT_MODE_FIFO_KHR;
    VkImageUsageFlags swapchainUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    uint32_t         framesInFlight = 2;

    bool             hidden          = false;
    bool             forceValidation = false;
    uint32_t         exitAfterFrames = 0;
};

struct FrameContext {
    uint32_t    frameIndex     = 0;
    uint32_t    imageIndex     = 0;
    VkImage     swapchainImage = VK_NULL_HANDLE;
    VkImageView swapchainView  = VK_NULL_HANDLE;
    VkImageView depthView      = VK_NULL_HANDLE;
    VkExtent2D  extent{};
    uint64_t    frameNumber    = 0;
};

class VulkanBase {
public:
    explicit VulkanBase(ExampleConfig cfg);
    virtual ~VulkanBase();

    VulkanBase(const VulkanBase&)            = delete;
    VulkanBase& operator=(const VulkanBase&) = delete;

    int run();

    unsigned validationMessageCount() const {
        return context_ ? context_->validationMessageCount() : 0u;
    }

protected:
    virtual void onFeatures(FeatureChain&) {}
    virtual void onInit() = 0;
    virtual void onUpdate(float) {}
    virtual void onRender(VkCommandBuffer, const FrameContext&) = 0;
    virtual void onResize(uint32_t, uint32_t) {}
    virtual void onEvent(const SDL_Event&) {}

    VulkanContext&       context()   { return *context_; }
    VulkanSwapchain&     swapchain() { return *swapchain_; }
    VulkanFrames&        frames()    { return *frames_; }
    const ExampleConfig& config() const { return config_; }

    std::string shaderPath(const std::string& file) const;

private:
    void initWindow();
    void initVulkan();
    void recreateSwapchain();
    bool renderFrame();

    ExampleConfig                    config_;
    SDL_Window*                      window_    = nullptr;
    VkSurfaceKHR                     surface_   = VK_NULL_HANDLE;

    std::unique_ptr<VulkanContext>   context_;
    std::unique_ptr<VulkanSwapchain> swapchain_;
    std::unique_ptr<VulkanFrames>    frames_;

    uint64_t frameNumber_   = 0;
    bool     needsRecreate_ = false;
};
