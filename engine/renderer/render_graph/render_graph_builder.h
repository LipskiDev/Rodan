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

enum class RenderGraphAccess {
  ReadColorAttachment,
  ReadTexture,
  WriteColorAttachment,
  WriteDepthAttachment,
  CopySrc,
  CopyDst,
  StorageRead,
  StorageWrite,
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
  RenderGraphAccess access;

  Velos::RHI::ImageLayout requiredLayout;
  Velos::RHI::ResourceState requiredState;
  SubresourceRange range;
};

class RenderGraphBuilder {
public:
  void ReadColorAttachment(const std::string &resource,
                           SubresourceRange range = {});
  void ReadTexture(const std::string &resource, SubresourceRange range = {});
  void WriteColorAttachment(const std::string &resource,
                            SubresourceRange range = {});
  void WriteDepthAttachment(const std::string &resource,
                            SubresourceRange range = {});
  void CopySrc(const std::string &resource, SubresourceRange range = {});
  void CopyDst(const std::string &resource, SubresourceRange range = {});
  void StorageRead(const std::string &resource, SubresourceRange range = {});
  void StorageWrite(const std::string &resource, SubresourceRange range = {});

private:
  friend class RenderGraph;

  void AddAccess(const std::string &resource, RenderGraphAccess access,
                 SubresourceRange range);

  std::vector<ResourceAccess> accesses_;
};

} // namespace Rodan
