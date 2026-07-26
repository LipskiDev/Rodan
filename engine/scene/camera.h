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
  virtual void UpdateProjectionMatrix() = 0;

  const glm::mat4 &GetView() const;
  const glm::mat4 &GetProjection() const;
  const glm::mat4& GetProjectionFromCascade(uint32_t cascade) const;
  float GetCascadeFarDistance(uint32_t cascade) const;


  virtual void SetPerspective(float fovDegrees, float aspect, float nearPlane,
                              float farPlane) = 0;
  virtual void SetPosition(glm::vec3 position) = 0;
  virtual void LookAt(glm::vec3 lookAt) = 0;
  void SetCascades(uint32_t cascadeCount, std::vector<float> cascades,
                   float maxDistance);
  uint32_t GetCascadeCount() const{
      return cascadeCount_;
  }

  virtual void Reset() = 0;

  glm::mat4 fakeView_ = glm::mat4(1.0);
  std::vector<glm::mat4> fakeProjs_ = {};
  void SetFakeAsCurrent();

protected:
  glm::mat4 view_;
  std::vector<glm::mat4> projections_;
  std::vector<glm::mat4> cascadeProjections_;
  std::vector<float> cascadeFarDistances_;
  glm::vec3 worldUp_ = {0.0f, 1.0f, 0.0f};
  uint32_t cascadeCount_ = 0;
  std::vector<float> cascades_;
  float cascadeMaxDistance_ = 250.0f;
};

} // namespace Rodan
