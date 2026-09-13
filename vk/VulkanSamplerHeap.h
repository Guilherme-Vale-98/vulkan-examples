#pragma once

#include "VulkanContext.h"
#include "VulkanResources.h"
#include <cstdint>

struct SamplerSlot{
    uint32_t offset = 0;
    uint32_t size   = 0;
};

class VulkanSamplerHeap {
public:
  VulkanSamplerHeap() = default;
  explicit VulkanSamplerHeap(VulkanContext& ctx);

  ~VulkanSamplerHeap()= default;

  VulkanSamplerHeap(VulkanSamplerHeap &&) = default;
  VulkanSamplerHeap(const VulkanSamplerHeap &) = delete;

  VulkanSamplerHeap &operator=(VulkanSamplerHeap &&) = default;
  VulkanSamplerHeap &operator=(const VulkanSamplerHeap &) = delete;
  
  SamplerSlot allocate(VulkanContext& ctx);
  void write(SamplerSlot slot, const VkSamplerCreateInfo& sampler);

  void reserve(VulkanContext& ctx, VkDeviceSize byteCapacity);
  void bind(VkCommandBuffer cmd) const;

  VkDeviceSize used() const { return cursor_; }
  VkDeviceSize size() const { return buffer_.size(); }
  const void* mapped() const { return buffer_.mapped(); }
  VkDeviceAddress address() const { return buffer_.address(); }

private:
  VkDevice device_ = VK_NULL_HANDLE;
  VulkanBuffer buffer_;
  VkDeviceSize reservedSize_ = 0;
  VkDeviceSize cursor_ = 0;
  VkPhysicalDeviceDescriptorHeapPropertiesEXT props_{}; 
};


