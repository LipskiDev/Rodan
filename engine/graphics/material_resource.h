#pragma once

#include "graphics/material_types.h"
#include "graphics/texture.h"
#include "rhi/rhi_handles.h"
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

struct MaterialResource {
  Texture baseColorTexture{};
  Texture normalTexture{};
  Texture metallicRoughnessTexture{};
  Texture occlusionTexture{};

  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;

  DescriptorSetHandle descriptorSet{};

  bool ownsBaseColorResources = false;
  bool ownsNormalResources = false;
  bool ownsMetallicRoughnessResources = false;
  bool ownsOcclusionTextureResources = false;

  bool hasBaseColorTexture = false;
  bool hasNormalTexture = false;
  bool hasMetallicRoughnessTexture = false;

  AlphaMode alphaMode;
  float alphaCutoff;
  bool doubleSided;

  // Extensions
  TransmissionMaterial transmission;
  VolumeMaterial volume;
};

} // namespace Rodan
