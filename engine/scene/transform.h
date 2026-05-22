#pragma once

#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/quaternion.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Rodan {
struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 scale = glm::vec3(1.0f);

  inline glm::mat4 ToMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

    return T * R * S;
  }

  static Transform FromMatrix(glm::mat4 matrix) {
    Transform transform{};

    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(matrix, transform.scale, transform.rotation,
                   transform.position, skew, perspective);

    return transform;
  }
};
} // namespace Rodan
