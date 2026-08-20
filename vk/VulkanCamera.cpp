#include "VulkanCamera.h"

#include <algorithm>
#include <cmath>

void VulkanCamera::setRotation(float yawDegrees, float pitchDegrees) {
    yaw_   = yawDegrees;
    pitch_ = std::clamp(pitchDegrees, -89.0f, 89.0f);
    updateVectors();
}

void VulkanCamera::setPerspective(float fovDegrees, float aspect, float nearZ, float farZ) {
    fov_    = fovDegrees;
    aspect_ = aspect;
    near_   = nearZ;
    far_    = farZ;
}

void VulkanCamera::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        yaw_   += event.motion.xrel * sensitivity;
        pitch_ -= event.motion.yrel * sensitivity;
        pitch_  = std::clamp(pitch_, -89.0f, 89.0f);
        updateVectors();
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        fov_ = std::clamp(fov_ - event.wheel.y, 1.0f, 90.0f);
    }
}

void VulkanCamera::update(float dt) {
    dt = std::min(dt, 0.1f);
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (!keys) return;
    
    float speed = moveSpeed * dt;
    if (keys[SDL_SCANCODE_LSHIFT]) speed *= 4.0f;

    if (keys[SDL_SCANCODE_R])     reset();
    if (keys[SDL_SCANCODE_W])     position_ += front_   * speed;
    if (keys[SDL_SCANCODE_S])     position_ -= front_   * speed;
    if (keys[SDL_SCANCODE_A])     position_ -= right_   * speed;
    if (keys[SDL_SCANCODE_D])     position_ += right_   * speed;
    if (keys[SDL_SCANCODE_SPACE]) position_ += worldUp_ * speed;
    if (keys[SDL_SCANCODE_LCTRL]) position_ -= worldUp_ * speed;
}

glm::mat4 VulkanCamera::view() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 VulkanCamera::projection() const {
    glm::mat4 proj = glm::perspective(glm::radians(fov_), aspect_, near_, far_);
    proj[1][1] *= -1.0f;
    return proj;
}

void VulkanCamera::reset(){
    yaw_    = -90.0f;
    pitch_  =   0.0f;
    position_ = {0.0f, 0.0f, 0.0f};
    fov_      = 45.f;
    updateVectors();
}

void VulkanCamera::updateVectors() {
    const float yawRad   = glm::radians(yaw_);
    const float pitchRad = glm::radians(pitch_);

    glm::vec3 f;
    f.x = std::cos(yawRad) * std::cos(pitchRad);
    f.y = std::sin(pitchRad);
    f.z = std::sin(yawRad) * std::cos(pitchRad);

    front_ = glm::normalize(f);
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    up_    = glm::normalize(glm::cross(right_, front_));
}
