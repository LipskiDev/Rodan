#pragma once

#include "glm/glm.hpp"

namespace Rodan {

class Camera {
public:
  const glm::mat4 &GetView() const;
  const glm::mat4 &GetProjection() const;

protected:
  glm::mat4 view_;
  glm::mat4 projection_;
  glm::vec3 worldUp_ = {0.0f, 1.0f, 0.0f};
};

} // namespace Rodan
