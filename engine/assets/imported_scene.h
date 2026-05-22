#pragma once

#include "scene/bounding_box.h"
#include "scene/transform.h"
#include <glm/glm.hpp>
#include <graphics/material_types.h>
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
  bool hasTangents;

  AABB localBounds;
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

  ImportedTextureRef baseColorTexture;
  ImportedTextureRef normalTexture;
  ImportedTextureRef metallicRoughnessTexture;
  ImportedTextureRef occlusionTexture;

  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutoff = 0.5f;
  bool doubleSided = false;
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

  AABB localBounds;
};

struct ImportedNode {
  Transform localTransform; // raw glTF node transform
  Transform worldTransform; // accumulated parent * local

  int meshIndex = -1;
  std::vector<int> children;
  std::string name;
};

struct ImportedScene {
  std::vector<ImportedMesh> meshes;
  std::vector<ImportedNode> nodes;
  std::vector<int> rootNodes;
  std::vector<ImportedMaterial> materials;
  std::vector<ImportedImage> images;
  std::vector<ImportedSampler> samplers;
  AABB worldBounds;
};

} // namespace Rodan
