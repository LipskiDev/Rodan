
#pragma once

#include "core/input_types.h"
#include "scene/camera.h"
#include <cwchar>

namespace Rodan {

class OrbitCamera : public Camera {
public:
  void Update(float dt);
  void OnMouseMove(float dx, float dy);
  void OnKeyboard(InputEvent input);

  void SetPerspective(float fovDegrees, float aspect, float nearPlane,
                      float farPlane) override;

  void SetPosition(glm::vec3 position) override;
  void LookAt(glm::vec3 lookAt) override;

  void SetTarget(glm::vec3 target);
  void SetDistance(float distance);

  void Reset() override;

private:
  glm::vec3 position_;
  float yaw_;
  float pitch_;

  float fovDegrees_ = 60.0f;
  float aspect_ = 16.0f / 9.0f;
  float nearPlane_ = 0.1f;
  float farPlane_ = 5000.0f;

  float mouseSensitivity_ = 0.5f;

  float distance_ = 50.0f;
  glm::vec3 target_ = {};

  glm::vec3 GetForward() const;
  glm::vec3 GetRight() const;
  void UpdateViewMatrix();
  void UpdateProjectionMatrix();

  struct MovementState {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
  } movement_;
};

} // namespace Rodan
