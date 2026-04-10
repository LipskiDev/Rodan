#include "samples/scenes/duck_scene.h"
#include "samples/scenes/metal_roughness_spheres_no_tex_scene.h"
#include "samples/scenes/million_cubes.h"
#include "samples/scenes/sponza_scene.h"
#include <memory>
#include <samples/scene.h>

namespace Rodan {

inline std::unique_ptr<IScene> CreateSceneByType(SceneType type) {
  switch (type) {
  case SceneType::Duck:
    return std::make_unique<DuckScene>();
  case SceneType::MillionCubes:
    return std::make_unique<MillionCubesScene>();
  case SceneType::Sponza:
    return std::make_unique<SponzaScene>();
  case SceneType::MetalRoughnessSpheresNoTex:
    return std::make_unique<MetalRoughnessSpheresNoTexScene>();
  default:
    return nullptr;
  }
}
} // namespace Rodan
