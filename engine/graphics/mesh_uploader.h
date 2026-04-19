#pragma once

#include "assets/imported_scene.h"
#include "graphics/mesh_resource.h"
#include "rhi/rhi_device.h"
#include "rhi/rhi_upload_context.h"
#include <memory>
namespace Rodan {

using namespace Velos::RHI;
class MeshUploader {
public:
  static std::shared_ptr<MeshResource>
  Upload(IDevice *device, IUploadContext *upload, const ImportedMesh &mesh);
};
} // namespace Rodan
