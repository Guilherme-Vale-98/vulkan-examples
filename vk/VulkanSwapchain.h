#pragma once

#include "VulkanContext.h"
#include "VulkanResources.h"

#include <vector>

class VulkanSwapchain {
public:
    explicit VulkanSwapchain(VulkanContext& ctx) : ctx_(ctx) {}
    ~VulkanSwapchain() { destroy(); }

    VulkanSwapchain(const VulkanSwapchain&)            = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    void create(VkSurfaceKHR surface, VkExtent2D extent,
                VkPresentModeKHR presentMode, bool wantDepth,
                VkImageUsageFlags swapchainUsage =
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    void recreate(VkExtent2D extent);
    void destroy();

    VkSwapchainKHR handle()      const { return swapchain_; }
    VkFormat       format()      const { return format_; }
    VkFormat       depthFormat() const { return depthFormat_; }
    VkExtent2D     extent()      const { return extent_; }
    uint32_t       imageCount()  const { return static_cast<uint32_t>(images_.size()); }
    VkImage        image(uint32_t i) const { return images_[i]; }
    VkImageView    view(uint32_t i)  const { return views_[i]; }
    VkImageView    depthView()   const { return depth_.view(); }
    VkImage        depthImage()  const { return depth_.handle(); }
    VkQueue        presentQueue()  const { return presentQueue_; }
    uint32_t       presentFamily() const { return presentFamily_; }

private:
    void build(VkExtent2D extent);
    void destroyImageViews();

    VulkanContext&           ctx_;
    VkSurfaceKHR             surface_       = VK_NULL_HANDLE;
    VkSwapchainKHR           swapchain_     = VK_NULL_HANDLE;
    VkFormat                 format_        = VK_FORMAT_UNDEFINED;
    VkFormat                 depthFormat_   = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR          colorSpace_    = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR         presentMode_   = VK_PRESENT_MODE_FIFO_KHR;
    VkImageUsageFlags        swapchainUsage_ =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkExtent2D               extent_{};
    bool                     wantDepth_     = false;
    std::vector<VkImage>     images_;
    std::vector<VkImageView> views_;
    VulkanImage              depth_;
    VkQueue                  presentQueue_  = VK_NULL_HANDLE;
    uint32_t                 presentFamily_ = UINT32_MAX;
};
