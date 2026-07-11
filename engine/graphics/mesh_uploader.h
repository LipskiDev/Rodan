#pragma once

#include "assets/imported_scene.h"
#include "graphics/mesh_resource.h"
#include "rhi/device.h"
#include "rhi/upload_context.h"
#include <memory>
namespace Rodan {

using namespace Velos::RHI;
class MeshUploader {
public:
  static std::shared_ptr<MeshResource>
  Upload(IDevice *device, IUploadContext *upload, const ImportedMesh &mesh);
};
} // namespace Rodan
