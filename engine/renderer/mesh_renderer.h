#pragma once

#include "graphics/material_resource.h"
#include "graphics/mesh_resource.h"
#include "rhi/rhi_command_list.h"
namespace Rodan {
using namespace Velos::RHI;
class MeshRenderer {
public:
  void Draw(ICommandList *cmd, const MeshResource &mesh,
            const std::vector<const MaterialResource *> &materials,
            PipelineHandle pipeline);

  void DrawSubmesh(ICommandList *cmd, const MeshResource &mesh,
                   const Submesh &submesh, const MaterialResource *material,
                   PipelineHandle pipeline);
  void DrawDepthOnly(ICommandList *cmd, const MeshResource &mesh);
};
} // namespace Rodan
