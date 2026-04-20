#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Rodan {
struct ImportedVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec4 tangent;
};

struct ImportedPrimitive {
  std::vector<ImportedVertex> vertices;
  std::vector<uint32_t> indices;
  int materialIndex = -1;
};

struct ImportedTextureRef {
  int imageIndex = -1;
  int samplerIndex = -1;
  int texCoord = 0;
};

struct ImportedMaterial {
  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  bool doubleSided = false;

  ImportedTextureRef baseColorTexture;
  ImportedTextureRef normalTexture;
  ImportedTextureRef metallicRoughnessTexture;
};

struct ImportedImage {
  std::string name;
  int width = 0;
  int height = 0;
  int components = 0;
  std::vector<std::uint8_t> pixelsRGBA8;
};

struct ImportedSampler {
  int minFilter = -1;
  int magFilter = -1;
  int wrapS = -1;
  int wrapT = -1;
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
  std::vector<ImportedMaterial> materials;
  std::vector<ImportedImage> images;
  std::vector<ImportedSampler> samplers;
};

} // namespace Rodan
