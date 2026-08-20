#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Volk/volk.h>
#include <SDL3/SDL_log.h>

#include <cstdlib>

namespace VulkanCommon {

const char* resultString(VkResult result);

void setObjectName(VkDevice device, VkObjectType type, uint64_t handle, const char* name);

}


#define VK_CHECK(expr)                                                        \
    do {                                                                      \
        const VkResult vkCheckResult_ = (expr);                               \
        if (vkCheckResult_ != VK_SUCCESS) {                                   \
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s:%d  %s failed: %s",      \
                         __FILE__, __LINE__, #expr,                           \
                         VulkanCommon::resultString(vkCheckResult_));                     \
            std::abort();                                                     \
        }                                                                     \
    } while (0)


