#pragma once

#include "assets/imported_scene.h"
#include <string>
namespace Rodan {
class GltfLoader {
public:
  static ImportedScene Load(const std::string &path);
};
} // namespace Rodan
