#pragma once

#include "rhi/rhi_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Rodan {

enum class AccessType {
  Read,
  Write,
};

struct SubresourceRange {
  uint32_t baseMip = 0;
  uint32_t mipCount = UINT32_MAX;
  uint32_t baseLayer = 0;
  uint32_t layerCount = UINT32_MAX;
};

struct ResourceAccess {
  std::string name;
  AccessType type;

  Velos::RHI::ImageLayout requiredLayout;
  SubresourceRange range;
};

class RenderGraphBuilder {
public:
  void Read(const std::string &resource, Velos::RHI::ImageLayout layout,
            SubresourceRange range = {});
  void Write(const std::string &resource, Velos::RHI::ImageLayout layout,
             SubresourceRange range = {});

private:
  friend class RenderGraph;

  std::vector<ResourceAccess> accesses_;
};

} // namespace Rodan
