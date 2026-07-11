#pragma once

#include "graphics/material_types.h"
#include "graphics/texture.h"
#include "rhi/handles.h"
#include <glm/glm.hpp>

namespace Rodan {
using namespace Velos::RHI;

struct TransmissionMaterial {
  float transmissionFactor = 0.0f;
  Texture transmissionTexture{};
  bool ownsTransmissionTexture = false;
};

struct VolumeMaterial {
  float thicknessFactor = 0.0f;
  Texture thicknessTexture{};
  bool ownsVolumeTexture = false;

  glm::vec3 attenuationColor = glm::vec3(1.0f);
  float attenuationDistance = 0.0f;
};

struct ClearcoatMaterial {
  float factor = 0.0f;
  Texture texture;
  bool ownsClearcoatTexture = false;
  float roughnessFactor = 0.0f;
  Texture roughnessTexture;
  bool ownsClearcoatRoughnessTexture = false;
  Texture normalTexture;
  bool ownsClearcoatNormalTexture = false;
};

struct EmissiveMaterial {
  glm::vec3 factor{0.0, 0.0, 0.0};
  Texture texture;
  bool ownsEmissiveTexture = false;
  float strength = 0.0;
};

struct MaterialResource {
  Texture baseColorTexture{};
  Texture normalTexture{};
  Texture metallicRoughnessTexture{};
  Texture occlusionTexture{};

  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  BindingSetHandle descriptorSet{};

  bool ownsBaseColorResources = false;
  bool ownsNormalResources = false;
  bool ownsMetallicRoughnessResources = false;
  bool ownsOcclusionTextureResources = false;

  bool hasBaseColorTexture = false;
  bool hasNormalTexture = false;
  bool hasMetallicRoughnessTexture = false;

  float ior = 1.5f;

  AlphaMode alphaMode;
  float alphaCutoff;
  bool doubleSided;

  // Extensions
  TransmissionMaterial transmission;
  VolumeMaterial volume;
  ClearcoatMaterial clearcoat;
  EmissiveMaterial emissive;
  bool useUnlit = false;
};

} // namespace Rodan
