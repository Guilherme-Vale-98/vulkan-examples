#pragma once

#include "VulkanResources.h"
#include <cstdint>
#include <utility>
enum class TEXTURE_COLOR_SPACE{
    LINEAR,
    SRGB,
};

enum class MIP_MODE{
    BASE, GENERATE
};


class VulkanTexture {
public:
    VulkanTexture() = default;
    VulkanTexture(VmaAllocator allocator,
                  VkImage image,
                  VmaAllocation allocation,
                  VkFormat format,
                  VkExtent2D extent,
                  uint32_t mipLevels,
                  VkImageLayout layout)
        : allocator_(allocator),
          image_(image),
          allocation_(allocation),
          format_(format),
          extent_(extent),
          mipLevels_(mipLevels),
          layout_(layout) {}


    VulkanTexture(VulkanTexture&& other) noexcept { swap(other); }
    VulkanTexture &operator=(VulkanTexture&& other) noexcept {
        if(this != &other){
          reset();
          swap(other);
        }
        return *this;
    }

    VulkanTexture &operator=(const VulkanTexture &) = delete;
    VulkanTexture(const VulkanTexture &) = delete;
    ~VulkanTexture(){reset();};
    void reset() {
        if (image_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, image_, allocation_);
            image_ = VK_NULL_HANDLE;
            allocation_ = nullptr;
        }
        allocator_ = nullptr;
        format_ = VK_FORMAT_UNDEFINED;
        extent_ = {};
        mipLevels_ = 0;
        layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkImage handle() const { return image_; }
    VkFormat format() const { return format_; }
    VkExtent2D extent() const { return extent_; }
    uint32_t mipLevels() const { return mipLevels_; }
    VkImageLayout layout() const { return layout_; }

    VkImageViewCreateInfo viewInfo() const;


private:

    void swap(VulkanTexture& other) noexcept {
        std::swap(allocator_, other.allocator_);
        std::swap(image_, other.image_);
        std::swap(allocation_, other.allocation_);
        std::swap(format_, other.format_);
        std::swap(extent_, other.extent_);
        std::swap(mipLevels_, other.mipLevels_);
        std::swap(layout_, other.layout_);
    }
    VmaAllocator allocator_ = nullptr;
    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    uint32_t mipLevels_ = 0;
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  
};
namespace VulkanTextures {

VulkanTexture loadRgba8(VulkanContext& ctx,
                        const char* path,
                        TEXTURE_COLOR_SPACE colorSpace, MIP_MODE mipMode);
bool supportsLinearBlit(VkPhysicalDevice physicalDevice, VkFormat format);

uint32_t mipLevelCount(uint32_t width, uint32_t height);
}
