#include "core/application.h"

#include "GLFW/glfw3.h"
#include "core/input_system.h"
#include "core/window.h"
#include "imgui.h"
#include "implot.h"
#include "platform/glfw/glfw_window.h"
#include "samples/scene_factory.h"
#include <ui/imgui_input_bridge.h>

#include <iostream>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;
using std::make_unique;

Application::Application() { std::cout << "[Rodan] Application created\n"; }

Application::~Application() { std::cout << "[Rodan] Application destroyed\n"; }

void Application::Run() {
  std::cout << "[Rodan] Application run\n";

  Initialize();
  MainLoop();
  Shutdown();
}

void Application::Initialize() {
  InitializeWindowAndDevice();
  CreateSwapchain();
  CreateDepthResources();
  InitializeImGui();
  CreateScene(currentSceneType_);

  timeStamp_ = glfwGetTime();
}

void Application::Shutdown() {
  if (!device_) {
    return;
  }

  device_->WaitIdle();

  if (currentScene_) {
    currentScene_->Shutdown(device_);
    currentScene_.reset();
  }

  DestroyDepthResources();

  if (swapchain_.IsValid()) {
    device_->DestroySwapchain(swapchain_);
    swapchain_ = {};
  }

  if (imguiRenderer_) {
    imguiRenderer_->Shutdown();
    imguiRenderer_.reset();
  }

  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  DestroyDevice(device_);
  device_ = nullptr;

  input_.reset();
  window_.reset();
}

void Application::InitializeWindowAndDevice() {
  input_ = make_unique<InputSystem>();

  window_ = make_unique<GlfwWindow>(windowWidth_, windowHeight_,
                                    "Rodan Renderer", true, input_.get());

  device_ = Velos::RHI::CreateDevice({
      .backend = BackendAPI::Vulkan,
      .enableValidation = true,
      .applicationName = "Rodan Renderer",
  });
}

void Application::CreateSwapchain() {
  swapchain_ = device_->CreateSwapchain({
      .windowHandle = window_->GetNativeHandle(),
      .width = static_cast<u32>(window_->GetFramebufferWidth()),
      .height = static_cast<u32>(window_->GetFramebufferHeight()),
      .format = colorFormat_,
      .bufferCount = 2,
      .vsync = false,
      .debugName = "Main Swapchain",
  });
}

void Application::CreateDepthResources() {
  const Extent2D dims = device_->GetSwapchainDimensions();
  if (dims.width == 0 || dims.height == 0) {
    return;
  }

  depthImage_ = device_->CreateImage({
      .width = dims.width,
      .height = dims.height,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = depthFormat_,
      .usage = ImageUsage::DepthStencil,
      .debugName = "Main Depth Image",
  });

  depthImageView_ = device_->CreateImageView({
      .image = depthImage_,
      .format = depthFormat_,
      .aspect = ImageAspect::Depth,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = "Main Depth View",
  });
}

void Application::InitializeImGui() {
  IMGUI_CHECKVERSION();

  // Create core contexts
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO &io = ImGui::GetIO();

  // Enable useful features
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Optional but recommended:
  // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  // Style
  ImGui::StyleColorsDark();

  // If you enable viewports, fix style:
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Create renderer backend (your custom Velos ImGui renderer)
  imguiRenderer_ = std::make_unique<ImGuiRenderer>();

  imguiRenderer_->Initialize(device_, swapchain_, colorFormat_, depthFormat_);
}

void Application::DestroyDepthResources() {
  if (!device_) {
    return;
  }

  if (depthImageView_.IsValid()) {
    device_->DestroyImageView(depthImageView_);
    depthImageView_ = {};
  }

  if (depthImage_.IsValid()) {
    device_->DestroyImage(depthImage_);
    depthImage_ = {};
  }
}

void Application::MainLoop() {
  while (!window_->ShouldClose()) {
    const double now = glfwGetTime();
    deltaSeconds_ = static_cast<float>(now - timeStamp_);
    timeStamp_ = now;

    input_->BeginFrame();
    window_->PollEvents();

    if (HandleResize()) {
      continue;
    }

    Update(deltaSeconds_);

    BeginImGuiFrame(deltaSeconds_);

    BuildApplicationImGui();

    if (currentScene_)
      currentScene_->RenderImGui();

    ImGui::Render();
    RenderFrame();
  }
}

bool Application::HandleResize() {
  if (!window_->WasFramebufferResized()) {
    return false;
  }

  window_->ResetFramebufferResizedFlag();

  const u32 fbWidth = static_cast<u32>(window_->GetFramebufferWidth());
  const u32 fbHeight = static_cast<u32>(window_->GetFramebufferHeight());

  if (fbWidth == 0 || fbHeight == 0) {
    return true;
  }

  device_->WaitIdle();
  device_->ResizeSwapchain(swapchain_, fbWidth, fbHeight);

  DestroyDepthResources();
  CreateDepthResources();

  if (currentScene_) {
    currentScene_->OnResize(device_, fbWidth, fbHeight);
  }

  return true;
}

void Application::Update(float deltaSeconds) {
  const Extent2D dims = device_->GetSwapchainDimensions();
  if (dims.width == 0 || dims.height == 0) {
    return;
  }

  if (currentScene_) {
    SceneUpdateContext ctx{};
    ctx.input = input_.get();
    ctx.framebufferWidth = dims.width;
    ctx.framebufferHeight = dims.height;

    currentScene_->Update(deltaSeconds, ctx);
  }
}

void Application::RenderFrame() {
  const Extent2D dims = device_->GetSwapchainDimensions();
  if (dims.width == 0 || dims.height == 0) {
    return;
  }

  FrameBeginResult frame = device_->BeginFrame(swapchain_);
  if (!frame.success) {
    return;
  }

  ICommandList &cmd = device_->GetCommandList(frame.commandList);
  cmd.Begin();

  if (currentScene_) {
    currentScene_->Prepare(cmd);
  }

  cmd.Barrier({
      .image = frame.backbufferImage,
      .newLayout = ImageLayout::ColorAttachment,
      .aspect = ImageAspect::Color,
  });

  cmd.Barrier({
      .image = depthImage_,
      .newLayout = ImageLayout::DepthAttachment,
      .aspect = ImageAspect::Depth,
  });

  ColorAttachmentDesc colorAttachment{};
  colorAttachment.view = frame.backbuffer;
  colorAttachment.loadOp = LoadOp::Clear;
  colorAttachment.storeOp = StoreOp::Store;
  colorAttachment.clearValue = {
      .r = 0.1f,
      .g = 0.1f,
      .b = 0.1f,
      .a = 1.0f,
  };

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = depthImageView_;
  depthAttachment.loadOp = LoadOp::Clear;
  depthAttachment.storeOp = StoreOp::Store;
  depthAttachment.clearDepth = 1.0f;
  depthAttachment.clearStencil = 0;

  Rect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent = {dims.width, dims.height};

  RenderingInfo renderingInfo{};
  renderingInfo.renderArea = renderArea;
  renderingInfo.colorAttachments = &colorAttachment;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.depthAttachment = &depthAttachment;

  cmd.BeginRendering(renderingInfo);

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(dims.width),
      .height = static_cast<float>(dims.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = {dims.width, dims.height},
  });

  if (currentScene_) {
    currentScene_->Render(cmd);
  }

  RenderImGui(cmd);

  cmd.EndRendering();

  cmd.Barrier({
      .image = frame.backbufferImage,
      .newLayout = ImageLayout::Present,
      .aspect = ImageAspect::Color,
  });

  cmd.End();
  device_->SubmitAndPresent(frame.commandList, swapchain_);
}

void Application::BeginImGuiFrame(float deltaTime) {
  const Extent2D dims = device_->GetSwapchainDimensions();
  ImGuiInputBridge::Apply(*input_);
  imguiRenderer_->NewFrame(deltaTime, window_->GetWidth(), window_->GetHeight(),
                           dims.width, dims.height);
}

void Application::BuildApplicationImGui() {
  const Extent2D dims = device_->GetSwapchainDimensions();
  const float fps = deltaSeconds_ > 0.0f ? 1.0f / deltaSeconds_ : 0.0f;
  const float ms = deltaSeconds_ * 1000.0f;

  ImGui::Begin("Runtime");
  ImGui::Text("Backend: Vulkan");
  ImGui::Text("Framebuffer: %u x %u", dims.width, dims.height);
  ImGui::Text("FPS: %.1f", fps);

  const char *sceneNames[] = {"Duck", "Million Cubes", "Sponza",
                              "MetalRoughnessSpheresNoTex"};

  int currentSceneIndex = static_cast<int>(currentSceneType_);
  if (ImGui::Combo("Scene", &currentSceneIndex, sceneNames,
                   IM_ARRAYSIZE(sceneNames))) {
    SwitchScene(static_cast<SceneType>(currentSceneIndex));
  }

  ImGui::End();

  {
    const float padding = 10.0f;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;   // top-left
    ImVec2 workSize = viewport->WorkSize; // size

    ImVec2 windowPos =
        ImVec2(workPos.x + workSize.x - padding, workPos.y + padding);

    ImVec2 windowPivot = ImVec2(1.0f, 0.0f); // top-right anchor

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(0.35f); // transparent background

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::Begin("##PerformanceOverlay", nullptr, flags);

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.2f ms", ms);

    ImGui::End();
  }

  if (showDemoWindow_) {
    ImGui::ShowDemoWindow(&showDemoWindow_);
  }
}

void Application::RenderImGui(Velos::RHI::ICommandList &cmd) {
  imguiRenderer_->Render(cmd, ImGui::GetDrawData());
}

void Application::CreateScene(SceneType type) {
  currentScene_ = CreateSceneByType(type);
  if (currentScene_) {
    currentScene_->Initialize(device_, swapchain_, colorFormat_, depthFormat_);
  }
}

void Application::SwitchScene(SceneType type) {
  if (type == currentSceneType_) {
    return;
  }

  device_->WaitIdle();

  if (currentScene_) {
    currentScene_->Shutdown(device_);
    currentScene_.reset();
  }

  currentSceneType_ = type;
  CreateScene(currentSceneType_);
}

} // namespace Rodan
