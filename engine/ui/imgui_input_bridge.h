#pragma once

namespace Rodan {
class InputSystem;

class ImGuiInputBridge {
public:
  static void Apply(const Rodan::InputSystem &input);
};

} // namespace Rodan
