#pragma once

#include "rhi/rhi_device.h"
#include "rhi/rhi_types.h"
#include "samples/scene.h"
#include "ui/imgui_renderer.h"

#include <memory>

namespace Rodan {

class InputSystem;
class GlfwWindow;

class Application {
public:
  Application();
  ~Application();

  void Run();

private:
  void Initialize();
  void Shutdown();
  void MainLoop();

  void InitializeWindowAndDevice();
  void CreateSwapchain();
  void CreateDepthResources();
  void DestroyDepthResources();

  bool HandleResize();
  void Update(float deltaSeconds);
  void RenderFrame();

  void InitializeImGui();
  void BeginImGuiFrame(float deltaTime);
  void BuildApplicationImGui();
  void RenderImGui(Velos::RHI::ICommandList &cmd);

  void CreateScene(SceneType type);
  void SwitchScene(SceneType type);

private:
  std::unique_ptr<GlfwWindow> window_;
  std::unique_ptr<InputSystem> input_;

  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};
  Velos::RHI::ImageHandle depthImage_{};
  Velos::RHI::ImageViewHandle depthImageView_{};

  std::unique_ptr<IScene> currentScene_;
  SceneType currentSceneType_ = SceneType::Duck;

  Velos::RHI::Format colorFormat_ = Velos::RHI::Format::BGRA8_UNORM;
  Velos::RHI::Format depthFormat_ = Velos::RHI::Format::D32_FLOAT;

  int windowWidth_ = 1600;
  int windowHeight_ = 900;

  double timeStamp_ = 0.0;
  float deltaSeconds_ = 0.0f;

  std::unique_ptr<ImGuiRenderer> imguiRenderer_;
  bool showDemoWindow_ = false;
};

} // namespace Rodan
