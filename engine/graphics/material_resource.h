#pragma once

#include "rhi/rhi_handles.h"
#include <glm/glm.hpp>

namespace Rodan {
using namespace Velos::RHI;

struct MaterialResource {
  glm::vec4 baseColorFactor{1.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;

  ImageHandle baseColorImage{};
  ImageViewHandle baseColorImageView{};
  SamplerHandle baseColorSampler{};
  BufferHandle baseColorStagingBuffer{};

  DescriptorSetHandle descriptorSet{};
  uint32_t baseColorWidth = 1;
  uint32_t baseColorHeight = 1;
  bool uploaded = false;
  bool ownsBaseColorResources = false;
};

} // namespace Rodan
