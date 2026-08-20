#pragma once

#include "VulkanPipeline.h"
#include "VulkanResources.h"

struct DescriptorSlot {
    uint32_t         offset = 0;
    VkDescriptorType type   = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint32_t         size   = 0;
};



class VulkanDescriptorHeap {
public:
    VulkanDescriptorHeap() = default;
    VulkanDescriptorHeap(VulkanContext& ctx);
    

    ~VulkanDescriptorHeap()                                        = default;
    VulkanDescriptorHeap(const VulkanDescriptorHeap&)              = delete;
    VulkanDescriptorHeap& operator=(const VulkanDescriptorHeap&)   = delete;
    VulkanDescriptorHeap(VulkanDescriptorHeap&&) noexcept          = default;
    VulkanDescriptorHeap& operator=(VulkanDescriptorHeap&&) noexcept = default; 


    DescriptorSlot allocate(VulkanContext& ctx, VkDescriptorType type);
    void writeBuffer(DescriptorSlot slot, VkDeviceAddress address, VkDeviceSize size);
    void writeImage(DescriptorSlot slot, const VkImageViewCreateInfo& view, VkImageLayout layout);

    void reserve(VulkanContext& ctx, VkDeviceSize byteCapacity);
    void bind(VkCommandBuffer cmd) const;

    VkDeviceSize    used()       const { return cursor_;}
    VkDeviceSize    size()       const { return buffer_.size(); }
    const void*     mapped()     const { return buffer_.mapped(); }
    VkDeviceAddress address() const { return buffer_.address(); }

private:
    VkDeviceSize sizeFor(VkDescriptorType type) const;
    VkDeviceSize alignmentFor(VkDescriptorType type) const;


    VkDevice     device_          = VK_NULL_HANDLE;
    VulkanBuffer buffer_;
    VkDeviceSize reservedSize_    = 0;
    VkDeviceSize cursor_          = 0;

    VkPhysicalDeviceDescriptorHeapPropertiesEXT props_{};
};

namespace DescriptorHeap {

void requireFeature(FeatureChain& chain);

void chainMappings(PipelineBuildContext& ctx,
                       const VkDescriptorSetAndBindingMappingEXT* mappings,
                       uint32_t count);

}
