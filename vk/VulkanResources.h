#pragma once

#include "VulkanContext.h"

#include <utility>

class VulkanBuffer {
public:
    VulkanBuffer() = default;
    VulkanBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation,
                 VkDeviceSize size, VkDeviceAddress address, void* mapped)
        : allocator_(allocator), buffer_(buffer), allocation_(allocation),
          size_(size), address_(address), mapped_(mapped) {}

    ~VulkanBuffer() { reset(); }

    VulkanBuffer(const VulkanBuffer&)            = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept { swap(other); }
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) { reset(); swap(other); }
        return *this;
    }

    VkBuffer        handle()  const { return buffer_; }
    VkDeviceAddress address() const { return address_; }
    VkDeviceSize    size()    const { return size_; }
    void*           mapped()  const { return mapped_; }

    void invalidate() {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaInvalidateAllocation(allocator_, allocation_, 0, VK_WHOLE_SIZE);
        }
    }

    void reset() {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
            buffer_     = VK_NULL_HANDLE;
            allocation_ = nullptr;
            address_    = 0;
            mapped_     = nullptr;
            size_       = 0;
        }
    }

private:
    void swap(VulkanBuffer& o) noexcept {
        std::swap(allocator_, o.allocator_);
        std::swap(buffer_, o.buffer_);
        std::swap(allocation_, o.allocation_);
        std::swap(size_, o.size_);
        std::swap(address_, o.address_);
        std::swap(mapped_, o.mapped_);
    }

    VmaAllocator    allocator_  = nullptr;
    VkBuffer        buffer_     = VK_NULL_HANDLE;
    VmaAllocation   allocation_ = nullptr;
    VkDeviceSize    size_       = 0;
    VkDeviceAddress address_    = 0;
    void*           mapped_     = nullptr;
};

class VulkanImage {
public:
    VulkanImage() = default;
    VulkanImage(VkDevice device, VmaAllocator allocator, VkImage image,
                VmaAllocation allocation, VkImageView view, VkFormat format,
                VkExtent2D extent)
        : device_(device), allocator_(allocator), image_(image),
          allocation_(allocation), view_(view), format_(format), extent_(extent) {}

    ~VulkanImage() { reset(); }

    VulkanImage(const VulkanImage&)            = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    VulkanImage(VulkanImage&& other) noexcept { swap(other); }
    VulkanImage& operator=(VulkanImage&& other) noexcept {
        if (this != &other) { reset(); swap(other); }
        return *this;
    }

    VkImage     handle() const { return image_; }
    VkImageView view()   const { return view_; }
    VkFormat    format() const { return format_; }
    VkExtent2D  extent() const { return extent_; }

    void reset() {
        if (view_  != VK_NULL_HANDLE) { vkDestroyImageView(device_, view_, nullptr); view_ = VK_NULL_HANDLE; }
        if (image_ != VK_NULL_HANDLE) { vmaDestroyImage(allocator_, image_, allocation_); image_ = VK_NULL_HANDLE; allocation_ = nullptr; }
    }

private:
    void swap(VulkanImage& o) noexcept {
        std::swap(device_, o.device_);
        std::swap(allocator_, o.allocator_);
        std::swap(image_, o.image_);
        std::swap(allocation_, o.allocation_);
        std::swap(view_, o.view_);
        std::swap(format_, o.format_);
        std::swap(extent_, o.extent_);
    }

    VkDevice      device_     = VK_NULL_HANDLE;
    VmaAllocator  allocator_  = nullptr;
    VkImage       image_      = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    VkImageView   view_       = VK_NULL_HANDLE;
    VkFormat      format_     = VK_FORMAT_UNDEFINED;
    VkExtent2D    extent_{};
};

namespace VulkanResources {

VulkanBuffer createBuffer(VulkanContext& ctx, VkDeviceSize size,
                          VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                          bool hostVisible, bool hostRandomAccess = false, VkDeviceSize minAlignment = 0);

VulkanBuffer createBufferWithData(VulkanContext& ctx, const void* data,
                                  VkDeviceSize size, VkBufferUsageFlags usage);

void readbackBuffer(VulkanContext& ctx, const VulkanBuffer& src,
                    void* dst, VkDeviceSize size);

VulkanImage createImage(VulkanContext& ctx, VkExtent2D extent, VkFormat format,
                        VkImageUsageFlags usage, VkImageAspectFlags aspect);

void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout from, VkImageLayout to,
                     VkImageAspectFlags aspect);

}


