#include "VulkanContext.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       userData)
{
    const SDL_LogPriority priority =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? SDL_LOG_PRIORITY_ERROR :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? SDL_LOG_PRIORITY_WARN  :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    ? SDL_LOG_PRIORITY_INFO  :
                                                                       SDL_LOG_PRIORITY_VERBOSE;
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[validation] %s", data->pMessage);

    const VkDebugUtilsMessageTypeFlagsEXT countedTypes =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    const bool countIt = ((types & countedTypes) != 0 &&
                          severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ||
                         severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (countIt) {
        if (auto* ctx = static_cast<VulkanContext*>(userData)) {
            ++ctx->validationMessages_;
        }
    }
    return VK_FALSE;
}

bool hasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
    for (const auto& e : list) {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

}

VulkanContext::VulkanContext(const VulkanContextConfig&                config,
                             const std::function<void(FeatureChain&)>& configureFeatures){
    if (config.loader) {
        volkInitializeCustom(config.loader);
    } else {
        VK_CHECK(volkInitialize());
    }

    createInstance(config);
    pickPhysicalDevice(config.requireSwapchain);
    createDevice(config, configureFeatures);
    createAllocator();

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &oneShotPool_));
}

VulkanContext::~VulkanContext() {
    if (oneShotPool_) vkDestroyCommandPool(device_, oneShotPool_, nullptr);
    if (allocator_)   vmaDestroyAllocator(allocator_);
    if (device_)      vkDestroyDevice(device_, nullptr);
    if (messenger_)   vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
    if (instance_)    vkDestroyInstance(instance_, nullptr);
}

void VulkanContext::createInstance(const VulkanContextConfig& config) {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "VulkanSandbox";
    app.pEngineName      = "VulkanSandbox";
    app.apiVersion       = VK_API_VERSION_1_4;

    std::vector<const char*> extensions = config.instanceExtensions;
    std::vector<const char*> layers;
    if (config.enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkDebugUtilsMessengerCreateInfoEXT dbg{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg.pfnUserCallback = debugCallback;
    dbg.pUserData       = this;

    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo        = &app;
    info.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    info.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    info.ppEnabledLayerNames     = layers.data();
    info.pNext                   = config.enableValidation ? &dbg : nullptr;

    VK_CHECK(vkCreateInstance(&info, nullptr, &instance_));
    volkLoadInstanceOnly(instance_);

    if (config.enableValidation) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance_, &dbg, nullptr, &messenger_));
    }
}

void VulkanContext::pickPhysicalDevice(bool requireSwapchain) {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, nullptr));
    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, devices.data()));

    int bestScore = -1;
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(candidate, &props);
        if (props.apiVersion < VK_API_VERSION_1_4) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, exts.data());
        if (requireSwapchain && !hasExtension(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qCount, families.data());

        uint32_t family = UINT32_MAX;
        for (uint32_t i = 0; i < qCount; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { family = i; break; }
        }
        if (family == UINT32_MAX) continue;

        const int score =
            (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 100 : 10;
        if (score > bestScore) {
            bestScore       = score;
            physicalDevice_ = candidate;
            graphicsFamily_ = family;
            std::snprintf(deviceName_, sizeof(deviceName_), "%s", props.deviceName);
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No Vulkan 1.4 device with a graphics queue");
        std::abort();
    }
    SDL_Log("[vulkan] using %s", deviceName_);
}

void VulkanContext::createDevice(const VulkanContextConfig&                config,
                                 const std::function<void(FeatureChain&)>& configureFeatures)
{
    FeatureChain chain;
    if (config.requireSwapchain) chain.require(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    chain.vulkan12().bufferDeviceAddress = VK_TRUE;
    chain.vulkan12().timelineSemaphore   = VK_TRUE;
    chain.vulkan12().scalarBlockLayout   = VK_TRUE;
    chain.vulkan12().descriptorIndexing  = VK_TRUE;
    chain.vulkan13().dynamicRendering    = VK_TRUE;
    chain.vulkan13().synchronization2    = VK_TRUE;
    chain.vulkan14().maintenance5        = VK_TRUE;
    chain.vulkan14().maintenance6        = VK_TRUE;
    chain.vulkan14().pushDescriptor      = VK_TRUE;

    if (configureFeatures) configureFeatures(chain);

    chain.f11.pNext = &chain.f12;
    chain.f12.pNext = &chain.f13;
    chain.f13.pNext = &chain.f14;
    chain.f14.pNext = chain.head;

    VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext    = &chain.f11;
    features2.features = chain.f10;

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = graphicsFamily_;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.pNext                   = &features2;
    info.queueCreateInfoCount    = 1;
    info.pQueueCreateInfos       = &queueInfo;
    info.enabledExtensionCount   = static_cast<uint32_t>(chain.extensions.size());
    info.ppEnabledExtensionNames = chain.extensions.data();

    VK_CHECK(vkCreateDevice(physicalDevice_, &info, nullptr, &device_));
    volkLoadDevice(device_);
    setObjectName(device_, VK_OBJECT_TYPE_DEVICE,
              reinterpret_cast<uint64_t>(device_), "02.logica_device");


    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);

    enabled_.dynamicRendering    = chain.f13.dynamicRendering;
    enabled_.synchronization2    = chain.f13.synchronization2;
    enabled_.bufferDeviceAddress = chain.f12.bufferDeviceAddress;
    enabled_.timelineSemaphore   = chain.f12.timelineSemaphore;
    enabled_.scalarBlockLayout   = chain.f12.scalarBlockLayout;
    enabled_.maintenance5        = chain.f14.maintenance5;
    enabled_.pushDescriptor      = chain.f14.pushDescriptor;
}

void VulkanContext::createAllocator() {
    VmaVulkanFunctions fns{};
    fns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    fns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo info{};
    info.physicalDevice   = physicalDevice_;
    info.device           = device_;
    info.instance         = instance_;
    info.vulkanApiVersion = VK_API_VERSION_1_4;
    info.pVulkanFunctions = &fns;
    info.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&info, &allocator_));
}

VkCommandBuffer VulkanContext::beginOneShot() {
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool        = oneShotPool_;
    alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device_, &alloc, &cmd));

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    return cmd;
}

void VulkanContext::endOneShot(VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos    = &cmdInfo;

    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(graphicsQueue_));
    vkFreeCommandBuffers(device_, oneShotPool_, 1, &cmd);
}
