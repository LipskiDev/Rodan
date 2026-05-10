#pragma once

#include "core/input_types.h"
#include "glm/ext/matrix_float4x4.hpp"
namespace Rodan {

class Camera {
public:
  virtual ~Camera() = default;
  virtual void Update(float dt) = 0;
  virtual void OnMouseMove(float dx, float dy) = 0;
  virtual void OnKeyboard(InputEvent input) = 0;

  const glm::mat4 &GetView() const;
  const glm::mat4 &GetProjection() const;

  virtual void SetPerspective(float fovDegrees, float aspect, float nearPlane,
                              float farPlane) = 0;
  virtual void SetPosition(glm::vec3 position) = 0;
  virtual void LookAt(glm::vec3 lookAt) = 0;

  virtual void Reset() = 0;

protected:
  glm::mat4 view_;
  glm::mat4 projection_;
  glm::vec3 worldUp_ = {0.0f, 1.0f, 0.0f};
};

} // namespace Rodan
