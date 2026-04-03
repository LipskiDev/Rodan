#pragma once

namespace Rodan {

class Application {
public:
  Application();
  ~Application();

  void Run();

  bool firstMouse = true;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
};

} // namespace Rodan
