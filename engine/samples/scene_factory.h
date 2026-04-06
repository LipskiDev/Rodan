#include "samples/scenes/duck_scene.h"
#include <memory>
#include <samples/scene.h>

namespace Rodan {

inline std::unique_ptr<IScene> CreateSceneByType(SceneType type) {
  switch (type) {
  case SceneType::Duck:
    return std::make_unique<DuckScene>();
  default:
    return nullptr;
  }
}
} // namespace Rodan
