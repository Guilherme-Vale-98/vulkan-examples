#include "VulkanResources.h"

#include <cstring>

VulkanBuffer VulkanResources::createBuffer(VulkanContext& ctx, VkDeviceSize size,
                          VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                          bool hostVisible, bool hostRandomAccess, VkDeviceSize minAlignment)
{
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size  = size;
    info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = memoryUsage;
    if (hostVisible) {
        alloc.flags = (hostRandomAccess ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                        : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VkBuffer          buffer{};
    VmaAllocation     allocation{};
    VmaAllocationInfo allocInfo{};
    
    VkResult result;
    if (minAlignment != 0) {
        result = vmaCreateBufferWithAlignment(
            ctx.allocator(),
            &info,
            &alloc,
            minAlignment,
            &buffer,
            &allocation,
            &allocInfo);
    }

    if (minAlignment == 0){
        result = vmaCreateBuffer(
            ctx.allocator(),
            &info,
            &alloc,
            &buffer,
            &allocation,
            &allocInfo);
    }
    
    VK_CHECK(result);


    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addressInfo.buffer = buffer;
    const VkDeviceAddress address = vkGetBufferDeviceAddress(ctx.device(), &addressInfo);

    return VulkanBuffer(ctx.allocator(), buffer, allocation, size, address,
                        hostVisible ? allocInfo.pMappedData : nullptr);
}

VulkanBuffer VulkanResources::createBufferWithData(VulkanContext& ctx, const void* data,
                                  VkDeviceSize size, VkBufferUsageFlags usage)
{
    VulkanBuffer staging = VulkanResources::createBuffer(ctx, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);
    std::memcpy(staging.mapped(), data, static_cast<size_t>(size));

    VulkanBuffer result = VulkanResources::createBuffer(ctx, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, false);

    VkCommandBuffer cmd = ctx.beginOneShot();
    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, staging.handle(), result.handle(), 1, &copy);
    ctx.endOneShot(cmd);

    return result;
}

void VulkanResources::readbackBuffer(VulkanContext& ctx, const VulkanBuffer& src,
                    void* dst, VkDeviceSize size)
{
    VulkanBuffer staging = VulkanResources::createBuffer(ctx, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true, true);

    VkCommandBuffer cmd = ctx.beginOneShot();
    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, src.handle(), staging.handle(), 1, &copy);
    ctx.endOneShot(cmd);

    staging.invalidate();
    std::memcpy(dst, staging.mapped(), static_cast<size_t>(size));
}

VulkanImage VulkanResources::createImage(VulkanContext& ctx, VkExtent2D extent, VkFormat format,
                        VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = format;
    info.extent        = {extent.width, extent.height, 1};
    info.mipLevels     = 1;
    info.arrayLayers   = 1;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = usage;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage       image{};
    VmaAllocation allocation{};
    VK_CHECK(vmaCreateImage(ctx.allocator(), &info, &alloc, &image, &allocation, nullptr));

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image                       = image;
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view{};
    VK_CHECK(vkCreateImageView(ctx.device(), &viewInfo, nullptr, &view));

    return VulkanImage(ctx.device(), ctx.allocator(), image, allocation, view, format, extent);
}

void VulkanResources::transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout from, VkImageLayout to,
                     VkImageAspectFlags aspect)
{
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.oldLayout     = from;
    barrier.newLayout     = to;
    barrier.image         = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(cmd, &dep);
}
