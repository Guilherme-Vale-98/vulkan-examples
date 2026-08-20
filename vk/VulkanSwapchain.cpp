#include "VulkanSwapchain.h"

#include <algorithm>

void VulkanSwapchain::create(VkSurfaceKHR surface, VkExtent2D extent,
                             VkPresentModeKHR presentMode, bool wantDepth,
                             VkImageUsageFlags swapchainUsage)
{
    surface_        = surface;
    presentMode_    = presentMode;
    wantDepth_      = wantDepth;
    swapchainUsage_ = swapchainUsage;

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx_.physicalDevice(), &familyCount, nullptr);
    for (uint32_t i = 0; i < familyCount; ++i) {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx_.physicalDevice(), i, surface_, &supported);
        if (supported) { presentFamily_ = i; break; }
    }
    if (presentFamily_ == UINT32_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "no queue family can present to this surface");
        std::abort();
    }
    if (presentFamily_ != ctx_.graphicsFamily()) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                     "present family %u != graphics family %u; unsupported by this base",
                     presentFamily_, ctx_.graphicsFamily());
        std::abort();
    }
    presentQueue_ = ctx_.graphicsQueue();

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physicalDevice(), surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx_.physicalDevice(), surface_, &formatCount, formats.data());

    if (formats.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "surface reports zero supported formats");
        std::abort();
    }

    format_     = formats[0].format;
    colorSpace_ = formats[0].colorSpace;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format_     = f.format;
            colorSpace_ = f.colorSpace;
            break;
        }
    }

    if (wantDepth_) depthFormat_ = VK_FORMAT_D32_SFLOAT;

    build(extent);
}

void VulkanSwapchain::recreate(VkExtent2D extent) {
    VK_CHECK(vkDeviceWaitIdle(ctx_.device()));
    build(extent);
}

void VulkanSwapchain::build(VkExtent2D extent) {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx_.physicalDevice(), surface_, &caps));

    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    extent_ = extent;

    uint32_t desired = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && desired > caps.maxImageCount) desired = caps.maxImageCount;

    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                     "surface does not support VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT "
                     "(supportedUsageFlags=0x%x); this base cannot present to it",
                     caps.supportedUsageFlags);
        std::abort();
    }
    const VkImageUsageFlags imageUsage =
        (swapchainUsage_ | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) & caps.supportedUsageFlags;

    uint32_t presentModeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physicalDevice(), surface_, &presentModeCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx_.physicalDevice(), surface_, &presentModeCount, presentModes.data()));

    VkPresentModeKHR chosenPresentMode = presentMode_;
    bool presentModeSupported = false;
    for (VkPresentModeKHR m : presentModes) {
        if (m == chosenPresentMode) { presentModeSupported = true; break; }
    }
    if (!presentModeSupported) {
        SDL_Log("[vulkan] present mode %d not supported by this surface, falling back to FIFO",
                static_cast<int>(chosenPresentMode));
        chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR alphaPreference[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (VkCompositeAlphaFlagBitsKHR mode : alphaPreference) {
        if (caps.supportedCompositeAlpha & mode) { compositeAlpha = mode; break; }
    }

    VkSwapchainKHR old = swapchain_;

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface          = surface_;
    info.minImageCount    = desired;
    info.imageFormat      = format_;
    info.imageColorSpace  = colorSpace_;
    info.imageExtent      = extent_;
    info.imageArrayLayers = 1;
    info.imageUsage       = imageUsage;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = compositeAlpha;
    info.presentMode      = chosenPresentMode;
    info.clipped          = VK_TRUE;
    info.oldSwapchain     = old;

    VK_CHECK(vkCreateSwapchainKHR(ctx_.device(), &info, nullptr, &swapchain_));

    destroyImageViews();
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(ctx_.device(), old, nullptr);

    uint32_t count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, nullptr));
    images_.resize(count);
    VK_CHECK(vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &count, images_.data()));

    views_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image                       = images_[i];
        viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                      = format_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(ctx_.device(), &viewInfo, nullptr, &views_[i]));
    }

    if (wantDepth_) {
        depth_ = VulkanResources::createImage(ctx_, extent_, depthFormat_,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void VulkanSwapchain::destroyImageViews() {
    for (VkImageView v : views_) {
        if (v != VK_NULL_HANDLE) vkDestroyImageView(ctx_.device(), v, nullptr);
    }
    views_.clear();
}

void VulkanSwapchain::destroy() {
    depth_.reset();
    destroyImageViews();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    images_.clear();
}
