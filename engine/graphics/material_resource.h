#pragma once

#include "graphics/texture.h"
#include "rhi/rhi_handles.h"
#include <glm/glm.hpp>

namespace Rodan {
using namespace Velos::RHI;

struct MaterialResource {
  Texture baseColor{};

  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;

  DescriptorSetHandle descriptorSet{};
  bool ownsBaseColorResources = false;
};

} // namespace Rodan
