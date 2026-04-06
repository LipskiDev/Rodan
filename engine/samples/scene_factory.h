#include "samples/scenes/duck_scene.h"
#include "samples/scenes/million_cubes.h"
#include <memory>
#include <samples/scene.h>

namespace Rodan {

inline std::unique_ptr<IScene> CreateSceneByType(SceneType type) {
  switch (type) {
  case SceneType::Duck:
    return std::make_unique<DuckScene>();
  case SceneType::MillionCubes:
    return std::make_unique<MillionCubesScene>();
  default:
    return nullptr;
  }
}
} // namespace Rodan
