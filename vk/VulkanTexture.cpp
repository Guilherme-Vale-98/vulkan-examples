#include "VulkanTexture.h"
#include "stb_image.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>



namespace {

struct TextureBarrierInfo {
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
    uint32_t baseMipLevel;
    uint32_t levelCount;
};

using StbiPixels = std::unique_ptr<stbi_uc, void (*)(void*)>;

struct DecodedRgba8 {
    StbiPixels pixels{nullptr, stbi_image_free};
    uint32_t width = 0;
    uint32_t height = 0;
    VkDeviceSize byteSize = 0;
};

DecodedRgba8 decodeRgba8(const char* path) {
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    StbiPixels pixels{
        stbi_load(path,
                  &width,
                  &height,
                  &sourceChannels,
                  STBI_rgb_alpha),
        stbi_image_free,
    };

    if (!pixels) {
        throw std::runtime_error(
            std::string{"failed to decode texture "} + path +
            ": " + stbi_failure_reason());
    }

    constexpr VkDeviceSize channelCount = STBI_rgb_alpha;
    const VkDeviceSize width64  = static_cast<VkDeviceSize>(width);
    const VkDeviceSize height64 = static_cast<VkDeviceSize>(height);

    if (width64 > std::numeric_limits<VkDeviceSize>::max() /
                      height64 / channelCount) {
        throw std::runtime_error(
            std::string{"texture byte size overflows VkDeviceSize: "} + path);
    }

    return DecodedRgba8{
        .pixels   = std::move(pixels),
        .width    = static_cast<uint32_t>(width),
        .height   = static_cast<uint32_t>(height),
        .byteSize = width64 * height64 * channelCount,
    };
}

VkFormat rgba8Format(TEXTURE_COLOR_SPACE colorSpace) {
    return colorSpace == TEXTURE_COLOR_SPACE::SRGB
        ? VK_FORMAT_R8G8B8A8_SRGB
        : VK_FORMAT_R8G8B8A8_UNORM;
}
void textureBarrier(VkCommandBuffer cmd,
                    VkImage image,
                    const TextureBarrierInfo& info) {

    VkImageMemoryBarrier2 barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2
    };
    barrier.srcStageMask  = info.srcStage;
    barrier.srcAccessMask = info.srcAccess;
    barrier.dstStageMask  = info.dstStage;
    barrier.dstAccessMask = info.dstAccess;
    barrier.oldLayout     = info.oldLayout;
barrier.newLayout     = info.newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = info.baseMipLevel;
    barrier.subresourceRange.levelCount     = info.levelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(cmd, &dependency);
}

}
VkImageViewCreateInfo VulkanTexture::viewInfo() const {
    assert(image_ != VK_NULL_HANDLE);
    assert(mipLevels_ != 0);

    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image    = image_;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format   = format_;
    view.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel   = 0;
    view.subresourceRange.levelCount     = mipLevels_;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount     = 1;
    return view;
}

//decode the file and allocate mapped buffer

VulkanTexture VulkanTextures::loadRgba8(VulkanContext& ctx,
                        const char* path,
                        TEXTURE_COLOR_SPACE colorSpace, MIP_MODE mipMode){
    const VkFormat format = rgba8Format(colorSpace);
    DecodedRgba8 decoded = decodeRgba8(path);
    const VkExtent2D extent{decoded.width, decoded.height};
    const uint32_t mipLevels = mipMode == MIP_MODE::GENERATE
        ? VulkanTextures::mipLevelCount(extent.width, extent.height)
        : 1u;
    
    if (mipLevels > 1 && !supportsLinearBlit(ctx.physicalDevice(), format)) {
      throw std::runtime_error(
        "Texture format does not support linear blit mip generation");
    }


    VulkanBuffer staging = VulkanResources::createBuffer(
        ctx, decoded.byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);

    std::memcpy(staging.mapped(), decoded.pixels.get(),
        static_cast<size_t>(decoded.byteSize));

    staging.flush(0, decoded.byteSize);
    
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = format;
    imageInfo.extent        = {extent.width, extent.height, 1};
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;

    if (mipLevels > 1) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    VK_CHECK(vmaCreateImage(
        ctx.allocator(),
        &imageInfo,
        &allocationInfo,
        &image,
        &allocation,
        nullptr));

    VulkanTexture texture{
        ctx.allocator(),
        image,
        allocation,
        format,
        extent,
        mipLevels,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkCommandBuffer cmd = ctx.beginOneShot();
    textureBarrier(
        cmd,
        texture.handle(),{
        .oldLayout    = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcStage     = VK_PIPELINE_STAGE_2_NONE,
        .srcAccess    = VK_ACCESS_2_NONE,
        .dstStage     = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT,
        .dstAccess    = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .baseMipLevel = 0,
        .levelCount   = mipLevels,
    });
 
    VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {extent.width, extent.height, 1};

    VkCopyBufferToImageInfo2 copy{
        VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2
    };
    copy.srcBuffer      = staging.handle();
    copy.dstImage       = texture.handle();
    copy.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy.regionCount    = 1;
    copy.pRegions       = &region;

    vkCmdCopyBufferToImage2(cmd, &copy);


    const VkPipelineStageFlags2 transferStages = VK_PIPELINE_STAGE_2_COPY_BIT |
        VK_PIPELINE_STAGE_2_BLIT_BIT;
    int32_t sourceWidth  = static_cast<int32_t>(extent.width);
    int32_t sourceHeight = static_cast<int32_t>(extent.height);

    for (uint32_t destinationMip = 1; 
        destinationMip < mipLevels; ++destinationMip) {

        const uint32_t sourceMip = destinationMip - 1;

        textureBarrier(cmd, texture.handle(), {
            .oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcStage     = transferStages,
            .srcAccess    = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStage     = VK_PIPELINE_STAGE_2_BLIT_BIT,
            .dstAccess    = VK_ACCESS_2_TRANSFER_READ_BIT,
            .baseMipLevel = sourceMip,
            .levelCount   = 1,
        });

        const int32_t destinationWidth  = std::max(1, sourceWidth / 2);
        const int32_t destinationHeight = std::max(1, sourceHeight / 2);

        VkImageBlit2 blitRegion{
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,

            .srcSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = sourceMip,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .srcOffsets = {
                {0, 0, 0},
                {sourceWidth, sourceHeight, 1},
            },

            .dstSubresource = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = destinationMip,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .dstOffsets = {
                {0, 0, 0},
                {destinationWidth, destinationHeight, 1},
            },
        };

        VkBlitImageInfo2 blitInfo{
            .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = texture.handle(),
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = texture.handle(),
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &blitRegion,
            .filter         = VK_FILTER_LINEAR,
        };

        vkCmdBlitImage2(cmd, &blitInfo);

        sourceWidth  = destinationWidth;
        sourceHeight = destinationHeight;
      }

    if (mipLevels > 1) {
        textureBarrier(cmd, texture.handle(), {
            .oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcStage     = transferStages,
            .srcAccess    = VK_ACCESS_2_TRANSFER_READ_BIT |
                        VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStage     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccess    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            .baseMipLevel = 0,
            .levelCount   = mipLevels - 1,
        });
    }

    textureBarrier(cmd, texture.handle(), {
        .oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcStage     = transferStages,
        .srcAccess    = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStage     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccess    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .baseMipLevel = mipLevels - 1,
        .levelCount   = 1,
    });         

    ctx.endOneShot(cmd);

    return texture;
} 

uint32_t VulkanTextures::mipLevelCount(uint32_t width, uint32_t height){
    uint32_t counter = 0;
    uint32_t max = std::max(width, height);
    while(max > 0){
      max = max/2;
      counter++;
    }
    return counter;
}

bool VulkanTextures::supportsLinearBlit(VkPhysicalDevice physicalDevice, VkFormat format){

    VkFormatProperties3 features{
        VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3
    };

    VkFormatProperties2 properties{
        VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
    };
    properties.pNext = &features;

    vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &properties);

    const VkFormatFeatureFlags2 required =
        VK_FORMAT_FEATURE_2_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_2_BLIT_DST_BIT |
        VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    const VkFormatFeatureFlags2 available = features.optimalTilingFeatures;
    const VkFormatFeatureFlags2 missing = required & ~available;
    
    return (missing == 0);
}



