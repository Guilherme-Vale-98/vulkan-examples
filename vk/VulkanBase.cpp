#include "VulkanBase.h"

#include <SDL3/SDL_vulkan.h>

#include <vector>

VulkanBase::VulkanBase(ExampleConfig cfg) : config_(std::move(cfg)) {}

VulkanBase::~VulkanBase() {
    frames_.reset();
    swapchain_.reset();
    if (surface_ != VK_NULL_HANDLE && context_) {
        vkDestroySurfaceKHR(context_->instance(), surface_, nullptr);
    }
    context_.reset();

    if (window_) SDL_DestroyWindow(window_);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

std::string VulkanBase::shaderPath(const std::string& file) const {
    return shaderDir(config_.name) + file;
}

void VulkanBase::initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Init failed: %s", SDL_GetError());
        std::abort();
    }
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Vulkan_LoadLibrary failed: %s", SDL_GetError());
        std::abort();
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    if (config_.hidden) flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(config_.title.c_str(),
                               static_cast<int>(config_.width),
                               static_cast<int>(config_.height),
                               flags);
    if (!window_) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_CreateWindow failed: %s", SDL_GetError());
        std::abort();
    }
}

void VulkanBase::initVulkan() {
    uint32_t extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);

    VulkanContextConfig cfg{};
    cfg.loader             = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                                 SDL_Vulkan_GetVkGetInstanceProcAddr());
    cfg.instanceExtensions = std::vector<const char*>(sdlExts, sdlExts + extCount);
    cfg.requireSwapchain   = true;
#ifdef _DEBUG
    cfg.enableValidation = true;
#else
    cfg.enableValidation = config_.forceValidation;
#endif

    context_ = std::make_unique<VulkanContext>(
        cfg, [this](FeatureChain& chain) { onFeatures(chain); });

    if (!SDL_Vulkan_CreateSurface(window_, context_->instance(), nullptr, &surface_)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        std::abort();
    }

    swapchain_ = std::make_unique<VulkanSwapchain>(*context_);
    swapchain_->create(surface_, VkExtent2D{config_.width, config_.height},
                       config_.presentMode, config_.depth, config_.swapchainUsage);

    frames_ = std::make_unique<VulkanFrames>(*context_);
    frames_->create(config_.framesInFlight, swapchain_->imageCount());
}

void VulkanBase::recreateSwapchain() {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    if (w <= 0 || h <= 0) return;

    swapchain_->recreate(VkExtent2D{static_cast<uint32_t>(w), static_cast<uint32_t>(h)});
    frames_->recreateImageSemaphores(swapchain_->imageCount());
    needsRecreate_ = false;
    onResize(swapchain_->extent().width, swapchain_->extent().height);
}

bool VulkanBase::renderFrame() {
    const uint32_t frameIndex = static_cast<uint32_t>(frameNumber_ % config_.framesInFlight);

    frames_->waitForFrame(frameNumber_);

    uint32_t imageIndex = 0;
    VkResult acquired = vkAcquireNextImageKHR(
        context_->device(), swapchain_->handle(), UINT64_MAX,
        frames_->imageAvailable(frameIndex), VK_NULL_HANDLE, &imageIndex);

    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "vkAcquireNextImageKHR: %s", vkResultString(acquired));
        std::abort();
    }

    frames_->resetPool(frameIndex);
    VkCommandBuffer cmd = frames_->commandBuffer(frameIndex);

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    transitionImage(cmd, swapchain_->image(imageIndex),
                    VK_IMAGE_LAYOUT_UNDEFINED, config_.renderLayout,
                    VK_IMAGE_ASPECT_COLOR_BIT);

    if (swapchain_->depthView() != VK_NULL_HANDLE) {
        transitionImage(cmd, swapchain_->depthImage(),
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    FrameContext frame{};
    frame.frameIndex     = frameIndex;
    frame.imageIndex     = imageIndex;
    frame.swapchainImage = swapchain_->image(imageIndex);
    frame.swapchainView  = swapchain_->view(imageIndex);
    frame.depthView      = swapchain_->depthView();
    frame.extent         = swapchain_->extent();
    frame.frameNumber    = frameNumber_;

    VkViewport viewport{0.0f, 0.0f,
                        static_cast<float>(frame.extent.width),
                        static_cast<float>(frame.extent.height),
                        0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, frame.extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    onRender(cmd, frame);

    transitionImage(cmd, swapchain_->image(imageIndex),
                    config_.renderLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = frames_->imageAvailable(frameIndex);
    wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSemaphoreSubmitInfo signals[2]{};
    signals[0].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signals[0].semaphore = frames_->renderFinished(imageIndex);
    signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signals[1].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signals[1].semaphore = frames_->timeline();
    signals[1].value     = frameNumber_ + 1;
    signals[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = cmd;

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount   = 1;
    submit.pWaitSemaphoreInfos      = &wait;
    submit.commandBufferInfoCount   = 1;
    submit.pCommandBufferInfos      = &cmdInfo;
    submit.signalSemaphoreInfoCount = 2;
    submit.pSignalSemaphoreInfos    = signals;

    VK_CHECK(vkQueueSubmit2(context_->graphicsQueue(), 1, &submit, VK_NULL_HANDLE));

    VkSwapchainKHR chain = swapchain_->handle();
    VkSemaphore    presentWait = frames_->renderFinished(imageIndex);

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &presentWait;
    present.swapchainCount     = 1;
    present.pSwapchains        = &chain;
    present.pImageIndices      = &imageIndex;

    const VkResult presented = vkQueuePresentKHR(swapchain_->presentQueue(), &present);

    ++frameNumber_;

    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) return false;
    if (presented != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "vkQueuePresentKHR: %s", vkResultString(presented));
        std::abort();
    }
    return true;
}

int VulkanBase::run() {
    initWindow();
    initVulkan();
    onInit();

    bool     running = true;
    uint64_t lastTicks = SDL_GetTicksNS();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
            if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) needsRecreate_ = true;
            onEvent(event);
        }
        if (!running) break;

        const uint64_t now = SDL_GetTicksNS();
        const float    dt  = static_cast<float>(now - lastTicks) / 1e9f;
        lastTicks = now;
        onUpdate(dt);

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        if (w <= 0 || h <= 0) { SDL_Delay(10); continue; }

        if (needsRecreate_) { recreateSwapchain(); continue; }
        if (!renderFrame())  { recreateSwapchain(); continue; }

        if (config_.exitAfterFrames != 0 && frameNumber_ >= config_.exitAfterFrames) {
            running = false;
        }
    }

    VK_CHECK(vkDeviceWaitIdle(context_->device()));
    return 0;
}
