#include "samples/scenes/compute_test_scene.h"
#include "samples/scenes/duck_scene.h"
#include "samples/scenes/gltf_viewer_scene.h"
#include "samples/scenes/million_cubes.h"
#include <memory>
#include <samples/scene.h>

namespace Rodan {

inline std::unique_ptr<IScene> CreateSceneByType(SceneType type) {
  switch (type) {
  case SceneType::GltfViewer:
    return std::make_unique<GltfViewerScene>();
  case SceneType::ComputeTest:
    return std::make_unique<ComputeTestScene>();
  default:
    return nullptr;
  }
}
} // namespace Rodan
