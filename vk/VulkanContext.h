#pragma once

#include "VulkanCommon.h"

#include <vma/vk_mem_alloc.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

struct FeatureChain {
    void require(const char* deviceExtension) { extensions.push_back(deviceExtension); }

    template <typename T>
    T& add(T features) {
        static_assert(alignof(T) <= alignof(std::max_align_t), "FeatureChain::add<T>: T over-aligned for make_unique<unsigned char[]> storage");
        static_assert(std::is_trivially_destructible_v<T>, "FeatureChain::add<T>: T destructor would never run");
        auto  storage = std::make_unique<unsigned char[]>(sizeof(T));
        T*    slot    = new (storage.get()) T(features);
        blocks.push_back(std::move(storage));
        chain(slot);
        return *slot;
    }

    VkPhysicalDeviceFeatures&         features() { return f10; }
    VkPhysicalDeviceVulkan11Features& vulkan11() { return f11; }
    VkPhysicalDeviceVulkan12Features& vulkan12() { return f12; }
    VkPhysicalDeviceVulkan13Features& vulkan13() { return f13; }
    VkPhysicalDeviceVulkan14Features& vulkan14() { return f14; }

    std::vector<const char*> extensions;

    VkPhysicalDeviceFeatures        f10{};
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan14Features f14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
    void*                            head = nullptr;

private:
    void chain(void* block) {
        auto* asChain = reinterpret_cast<VkBaseOutStructure*>(block);
        while (asChain->pNext != nullptr) {
            asChain = asChain->pNext;
        }
        asChain->pNext = reinterpret_cast<VkBaseOutStructure*>(head);
        head           = block;
    }
    std::vector<std::unique_ptr<unsigned char[]>> blocks;
};

struct VulkanContextConfig {
    PFN_vkGetInstanceProcAddr loader = nullptr;
    std::vector<const char*>  instanceExtensions;
    bool                      enableValidation = false;
    bool                      requireSwapchain = true;
};

class VulkanContext {
public:
    struct Enabled {
        VkBool32 dynamicRendering     = VK_FALSE;
        VkBool32 synchronization2     = VK_FALSE;
        VkBool32 bufferDeviceAddress  = VK_FALSE;
        VkBool32 timelineSemaphore    = VK_FALSE;
        VkBool32 scalarBlockLayout    = VK_FALSE;
        VkBool32 maintenance5         = VK_FALSE;
        VkBool32 pushDescriptor       = VK_FALSE;
    };

    explicit VulkanContext(const VulkanContextConfig&               config,
                           const std::function<void(FeatureChain&)>& configureFeatures = {});
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&)                 = delete;
    VulkanContext& operator=(VulkanContext&&)      = delete;

    VkInstance       instance()       const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkDevice         device()         const { return device_; }
    VkQueue          graphicsQueue()  const { return graphicsQueue_; }
    uint32_t         graphicsFamily() const { return graphicsFamily_; }
    VmaAllocator     allocator()      const { return allocator_; }
    const char*      deviceName()     const { return deviceName_; }
    const Enabled&   enabled()        const { return enabled_; }

    unsigned         validationMessageCount() const { return validationMessages_; }

    VkCommandBuffer beginOneShot();
    void            endOneShot(VkCommandBuffer cmd);

private:
    void createInstance(const VulkanContextConfig&);
    void pickPhysicalDevice(bool requireSwapchain);
    void createDevice(const VulkanContextConfig&, const std::function<void(FeatureChain&)>&);
    void createAllocator();

    VkInstance               instance_           = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger_          = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice_     = VK_NULL_HANDLE;
    VkDevice                 device_             = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue_      = VK_NULL_HANDLE;
    uint32_t                 graphicsFamily_     = UINT32_MAX;
    VmaAllocator             allocator_          = nullptr;
    VkCommandPool            oneShotPool_        = VK_NULL_HANDLE;
    char                     deviceName_[256]    = {};
    Enabled                  enabled_{};

public:
    unsigned validationMessages_ = 0;
};
