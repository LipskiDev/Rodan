#include "core/application.h"

#include "../../external/velos/velos/rhi/rhi_command_list.h"
#include "../../external/velos/velos/rhi/rhi_device.h"
#include "../../external/velos/velos/rhi/rhi_pipeline.h"
#include "../../external/velos/velos/rhi/rhi_types.h"
#include "GLFW/glfw3.h"
#include "core/fps_counter.h"
#include "core/input_system.h"
#include "core/types.h"
#include "core/window.h"
#include "graphics/bitmap.h"
#include "imgui.h"
#include "implot.h"
#include "platform/glfw/glfw_window.h"
#include "renderer/graph_renderer.h"
#include "renderer/line_renderer.h"
#include "scene/first_person_camera.h"
#include "ui/imgui_input_bridge.h"
#include "ui/imgui_renderer.h"

#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;
using std::make_unique;

namespace {
struct SkyboxPushConstants {
  glm::mat4 view;
  glm::mat4 proj;
};

std::vector<glm::vec3> CreateSkyboxVertices() {
  return {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {1, 1, -1},  {-1, 1, -1},
      {-1, -1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1},   {1, 1, 1},
      {-1, 1, 1},   {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1},
      {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {1, 1, 1},   {1, 1, -1},
      {1, -1, -1},  {1, -1, -1}, {1, -1, 1}, {1, 1, 1},   {-1, -1, -1},
      {1, -1, -1},  {1, -1, 1},  {1, -1, 1}, {-1, -1, 1}, {-1, -1, -1},
      {-1, 1, -1},  {1, 1, -1},  {1, 1, 1},  {1, 1, 1},   {-1, 1, 1},
      {-1, 1, -1},
  };
}
} // namespace

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
  CreateSkyboxResources();
  CreateSkyboxDescriptors();
  CreateSkyboxPipeline();
  InitializeImGui();
  InitializeDebugTools();

  timeStamp_ = glfwGetTime();
}

void Application::Shutdown() {
  if (!device_) {
    return;
  }

  device_->WaitIdle();

  if (imguiRenderer_) {
    imguiRenderer_->Shutdown();
  }

  ImPlot::DestroyContext();
  ImGui::DestroyContext();

  line3d_.reset();
  line2d_.reset();
  graphRenderer_.reset();
  camera_.reset();
  imguiRenderer_.reset();

  device_->DestroyPipeline(skybox_.pipeline);
  device_->DestroyShader(skybox_.fragmentShader);
  device_->DestroyShader(skybox_.vertexShader);

  device_->DestroyDescriptorPool(skybox_.descriptorPool);
  device_->DestroyDescriptorSetLayout(skybox_.setLayout);

  device_->DestroySampler(skybox_.sampler);
  device_->DestroyImageView(skybox_.imageView);
  device_->DestroyImage(skybox_.image);

  device_->DestroyBuffer(skybox_.stagingBuffer);
  device_->DestroyBuffer(skybox_.vertexBuffer);

  device_->DestroySwapchain(swapchain_);
  DestroyDevice(device_);
  device_ = nullptr;
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

  camera_ = make_unique<FirstPersonCamera>();
}

void Application::CreateSwapchain() {
  swapchain_ = device_->CreateSwapchain({
      .windowHandle = window_->GetNativeHandle(),
      .width = static_cast<u32>(window_->GetFramebufferWidth()),
      .height = static_cast<u32>(window_->GetFramebufferHeight()),
      .format = Format::BGRA8_UNORM,
      .bufferCount = 2,
      .vsync = false,
      .debugName = "Main Swapchain",
  });
}

void Application::CreateSkyboxResources() {
  const std::vector<glm::vec3> skyboxVertices = CreateSkyboxVertices();

  skybox_.vertexBuffer = device_->CreateBuffer({
      .size = static_cast<u64>(skyboxVertices.size() * sizeof(glm::vec3)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = skyboxVertices.data(),
      .debugName = "Skybox Vertex Buffer",
  });

  LoadSkyboxTextureData();

  skybox_.image = device_->CreateImage({
      .width = skybox_.faceSize,
      .height = skybox_.faceSize,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 6,
      .format = Format::RGBA32_FLOAT,
      .type = ImageType::Cube,
      .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
  });

  skybox_.imageView = device_->CreateImageView({
      .image = skybox_.image,
      .format = Format::RGBA32_FLOAT,
      .type = ImageViewType::Cube,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6,
      .debugName = "Skybox Cubemap View",
  });

  skybox_.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .debugName = "Skybox Cubemap Sampler",
  });
}

void Application::LoadSkyboxTextureData() {
  std::cout << "Loading HDR skybox texture\n";

  stbi_set_flip_vertically_on_load(1);

  int w = 0;
  int h = 0;
  float *img =
      stbi_loadf("assets/textures/piazza_bologni_1k.hdr", &w, &h, nullptr, 4);

  if (!img) {
    throw std::runtime_error("Failed to load HDR skybox texture");
  }

  Graphics::Bitmap in(w, h, 4, Graphics::BitmapFormat::Float, img);
  Graphics::Bitmap verticalCross = convertEquirectangularMapToVerticalCross(in);
  stbi_image_free(img);

  stbi_write_hdr(".cache/screenshot.hdr", verticalCross.w_, verticalCross.h_,
                 verticalCross.comp_,
                 reinterpret_cast<const float *>(verticalCross.data_.data()));

  Graphics::Bitmap cubemap = convertVerticalCrossToCubeMapFaces(verticalCross);

  skybox_.faceSize = static_cast<u32>(cubemap.w_);

  const u64 cubemapSize = static_cast<u64>(cubemap.w_) *
                          static_cast<u64>(cubemap.h_) * 6ull *
                          static_cast<u64>(cubemap.comp_) * sizeof(float);

  skybox_.stagingBuffer = device_->CreateBuffer({
      .size = cubemapSize,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = cubemap.data_.data(),
      .debugName = "Skybox Cubemap Upload Buffer",
  });
}

void Application::CreateSkyboxDescriptors() {
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  skybox_.setLayout = device_->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "Skybox Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  skybox_.descriptorPool = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "Skybox Descriptor Pool",
  });

  skybox_.descriptorSet = device_->AllocateDescriptorSet(
      skybox_.descriptorPool, skybox_.setLayout, "Skybox Descriptor Set");

  DescriptorImageInfo imageInfo{};
  imageInfo.sampler = skybox_.sampler;
  imageInfo.imageView = skybox_.imageView;
  imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = skybox_.descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });
}

void Application::CreateSkyboxPipeline() {
  auto vertSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/skybox.vert",
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/skybox.frag",
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  skybox_.vertexShader = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Skybox Vertex Shader",
  });

  skybox_.fragmentShader = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Skybox Fragment Shader",
  });

  VertexBufferLayoutDesc layout{
      .stride = sizeof(glm::vec3),
      .inputRate = VertexInputRate::PerVertex,
      .attributes = {{
          .location = 0,
          .format = VertexFormat::Float32x3,
          .offset = 0,
      }},
  };

  DescriptorSetLayoutHandle setLayouts[] = {skybox_.setLayout};

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = skybox_.vertexShader;
  pipelineDesc.fragmentShader = skybox_.fragmentShader;
  pipelineDesc.vertexLayouts.push_back(layout);
  pipelineDesc.layout.descriptorSetLayouts = setLayouts;
  pipelineDesc.layout.descriptorSetLayoutCount = 1;
  pipelineDesc.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.raster.cullBackFaces = false;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = false;
  pipelineDesc.blend = {
      .enable = true,
      .srcColor = BlendFactor::SrcAlpha,
      .dstColor = BlendFactor::OneMinusSrcAlpha,
      .colorOp = BlendOp::Add,
      .srcAlpha = BlendFactor::One,
      .dstAlpha = BlendFactor::OneMinusSrcAlpha,
      .alphaOp = BlendOp::Add,
  };
  pipelineDesc.colorFormat = Format::BGRA8_UNORM;
  pipelineDesc.debugName = "Skybox Pipeline";

  skybox_.pipeline = device_->CreateGraphicsPipeline(pipelineDesc);
}

void Application::InitializeImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  imguiRenderer_ = make_unique<ImGuiRenderer>();
  imguiRenderer_->Initialize(device_, swapchain_, Format::BGRA8_UNORM,
                             Format::Undefined);
}

void Application::InitializeDebugTools() {
  line3d_ = make_unique<Debug::LineRenderer3D>(device_);
  line2d_ = make_unique<Debug::LineRenderer2D>();
  graphRenderer_ = make_unique<Debug::GraphRenderer>("fps graph", 2048);
}

void Application::MainLoop() {
  using Clock = std::chrono::high_resolution_clock;
  auto lastTime = Clock::now();

  while (!window_->ShouldClose()) {
    const double newTimeStamp = glfwGetTime();
    deltaSeconds_ = static_cast<float>(newTimeStamp - timeStamp_);
    timeStamp_ = newTimeStamp;

    auto now = Clock::now();
    const float deltaTime =
        std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;

    time_ += deltaTime;
    fpsCounter_.tick(deltaSeconds_);
    currentFps_ = fpsCounter_.getFPS();

    input_->BeginFrame();
    window_->PollEvents();

    if (HandleResize()) {
      continue;
    }

    ProcessInputEvents();
    Update(deltaSeconds_);

    BeginImGuiFrame(deltaTime);
    BuildImGui();
    ImGui::Render();

    RenderFrame();

    line3d_->clear();
    graphRenderer_->addPoint(fpsCounter_.getFPS());
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

  imguiRenderer_->Shutdown();
  imguiRenderer_->Initialize(device_, swapchain_, Format::BGRA8_UNORM,
                             Format::Undefined);

  return true;
}

void Application::ProcessInputEvents() {
  for (const InputEvent &event : input_->GetEvents()) {
    if (event.type == InputEventType::KeyDown ||
        event.type == InputEventType::KeyUp) {
      camera_->OnKeyboard(event);
    } else if (event.type == InputEventType::MouseMove) {
      float dx = 0.0f;
      float dy = 0.0f;

      if (firstMouse_) {
        lastMouseX_ = event.mouseMove.x;
        lastMouseY_ = event.mouseMove.y;
        firstMouse_ = false;
      } else {
        dx = event.mouseMove.x - lastMouseX_;
        dy = event.mouseMove.y - lastMouseY_;
        lastMouseX_ = event.mouseMove.x;
        lastMouseY_ = event.mouseMove.y;
      }

      if (input_->IsMouseDown(MouseButton::Right))
        camera_->OnMouseMove(dx, dy);
    }
  }
}

void Application::Update(float deltaSeconds) {
  const Extent2D dims = device_->GetSwapchainDimensions();
  if (dims.width == 0 || dims.height == 0) {
    return;
  }

  camera_->SetPerspective(60.0f, static_cast<float>(dims.width) / dims.height,
                          0.1f, 100.0f);
  camera_->Update(deltaSeconds);

  line2d_->clear();
  line2d_->line({100, 300}, {100, 400}, glm::vec4(1, 0, 0, 1));
  line2d_->line({100, 400}, {200, 400}, glm::vec4(0, 1, 0, 1));
  line2d_->line({200, 400}, {200, 300}, glm::vec4(0, 0, 1, 1));
  line2d_->line({200, 300}, {100, 300}, glm::vec4(1, 1, 0, 1));

  line3d_->plane(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                 glm::vec3(0.0f, 0.0f, 1.0f), 10, 10, 10.0f, 10.0f,
                 glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
                 glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));

  line3d_->box(glm::mat4(1.0f), glm::vec3(1.0f), glm::vec4(1, 0, 0, 1));
}

void Application::BeginImGuiFrame(float deltaTime) {
  const Extent2D dims = device_->GetSwapchainDimensions();
  ImGuiInputBridge::Apply(*input_);
  imguiRenderer_->NewFrame(deltaTime, window_->GetWidth(), window_->GetHeight(),
                           dims.width, dims.height);
}

void Application::BuildImGui() {
  const Extent2D dims = device_->GetSwapchainDimensions();

  if (const ImGuiViewport *v = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowPos(
        {v->WorkPos.x + v->WorkSize.x - 15.0f, v->WorkPos.y + 15.0f},
        ImGuiCond_Always, {1.0f, 0.0f});
  }

  ImGui::SetNextWindowBgAlpha(0.30f);
  ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));

  if (ImGui::Begin("##FPS", nullptr,
                   ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoFocusOnAppearing |
                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
    ImGui::Text("FPS : %.0f", fpsCounter_.getFPS());
    ImGui::Text("Ms  : %.2f",
                currentFps_ > 0.0f ? 1000.0f / currentFps_ : 0.0f);
  }
  ImGui::End();

  ImGui::Begin("Velos");
  ImGui::Text("Custom Dear ImGui renderer on top of Velos");
  ImGui::Text("Backend: Vulkan");
  ImGui::Separator();
  ImGui::Text("Window: %d x %d", window_->GetWidth(), window_->GetHeight());
  ImGui::Text("Framebuffer: %u x %u", dims.width, dims.height);
  ImGui::Text("Delta Time: %.4f", deltaSeconds_);
  ImGui::Text("Mouse: %.1f, %.1f", input_->GetMouseX(), input_->GetMouseY());

  line2d_->render("quad");

  ImGui::End();

  graphRenderer_->renderGraph(0, dims.height * 0.8f, dims.width,
                              dims.height * 0.2f);

  if (showDemoWindow_) {
    ImGui::ShowDemoWindow(&showDemoWindow_);
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

  UploadSkyboxIfNeeded(cmd);

  cmd.Barrier({
      .image = frame.backbufferImage,
      .newLayout = ImageLayout::ColorAttachment,
      .aspect = ImageAspect::Color,
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

  Rect2D renderArea{};
  renderArea.offset = {0, 0};
  renderArea.extent = {dims.width, dims.height};

  RenderingInfo renderingInfo{};
  renderingInfo.renderArea = renderArea;
  renderingInfo.colorAttachments = &colorAttachment;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.depthAttachment = nullptr;

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

  RenderSkybox(cmd);
  RenderDebug(cmd);
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

void Application::RenderSkybox(ICommandList &cmd) {
  const std::vector<glm::vec3> skyboxVertices = CreateSkyboxVertices();

  SkyboxPushConstants push{};
  push.view = camera_->GetView();
  push.proj = camera_->GetProjection();

  cmd.BindPipeline(skybox_.pipeline);
  cmd.BindDescriptorSet(skybox_.pipeline, 0, skybox_.descriptorSet);
  cmd.BindVertexBuffer(0, skybox_.vertexBuffer, 0);
  cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(SkyboxPushConstants), &push);
  cmd.Draw(static_cast<u32>(skyboxVertices.size()));
}

void Application::RenderDebug(ICommandList &cmd) {
  line3d_->render(cmd, camera_->GetProjection() * camera_->GetView());
}

void Application::RenderImGui(ICommandList &cmd) {
  imguiRenderer_->Render(cmd, ImGui::GetDrawData());
}

void Application::UploadSkyboxIfNeeded(ICommandList &cmd) {
  if (skybox_.uploaded) {
    return;
  }

  std::cout << "Uploading skybox cubemap\n";

  const u32 bytesPerPixel = 4 * sizeof(float);
  const u32 facePixelCount = skybox_.faceSize * skybox_.faceSize;
  const u64 faceByteSize = static_cast<u64>(facePixelCount) * bytesPerPixel;

  cmd.Barrier({
      .image = skybox_.image,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
  });

  for (u32 face = 0; face < 6; ++face) {
    BufferImageCopyRegion region{};
    region.bufferOffset = face * faceByteSize;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.mipLevel = 0;
    region.baseArrayLayer = face;
    region.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {skybox_.faceSize, skybox_.faceSize, 1};
    region.aspect = ImageAspect::Color;

    cmd.CopyBufferToImage(skybox_.stagingBuffer, skybox_.image, region);
  }

  cmd.Barrier({
      .image = skybox_.image,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
  });

  skybox_.uploaded = true;
}

} // namespace Rodan
