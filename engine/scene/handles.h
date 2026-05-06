#pragma once

#include <cstdint>
#include <graphics/material_resource.h>

namespace Rodan {

struct MeshTag {};
struct MaterialTag {};
struct RenderObjectTag {};

using MeshHandle = Handle<MeshTag>;
using MaterialHandle = Handle<MaterialTag>;
using RenderObjectHandle = Handle<RenderObjectTag>;

using DirectionalLightHandle = uint32_t;
} // namespace Rodan
