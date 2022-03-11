#include "camera.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "device.h"

Camera::Camera() {
    look_at = glm::vec3(0.0f, 0.0f, 0.0f);
    position = glm::vec3(0.0f, 0.0f, -5.0f);
}

Camera::~Camera() {}

glm::mat4 Camera::GetViewProj() {
    float sin_phi = sin(phi_);
    if (sin_phi == 0.0f) {
        phi_ += 0.00001f;
        sin_phi = sin(phi_);
    }

    float r = glm::length(position);
    glm::vec3 dir_to_camera = glm::vec3(sin_phi * sin(theta_), cos(phi_), sin_phi * cos(theta_));
    position = look_at + r * dir_to_camera;

    vk::Extent2D viewport = Device::Get()->swapchain_extent();
    auto proj = glm::perspectiveFov(glm::pi<float>() / 3.0f, (float)viewport.width, (float)viewport.height, 0.1f, 100.0f);
    auto view = glm::lookAt(position, look_at, glm::vec3(0.0f, 1.0f, 0.0f));
    proj[1][1]  *= -1.0f;
    return proj * view;
}

void Camera::RotateBy(float phi, float theta) {
    phi_ += phi;
    theta_ += theta;

    phi_ = glm::clamp(phi_, 0.001f, glm::pi<float>() - 0.001f);
}
