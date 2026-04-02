#include "core/application.h"

#include "../../external/velos/velos/rhi/rhi_command_list.h"
#include "../../external/velos/velos/rhi/rhi_device.h"
#include "../../external/velos/velos/rhi/rhi_pipeline.h"
#include "../../external/velos/velos/rhi/rhi_types.h"
#include "GLFW/glfw3.h"
#include "core/input_system.h"
#include "core/types.h"
#include "core/window.h"
#include "graphics/bitmap.h"
#include "imgui.h"
#include "platform/glfw/glfw_window.h"
#include "ui/imgui_input_bridge.h"
#include "ui/imgui_renderer.h"

#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include <core/fps_counter.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Rodan {
using std::make_unique;

using namespace Velos;
using namespace Velos::RHI;

struct Vertex {
  glm::vec3 pos;
  glm::vec2 uv;
};

Application::Application() { std::cout << "[Rodan] Application created\n"; }

Application::~Application() { std::cout << "[Rodan] Application destroyed\n"; }

void Application::Run() {
  std::cout << "[Rodan] Application run\n";

  Velos::u32 windowHeight = 720;
  Velos::u32 windowWidth = 1280;

  InputSystem input;

  std::unique_ptr<GlfwWindow> window = make_unique<GlfwWindow>(
      windowWidth, windowHeight, "Rodan Renderer", true, &input);

  IDevice *device = Velos::RHI::CreateDevice({
      .backend = BackendAPI::Vulkan,
      .enableValidation = true,
      .applicationName = "Rodan ImGui Demo",
  });

  SwapchainHandle swapchain = device->CreateSwapchain({
      .windowHandle = window->GetNativeHandle(),
      .width = static_cast<Velos::u32>(window->GetFramebufferWidth()),
      .height = static_cast<Velos::u32>(window->GetFramebufferHeight()),
      .format = Format::BGRA8_UNORM,
      .bufferCount = 2,
      .debugName = "Main Swapchain",
  });

  std::vector<glm::vec3> skyboxVertices = {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {1, 1, -1},  {-1, 1, -1},
      {-1, -1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1},   {1, 1, 1},
      {-1, 1, 1},   {-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1},
      {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1}, {1, 1, 1},   {1, 1, -1},
      {1, -1, -1},  {1, -1, -1}, {1, -1, 1}, {1, 1, 1},   {-1, -1, -1},
      {1, -1, -1},  {1, -1, 1},  {1, -1, 1}, {-1, -1, 1}, {-1, -1, -1},
      {-1, 1, -1},  {1, 1, -1},  {1, 1, 1},  {1, 1, 1},   {-1, 1, 1},
      {-1, 1, -1}};

  std::cout << "Creating vertex buffer\n";
  BufferHandle vertexBuffer = device->CreateBuffer({
      .size =
          static_cast<Velos::u64>(skyboxVertices.size() * sizeof(glm::vec3)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = skyboxVertices.data(),
      .debugName = "Quad Vertex Buffer",
  });

  std::cout << "Loading transparent texture with stb_image\n";
  stbi_set_flip_vertically_on_load(1);

  int w, h;
  const float *img =
      stbi_loadf("assets/textures/piazza_bologni_1k.hdr", &w, &h, nullptr, 4);

  if (!img) {
    throw std::runtime_error("Failed to load awesomeface.png with stb_image");
  }

  Graphics::Bitmap in(w, h, 4, Graphics::BitmapFormat::Float, img);
  Graphics::Bitmap out = convertEquirectangularMapToVerticalCross(in);
  stbi_image_free((void *)img);

  stbi_write_hdr(".cache/screenshot.hdr", out.w_, out.h_, out.comp_,
                 (const float *)out.data_.data());

  Graphics::Bitmap cubemap = convertVerticalCrossToCubeMapFaces(out);

  const Velos::u64 cubemapSize = static_cast<Velos::u64>(cubemap.w_) *
                                 static_cast<Velos::u64>(cubemap.h_) * 6 *
                                 static_cast<Velos::u64>(cubemap.comp_) *
                                 sizeof(float);

  std::cout << "Creating staging buffer\n";

  BufferHandle cubemapStagingBuffer = device->CreateBuffer({
      .size = cubemapSize,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = cubemap.data_.data(),
      .debugName = "Cubemap Upload Staging Buffer",
  });

  std::cout << "Creating texture image\n";
  u32 faceSize = cubemap.w_;
  ImageHandle cubemapImage = device->CreateImage(
      {.width = static_cast<Velos::u32>(faceSize),
       .height = static_cast<Velos::u32>(faceSize),
       .depth = 1,
       .mipLevels = 1,
       .arrayLayers = 6,
       .format = Format::RGBA32_FLOAT,
       .type = ImageType::Cube,
       .usage = ImageUsage::TransferDst | ImageUsage::Sampled});

  ImageViewHandle cubemapImageView =
      device->CreateImageView({.image = cubemapImage,
                               .format = Format::RGBA32_FLOAT,
                               .type = ImageViewType::Cube,
                               .aspect = ImageAspect::Color,
                               .baseMipLevel = 0,
                               .mipLevelCount = 1,
                               .baseArrayLayer = 0,
                               .arrayLayerCount = 6,
                               .debugName = "Cubemap Image View"});

  SamplerHandle sampler = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .debugName = "Cubemap Sampler",
  });

  std::cout << "Creating descriptor set layout\n";
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  DescriptorSetLayoutHandle setLayout = device->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "Blending Test Set Layout",
  });

  std::cout << "Creating descriptor pool\n";
  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  DescriptorPoolHandle descriptorPool = device->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "Blending Test Descriptor Pool",
  });

  std::cout << "Allocating descriptor set\n";
  DescriptorSetHandle descriptorSet = device->AllocateDescriptorSet(
      descriptorPool, setLayout, "Blending Test Set");

  std::cout << "Updating descriptor set\n";
  DescriptorImageInfo imageInfo{};
  imageInfo.sampler = sampler;
  imageInfo.imageView = cubemapImageView;
  imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device->UpdateDescriptorSet({
      .dstSet = descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });

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

  ShaderHandle vertexShader = device->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Blending Test Vertex Shader",
  });

  ShaderHandle fragmentShader = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Blending Test Fragment Shader",
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

  DescriptorSetLayoutHandle setLayouts[] = {setLayout};

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
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
  pipelineDesc.debugName = "Blending Test Pipeline";

  std::cout << "Creating pipeline\n";
  PipelineHandle pipeline = device->CreateGraphicsPipeline(pipelineDesc);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGuiRenderer imguiRenderer;
  imguiRenderer.Initialize(device, swapchain, Format::BGRA8_UNORM,
                           Format::Undefined);

  using Clock = std::chrono::high_resolution_clock;
  auto lastTime = Clock::now();

  bool showDemoWindow = true;

  bool uploaded = false;
  float time = 0.0f;
  float deltaSeconds = 0.0f;
  double timeStamp = glfwGetTime();

  FramePerSecondCounter fpsCounter(0.5f);

  while (!window->ShouldClose()) {
    fpsCounter.tick(deltaSeconds);

    const double newTimeStamp = glfwGetTime();
    deltaSeconds = static_cast<float>(newTimeStamp - timeStamp);
    timeStamp = newTimeStamp;

    input.BeginFrame();
    window->PollEvents();

    if (window->WasFramebufferResized()) {
      window->ResetFramebufferResizedFlag();

      const Velos::u32 fbWidth =
          static_cast<Velos::u32>(window->GetFramebufferWidth());
      const Velos::u32 fbHeight =
          static_cast<Velos::u32>(window->GetFramebufferHeight());

      if (fbWidth > 0 && fbHeight > 0) {
        device->WaitIdle();
        device->ResizeSwapchain(swapchain, fbWidth, fbHeight);

        imguiRenderer.Shutdown();
        imguiRenderer.Initialize(device, swapchain, Format::BGRA8_UNORM,
                                 Format::Undefined);
      }

      continue;
    }

    Extent2D dims = device->GetSwapchainDimensions();
    if (dims.width == 0 || dims.height == 0) {
      continue;
    }

    auto now = Clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;

    time += deltaTime;

    ImGuiInputBridge::Apply(input);
    imguiRenderer.NewFrame(deltaTime, window->GetWidth(), window->GetHeight(),
                           dims.width, dims.height);

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
      ImGui::Text("FPS : %i", (int)fpsCounter.getFPS());
      ImGui::Text("Ms  : %.1f", 1000.0 / fpsCounter.getFPS());
    }

    ImGui::End();

    ImGui::Begin("Velos");
    ImGui::Text("Custom Dear ImGui renderer on top of Velos");
    ImGui::Text("Backend: Vulkan");
    ImGui::Separator();
    ImGui::Text("Window: %d x %d", window->GetWidth(), window->GetHeight());
    ImGui::Text("Framebuffer: %u x %u", dims.width, dims.height);
    ImGui::Text("Delta Time: %.4f", deltaTime);
    ImGui::Text("Mouse: %.1f, %.1f", input.GetMouseX(), input.GetMouseY());
    ImGui::End();

    ImGui::Render();

    FrameBeginResult frame = device->BeginFrame(swapchain);
    if (!frame.success) {
      continue;
    }

    ICommandList &cmd = device->GetCommandList(frame.commandList);

    cmd.Begin();

    if (!uploaded) {

      std::cout << "Recording texture upload commands\n";

      const u32 bytesPerPixel = cubemap.comp_ * sizeof(float);
      const u32 faceSize = cubemap.w_; // since width = faceWidth
      const u32 facePixelCount = faceSize * faceSize;
      const u64 faceByteSize = (u64)facePixelCount * bytesPerPixel;

      cmd.Barrier({
          .image = cubemapImage,
          .newLayout = ImageLayout::TransferDst,
          .aspect = ImageAspect::Color,
      });

      for (u32 face = 0; face < 6; ++face) {
        BufferImageCopyRegion region{};

        region.bufferOffset = face * faceByteSize;
        region.bufferRowLength = 0;   // tightly packed
        region.bufferImageHeight = 0; // tightly packed

        region.mipLevel = 0;
        region.baseArrayLayer = face;
        region.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            faceSize,
            faceSize,
            1,
        };

        region.aspect = ImageAspect::Color;

        cmd.CopyBufferToImage(cubemapStagingBuffer, cubemapImage, region);
      }

      cmd.Barrier({
          .image = cubemapImage,
          .newLayout = ImageLayout::ShaderReadOnly,
          .aspect = ImageAspect::Color,
      });

      uploaded = true;
    }

    cmd.Barrier(ImageBarrier{
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

    glm::mat4 model =
        glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), (float)dims.width / dims.height, 0.1f, 100.0f);

    proj[1][1] *= -1.0f; // Vulkan clip space
    glm::mat4 mvp = proj * view * model;

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

    struct Push {
      glm::mat4 view;
      glm::mat4 proj;
    };

    Push push{};
    push.view = glm::mat4(glm::mat3(view));
    push.proj = proj;

    cmd.BindPipeline(pipeline);
    cmd.BindDescriptorSet(pipeline, 0, descriptorSet);
    cmd.BindVertexBuffer(0, vertexBuffer, 0);
    cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(Push), &push);
    cmd.Draw(static_cast<Velos::u32>(skyboxVertices.size()));

    imguiRenderer.Render(cmd, ImGui::GetDrawData());

    cmd.EndRendering();

    cmd.Barrier(ImageBarrier{
        .image = frame.backbufferImage,
        .newLayout = ImageLayout::Present,
        .aspect = ImageAspect::Color,
    });

    cmd.End();

    device->SubmitAndPresent(frame.commandList, swapchain);
  }

  device->WaitIdle();

  imguiRenderer.Shutdown();
  ImGui::DestroyContext();

  device->DestroyPipeline(pipeline);
  device->DestroyShader(fragmentShader);
  device->DestroyShader(vertexShader);
  device->DestroyDescriptorPool(descriptorPool);
  device->DestroyDescriptorSetLayout(setLayout);
  device->DestroySampler(sampler);
  device->DestroyImageView(cubemapImageView);
  device->DestroyImage(cubemapImage);
  device->DestroyBuffer(cubemapStagingBuffer);
  device->DestroyBuffer(vertexBuffer);
  device->DestroySwapchain(swapchain);
  DestroyDevice(device);
}

} // namespace Rodan
