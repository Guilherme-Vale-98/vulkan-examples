#include "VulkanDescriptorHeap.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {
VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) {

    // gets the minimum multiple of a >= v;
    return a ? (v + a - 1) / a * a : v;
}
}


void DescriptorHeap::requireFeature(FeatureChain& chain) {
    chain.require(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);

    VkPhysicalDeviceDescriptorHeapFeaturesEXT heap{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT};
    heap.descriptorHeap = VK_TRUE;
    chain.add(heap);
}

void DescriptorHeap::chainMappings(PipelineBuildContext& ctx,
                       const VkDescriptorSetAndBindingMappingEXT* mappings,
                       uint32_t count)
{
    VkShaderDescriptorSetAndBindingMappingInfoEXT mapping{
        VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
    mapping.mappingCount = count;
    mapping.pMappings    = mappings;
    for (uint32_t i = 0; i < ctx.stageCount; ++i) {
        ctx.chain(ctx.stages[i].pNext, mapping);
    }

    VkPipelineCreateFlags2CreateInfo flags{
        VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO};
    flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
    ctx.chain(ctx.info.pNext, flags);
}
VkDeviceSize VulkanDescriptorHeap::sizeFor(VkDescriptorType type) const {
    switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return props_.bufferDescriptorSize;
        default:
            return props_.imageDescriptorSize;
    }
}
VkDeviceSize VulkanDescriptorHeap::alignmentFor(VkDescriptorType type) const {
    switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return props_.bufferDescriptorAlignment;
        default:
            return props_.imageDescriptorAlignment;
    }
}
VulkanDescriptorHeap::VulkanDescriptorHeap(VulkanContext& ctx) {
    props_ = ctx.properties(VkPhysicalDeviceDescriptorHeapPropertiesEXT{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT});

    assert(props_.bufferDescriptorSize != 0);

    reservedSize_ = props_.minResourceHeapReservedRange;
    cursor_       = reservedSize_;
    assert(reservedSize_ <= props_.maxResourceHeapSize);
    

    reserve(ctx, reservedSize_ + 2);
}

void VulkanDescriptorHeap::reserve(VulkanContext& ctx, VkDeviceSize byteCapacity) {
    if (byteCapacity <= buffer_.size()) {
        return;
    }

    assert(props_.bufferDescriptorSize != 0);
    assert(byteCapacity <= props_.maxResourceHeapSize);

    VulkanBuffer bigger = VulkanResources::createBuffer(ctx, byteCapacity,
                                       VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT |
                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true,false, props_.resourceHeapAlignment);

    assert(bigger.mapped() != nullptr);
    assert(bigger.address() % props_.resourceHeapAlignment == 0);

    if (buffer_.mapped() != nullptr && cursor_ > reservedSize_) {
        std::memcpy(static_cast<unsigned char*>(bigger.mapped())      + reservedSize_,
                    static_cast<const unsigned char*>(buffer_.mapped()) + reservedSize_,
                    static_cast<size_t>(cursor_ - reservedSize_));
    }

    VK_CHECK(vkDeviceWaitIdle(ctx.device()));

    device_ = ctx.device();
    buffer_ = std::move(bigger);
}

DescriptorSlot VulkanDescriptorHeap::allocate(VulkanContext& ctx, VkDescriptorType type) {
    const VkDeviceSize size  = sizeFor(type);
    const VkDeviceSize align = alignmentFor(type);

    assert(size  != 0);
    assert(align != 0);

    const VkDeviceSize offset = alignUp(cursor_, align);
    const VkDeviceSize end    = offset + size;

    if (end > buffer_.size()) {
        reserve(ctx, std::max(end, buffer_.size() * 2));
    }

    cursor_ = end;

    return DescriptorSlot{
        .offset = static_cast<uint32_t>(offset),
        .type   = type,
        .size   = static_cast<uint32_t>(size),
    };
}

void VulkanDescriptorHeap::writeBuffer(DescriptorSlot slot,
                                       VkDeviceAddress address, VkDeviceSize size)
{
    assert(slot.size != 0);
    assert(slot.offset + slot.size <= cursor_);

    VkDeviceAddressRangeEXT range{.address = address, .size = size};

    VkResourceDescriptorInfoEXT resource{VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT};
    resource.type               = slot.type;
    resource.data.pAddressRange = &range;

    VkHostAddressRangeEXT dst{
        .address = static_cast<unsigned char*>(buffer_.mapped()) + slot.offset,
        .size    = slot.size,
    };

    VK_CHECK(vkWriteResourceDescriptorsEXT(device_, 1, &resource, &dst));
}

void VulkanDescriptorHeap::writeImage(DescriptorSlot slot,
                                      const VkImageViewCreateInfo& view, VkImageLayout layout)
{
    assert(slot.size != 0);
    assert(slot.offset + slot.size <= cursor_);

    VkImageDescriptorInfoEXT image{VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT};
    image.pView  = &view;
    image.layout = layout;

    VkResourceDescriptorInfoEXT resource{VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT};
    resource.type       = slot.type;
    resource.data.pImage = &image;

    VkHostAddressRangeEXT dst{
        .address = static_cast<unsigned char*>(buffer_.mapped()) + slot.offset,
        .size    = slot.size,
    };

    VK_CHECK(vkWriteResourceDescriptorsEXT(device_, 1, &resource, &dst));
}

void VulkanDescriptorHeap::bind(VkCommandBuffer cmd) const {
    VkBindHeapInfoEXT info{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    info.heapRange.address   = buffer_.address();
    info.heapRange.size      = buffer_.size();
    info.reservedRangeOffset = 0;
    info.reservedRangeSize   = reservedSize_;
    vkCmdBindResourceHeapEXT(cmd, &info);
}

