#pragma once

#include "../../external/velos/velos/rhi/rhi_device.h"
#include "../../external/velos/velos/rhi/rhi_pipeline.h"
#include "../../external/velos/velos/rhi/rhi_resources.h"
#include "core/fps_counter.h"
#include "core/types.h"

#include <memory>

namespace Rodan {

class InputSystem;
class GlfwWindow;
class FirstPersonCamera;

namespace Debug {
class LineRenderer3D;
class LineRenderer2D;
class GraphRenderer;
} // namespace Debug

class ImGuiRenderer;

class Application {
public:
  Application();
  ~Application();

  void Run();

private:
  struct SkyboxResources {
    Velos::RHI::BufferHandle vertexBuffer{};
    Velos::RHI::BufferHandle stagingBuffer{};
    Velos::RHI::ImageHandle image{};
    Velos::RHI::ImageViewHandle imageView{};
    Velos::RHI::SamplerHandle sampler{};

    Velos::RHI::DescriptorSetLayoutHandle setLayout{};
    Velos::RHI::DescriptorPoolHandle descriptorPool{};
    Velos::RHI::DescriptorSetHandle descriptorSet{};

    Velos::RHI::ShaderHandle vertexShader{};
    Velos::RHI::ShaderHandle fragmentShader{};
    Velos::RHI::PipelineHandle pipeline{};

    Velos::u32 faceSize = 0;
    bool uploaded = false;
  };

private:
  void Initialize();
  void Shutdown();

  void InitializeWindowAndDevice();
  void InitializeImGui();
  void InitializeDebugTools();

  void CreateSwapchain();
  void CreateSkyboxResources();
  void CreateSkyboxDescriptors();
  void CreateSkyboxPipeline();

  void MainLoop();
  void BeginFrameTiming();
  bool HandleResize();
  void ProcessInputEvents();
  void Update(float deltaSeconds);
  void RenderFrame();

  void BeginImGuiFrame(float deltaTime);
  void BuildImGui();
  void RenderScene(Velos::RHI::ICommandList &cmd,
                   const Velos::RHI::Extent2D &dims,
                   const Velos::RHI::FrameBeginResult &frame);
  void RenderSkybox(Velos::RHI::ICommandList &cmd);
  void RenderDebug(Velos::RHI::ICommandList &cmd);
  void RenderImGui(Velos::RHI::ICommandList &cmd);

  void UploadSkyboxIfNeeded(Velos::RHI::ICommandList &cmd);
  void LoadSkyboxTextureData();

private:
  Velos::u32 windowWidth_ = 1280;
  Velos::u32 windowHeight_ = 720;

  std::unique_ptr<InputSystem> input_;
  std::unique_ptr<GlfwWindow> window_;

  Velos::RHI::IDevice *device_ = nullptr;
  Velos::RHI::SwapchainHandle swapchain_{};

  std::unique_ptr<ImGuiRenderer> imguiRenderer_;

  std::unique_ptr<FirstPersonCamera> camera_;
  std::unique_ptr<Debug::LineRenderer3D> line3d_;
  std::unique_ptr<Debug::LineRenderer2D> line2d_;
  std::unique_ptr<Debug::GraphRenderer> graphRenderer_;

  FramePerSecondCounter fpsCounter_{0.005f};
  float currentFps_ = 0.0f;

  SkyboxResources skybox_;

  bool showDemoWindow_ = true;
  bool firstMouse_ = true;
  float lastMouseX_ = 0.0f;
  float lastMouseY_ = 0.0f;

  float deltaSeconds_ = 0.0f;
  float time_ = 0.0f;
  double timeStamp_ = 0.0;
};

} // namespace Rodan
