#pragma once

#include "assets/imported_scene.h"
#include "graphics/mesh_resource.h"
#include "rhi/rhi_device.h"
#include <memory>
namespace Rodan {

using namespace Velos::RHI;
class MeshUploader {
public:
  static std::shared_ptr<MeshResource> Upload(IDevice *device,
                                              const ImportedMesh &mesh);
};
} // namespace Rodan
