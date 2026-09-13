
#include "VulkanSamplerHeap.h"

#include <algorithm>
#include <cassert>
#include <cstring>
namespace {
VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) {
    // gets the minimum multiple of a >= v;
    return a ? (v + a - 1) / a * a : v;
}
}


VulkanSamplerHeap::VulkanSamplerHeap(VulkanContext& ctx){
    props_ = ctx.properties(VkPhysicalDeviceDescriptorHeapPropertiesEXT{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT});

    assert(props_.samplerDescriptorSize != 0);
    assert(props_.samplerDescriptorAlignment != 0);
    assert(props_.samplerHeapAlignment != 0);

    reservedSize_ = props_.minSamplerHeapReservedRange;
    cursor_       = reservedSize_;

    assert(reservedSize_ <= props_.maxSamplerHeapSize);
    assert(props_.samplerDescriptorSize <= props_.maxSamplerHeapSize);

    const VkDeviceSize firstOffset =
        alignUp(cursor_, props_.samplerDescriptorAlignment);

    assert(firstOffset <=
           props_.maxSamplerHeapSize - props_.samplerDescriptorSize);

    reserve(ctx, firstOffset + props_.samplerDescriptorSize);
}

void VulkanSamplerHeap::reserve(VulkanContext& ctx, VkDeviceSize byteCapacity){
    if(byteCapacity <= buffer_.size()){
        return;

    }

    assert(byteCapacity <= props_.maxSamplerHeapSize);

    VulkanBuffer bigger = VulkanResources::createBuffer(
        ctx,
        byteCapacity,
        VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        true,
        false,
        props_.samplerHeapAlignment);

    assert(bigger.mapped() != nullptr);
    //VUID-vkCmdBindSamplerHeapEXT-pBindInfo-11226
    assert(bigger.address() % props_.samplerHeapAlignment == 0);


    if(buffer_.handle() != VK_NULL_HANDLE){
        VK_CHECK(vkDeviceWaitIdle(ctx.device()));
        if(cursor_ > reservedSize_){
            std::memcpy( 
                static_cast<unsigned char*>(bigger.mapped()) + reservedSize_,
                static_cast<const unsigned char*>(buffer_.mapped()) + reservedSize_,
                static_cast<size_t>(cursor_ - reservedSize_));
            
            bigger.flush(reservedSize_, cursor_ - reservedSize_);
        }
    }

    device_ = ctx.device();
    buffer_ = std::move(bigger);
}

void VulkanSamplerHeap::bind(VkCommandBuffer cmd) const {
    VkBindHeapInfoEXT info{VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    info.heapRange.address   = buffer_.address();
    info.heapRange.size      = buffer_.size();
    info.reservedRangeOffset = 0;
    info.reservedRangeSize   = reservedSize_;
    vkCmdBindSamplerHeapEXT(cmd, &info);  
}

SamplerSlot VulkanSamplerHeap::allocate(VulkanContext& ctx) {
    const VkDeviceSize descriptorSize = props_.samplerDescriptorSize;
    const VkDeviceSize alignment      = props_.samplerDescriptorAlignment;

    const VkDeviceSize offset = alignUp(cursor_, alignment);

    assert(offset <= props_.maxSamplerHeapSize - descriptorSize);
    const VkDeviceSize end = offset + descriptorSize;

    if (end > buffer_.size()) {
        const VkDeviceSize doubled =
            buffer_.size() > props_.maxSamplerHeapSize / 2
                ? props_.maxSamplerHeapSize
                : buffer_.size() * 2;

        reserve(ctx, std::max(end, doubled));
    }

    assert(offset % alignment == 0);
    assert(offset <= std::numeric_limits<uint32_t>::max());
    assert(descriptorSize <= std::numeric_limits<uint32_t>::max());

    cursor_ = end;

    return SamplerSlot{
        .offset = static_cast<uint32_t>(offset),
        .size   = static_cast<uint32_t>(descriptorSize),
    };
}

void VulkanSamplerHeap::write(
    SamplerSlot slot,
    const VkSamplerCreateInfo& sampler) {

    assert(slot.size >= props_.samplerDescriptorSize);
    assert(slot.offset % props_.samplerDescriptorAlignment == 0);
    assert(VkDeviceSize(slot.offset) + slot.size <= cursor_);

    VkHostAddressRangeEXT destination{
        .address = static_cast<unsigned char*>(buffer_.mapped()) + slot.offset,
        .size    = slot.size,
    };

    VK_CHECK(vkWriteSamplerDescriptorsEXT(
        device_, 1, &sampler, &destination));

    buffer_.flush(slot.offset, slot.size);
}








