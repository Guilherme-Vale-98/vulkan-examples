#pragma once

#include "VulkanCommon.h"

#include <SDL3/SDL.h>

class VulkanCamera {
public:
    float moveSpeed   = 2.5f;
    float sensitivity = 0.1f;

    void setPosition(const glm::vec3& p) { position_ = p; }
    void setRotation(float yawDegrees, float pitchDegrees);
    void setPerspective(float fovDegrees, float aspect, float nearZ, float farZ);
    void setAspect(float aspect) { aspect_ = aspect; }

    void handleEvent(const SDL_Event& event);
    void update(float dt);
    void reset();

    glm::mat4 view() const;
    glm::mat4 projection() const;
    glm::mat4 viewProjection() const { return projection() * view(); }

    const glm::vec3& position() const { return position_; }
    const glm::vec3& front()    const { return front_; }
    float            fov()      const { return fov_; }

private:
    void updateVectors();

    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    glm::vec3 front_{0.0f, 0.0f, -1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    glm::vec3 worldUp_{0.0f, 1.0f, 0.0f};

    float yaw_    = -90.0f;
    float pitch_  =   0.0f;
    float fov_    =  45.0f;
    float aspect_ =  16.0f / 9.0f;
    float near_   =   0.1f;
    float far_    = 100.0f;
};
