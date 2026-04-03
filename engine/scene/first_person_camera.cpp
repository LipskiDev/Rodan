#include "core/input_types.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <scene/first_person_camera.h>

namespace Rodan {

void FirstPersonCamera::Update(float dt) {
  const float moveSpeed = 5.0f;

  glm::vec3 forward = GetForward();
  forward.y = 0.0f;
  if (glm::length(forward) > 0.0f)
    forward = glm::normalize(forward);
  glm::vec3 right = GetRight();

  glm::vec3 moveDir(0.0f);

  if (movement_.forward)
    moveDir += forward;
  if (movement_.backward)
    moveDir -= forward;
  if (movement_.right)
    moveDir += right;
  if (movement_.left)
    moveDir -= right;

  if (glm::length(moveDir) > 0.0f) {
    moveDir = glm::normalize(moveDir);
    position_ += moveDir * moveSpeed * dt;
  }

  UpdateViewMatrix();
}

void FirstPersonCamera::OnMouseMove(float dx, float dy) {
  yaw_ += dx * mouseSensitivity_;
  pitch_ -= dy * mouseSensitivity_;

  pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);

  UpdateViewMatrix();
}

void FirstPersonCamera::OnKeyboard(InputEvent input) {
  bool pressed = (input.type == InputEventType::KeyDown);

  if (input.key.key == Key::W) {
    printf("Forward\n");
    movement_.forward = pressed;
  } else if (input.key.key == Key::S) {
    printf("Backward\n");
    movement_.backward = pressed;
  } else if (input.key.key == Key::A) {
    printf("Left\n");
    movement_.left = pressed;
  } else if (input.key.key == Key::D) {
    printf("Right\n");
    movement_.right = pressed;
  }
}

glm::vec3 FirstPersonCamera::GetForward() const {
  glm::vec3 forward;
  forward.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  forward.y = sin(glm::radians(pitch_));
  forward.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  return glm::normalize(forward);
}

glm::vec3 FirstPersonCamera::GetRight() const {
  return glm::normalize(glm::cross(GetForward(), worldUp_));
}

void FirstPersonCamera::UpdateViewMatrix() {
  glm::vec3 forward = GetForward();
  view_ = glm::lookAt(position_, position_ + forward, worldUp_);
}

void FirstPersonCamera::UpdateProjectionMatrix() {
  projection_ = glm::perspective(glm::radians(fovDegrees_), aspect_, nearPlane_,
                                 farPlane_);

  // Vulkan clip space
  projection_[1][1] *= -1.0f;
}

void FirstPersonCamera::SetPerspective(float fovDegrees, float aspect,
                                       float nearPlane, float farPlane) {
  fovDegrees_ = fovDegrees;
  aspect_ = aspect;
  nearPlane_ = nearPlane;
  farPlane_ = farPlane;
  UpdateProjectionMatrix();
}

} // namespace Rodan
