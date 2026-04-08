#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Rodan {
struct ImportedVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

struct ImportedPrimitive {
  std::vector<ImportedVertex> vertices;
  std::vector<uint32_t> indices;
  int materialIndex = -1;
};

struct ImportedMesh {
  std::string name;
  std::vector<ImportedPrimitive> primitives;
};

struct ImportedNode {
  glm::mat4 transform{1.0f};
  int meshIndex = -1;
  std::vector<int> children;
};

struct ImportedScene {
  std::vector<ImportedMesh> meshes;
  std::vector<ImportedNode> nodes;
  std::vector<int> rootNodes;
};

} // namespace Rodan
