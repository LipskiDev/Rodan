#include "core/input_types.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <scene/first_person_camera.h>
#include <scene/orbit_camera.h>

namespace Rodan {

void OrbitCamera::Update(float dt) {
  (void)dt;
  if (autoRotate_) {
    yaw_ += dt * autoRotateSpeed_ * 100;
  }
  UpdateViewMatrix();
}

void OrbitCamera::OnMouseMove(float dx, float dy) {
  yaw_ += dx * mouseSensitivity_;
  pitch_ -= dy * mouseSensitivity_;

  pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);

  UpdateViewMatrix();
}

void OrbitCamera::OnKeyboard(InputEvent input) {
  bool pressed = (input.type == InputEventType::KeyDown);

  if (!pressed) {
    return;
  }

  if (input.key.key == Key::W) {
    distance_ *= 0.9f; // zoom in
  } else if (input.key.key == Key::S) {
    distance_ *= 1.1f; // zoom out
  } else if (input.key.key == Key::P) {
    SetAutoRotate(!autoRotate_);
  } else if (input.key.key == Key::UpArrow) {
    SetAutoRotateSpeed(autoRotateSpeed_ + 1);
  } else if (input.key.key == Key::DownArrow) {
    SetAutoRotateSpeed(autoRotateSpeed_ - 1);
  }

  distance_ = glm::clamp(distance_, 0.1f, 1000.0f);

  UpdateViewMatrix();
}

glm::vec3 OrbitCamera::GetForward() const {
  glm::vec3 forward;
  forward.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  forward.y = sin(glm::radians(pitch_));
  forward.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
  return glm::normalize(forward);
}

glm::vec3 OrbitCamera::GetRight() const {
  return glm::normalize(glm::cross(GetForward(), worldUp_));
}

void OrbitCamera::UpdateViewMatrix() {
  glm::vec3 forward = GetForward();

  position_ = target_ - forward * distance_;

  view_ = glm::lookAt(position_, target_, worldUp_);
}

void OrbitCamera::UpdateProjectionMatrix() {
  projections_.clear();
  cascadeProjections_.clear();
  cascadeFarDistances_.clear();
  glm::mat4 projection = glm::perspectiveRH_ZO(
      glm::radians(fovDegrees_), aspect_, nearPlane_, farPlane_);

  projection[1][1] *= -1.0f;

  projections_.push_back(projection);
  for (uint32_t i = 0; i < cascadeCount_; ++i) {
      const float nominalNearFraction = i == 0 ? 0.0f : cascades_[i - 1];
      const float previousNearFraction = i <= 1 ? 0.0f : cascades_[i - 2];
      const float nearFraction = i == 0
          ? 0.0f
          : nominalNearFraction -
                (nominalNearFraction - previousNearFraction) * 0.10f;
      const float farFraction =
          i + 1 == cascadeCount_ ? 1.0f : cascades_[i];
      const float shadowFar = glm::clamp(
          cascadeMaxDistance_, nearPlane_ + 0.001f, farPlane_);
      const float near =
          nearPlane_ + (shadowFar - nearPlane_) * nearFraction;
      const float far =
          nearPlane_ + (shadowFar - nearPlane_) * farFraction;

      glm::mat4 cascadeProjection = glm::perspectiveRH_ZO(
          glm::radians(fovDegrees_), aspect_, near, far);
      cascadeProjection[1][1] *= -1.0f;
      cascadeProjections_.push_back(cascadeProjection);
      cascadeFarDistances_.push_back(far);
  }
}

void OrbitCamera::SetPerspective(float fovDegrees, float aspect,
                                 float nearPlane, float farPlane) {
  fovDegrees_ = fovDegrees;
  aspect_ = aspect;
  nearPlane_ = nearPlane;
  farPlane_ = farPlane;
  UpdateProjectionMatrix();
}

void OrbitCamera::SetPosition(glm::vec3 position) {
  position_ = position;

  glm::vec3 dir = glm::normalize(target_ - position_);
  distance_ = glm::length(target_ - position_);

  pitch_ = glm::degrees(std::asin(dir.y));
  yaw_ = glm::degrees(std::atan2(dir.z, dir.x));

  UpdateViewMatrix();
}

void OrbitCamera::LookAt(glm::vec3 target) {
  target_ = target;
  UpdateViewMatrix();
}

void OrbitCamera::SetTarget(glm::vec3 target) {
  target_ = target;
  UpdateViewMatrix();
}

void OrbitCamera::SetDistance(float distance) {
  distance_ = glm::max(distance, 0.1f);
  UpdateViewMatrix();
}

void OrbitCamera::Reset() {
  target_ = glm::vec3(0.0f);
  distance_ = 5.0f;

  yaw_ = -90.0f;
  pitch_ = 20.0f;

  movement_ = {};

  UpdateViewMatrix();
}

} // namespace Rodan
