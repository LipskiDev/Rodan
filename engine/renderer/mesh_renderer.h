#pragma once

#include "rhi/rhi_command_list.h"
#include "scene/static_mesh_instance.h"
namespace Rodan {
using namespace Velos::RHI;
class MeshRenderer {
public:
  void Draw(ICommandList *cmd, const StaticMeshInstance &instance);
};
} // namespace Rodan
