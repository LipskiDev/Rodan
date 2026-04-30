#pragma once

#include "assets/imported_scene.h"
#include "graphics/material_resource.h"
#include "rhi/rhi_command_list.h"
#include "scene/static_mesh_instance.h"
namespace Rodan {
using namespace Velos::RHI;
class MeshRenderer {
public:
  void Draw(ICommandList *cmd, const StaticMeshInstance &instance,
            const std::vector<MaterialResource> &materials,
            PipelineHandle pipeline);
  void DrawSubmesh(ICommandList *cmd, const StaticMeshInstance &instance,
                   const Submesh &submesh, const MaterialResource *material,
                   PipelineHandle);
};
} // namespace Rodan
