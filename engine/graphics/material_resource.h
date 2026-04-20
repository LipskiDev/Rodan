#pragma once

#include "graphics/texture.h"
#include "rhi/rhi_handles.h"
#include <glm/glm.hpp>

namespace Rodan {
using namespace Velos::RHI;

struct MaterialResource {
  Texture baseColorTexture{};
  Texture normalTexture{};
  Texture metallicRoughnessTexture{};

  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;

  DescriptorSetHandle descriptorSet{};

  bool ownsBaseColorResources = false;
  bool ownsNormalResources = false;
  bool ownsMetallicRoughnessResources = false;

  bool hasBaseColorTexture = false;
  bool hasNormalTexture = false;
  bool hasMetallicRoughnessTexture = false;
};

} // namespace Rodan
