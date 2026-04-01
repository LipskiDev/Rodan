#include "core/application.h"

#include "../../external/velos/velos/core/application.h"
#include "../../external/velos/velos/core/window.h"
#include "../../external/velos/velos/rhi/rhi_command_list.h"
#include "../../external/velos/velos/rhi/rhi_device.h"
#include "../../external/velos/velos/rhi/rhi_pipeline.h"
#include "../../external/velos/velos/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace Rodan {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
};

Application::Application() { std::cout << "[Rodan] Application created\n"; }

Application::~Application() { std::cout << "[Rodan] Application destroyed\n"; }

void Application::Run() {
  using namespace Velos;
  using namespace Velos::RHI;

  std::cout << "[Rodan] Creating Velos app\n";
  Velos::Application app({
      .width = 1280,
      .height = 720,
      .title = "Rodan Cube",
      .resizable = true,
  });

  std::cout << "[Rodan] Creating device\n";
  IDevice *device = CreateDevice({
      .backend = BackendAPI::Vulkan,
      .enableValidation = true,
      .applicationName = "Rodan Cube",
  });

  std::cout << "[Rodan] Creating swapchain\n";
  SwapchainHandle swapchain = device->CreateSwapchain({
      .windowHandle = app.GetWindow().GetNativeHandle(),
      .width = static_cast<u32>(app.GetWindow().GetWidth()),
      .height = static_cast<u32>(app.GetWindow().GetHeight()),
      .format = Format::BGRA8_UNORM,
      .bufferCount = 2,
      .vsync = true,
      .debugName = "Main Swapchain",
  });

  std::vector<Vertex> vertices = {
      {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}},
      {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.3f, 0.3f, 1.0f}},
  };

  std::vector<std::uint16_t> indices = {
      // Front (+Z)
      0,
      1,
      2,
      0,
      2,
      3,

      // Back (-Z)
      5,
      4,
      7,
      5,
      7,
      6,

      // Left (-X)
      4,
      0,
      3,
      4,
      3,
      7,

      // Right (+X)
      1,
      5,
      6,
      1,
      6,
      2,

      // Top (+Y)
      3,
      2,
      6,
      3,
      6,
      7,

      // Bottom (-Y)
      4,
      5,
      1,
      4,
      1,
      0,
  };

  std::cout << "[Rodan] Creating vertex buffer\n";
  BufferHandle vertexBuffer = device->CreateBuffer({
      .size = static_cast<u64>(vertices.size() * sizeof(Vertex)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = vertices.data(),
      .debugName = "Cube Vertex Buffer",
  });

  std::cout << "[Rodan] Creating index buffer\n";
  BufferHandle indexBuffer = device->CreateBuffer({
      .size = static_cast<u64>(indices.size() * sizeof(std::uint16_t)),
      .usage = BufferUsage::Index,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = indices.data(),
      .debugName = "Cube Index Buffer",
  });

  auto vertSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/cube.vert",
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/cube.frag",
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
      .debugName = "Cube Vertex Shader",
  });

  ShaderHandle fragmentShader = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Cube Fragment Shader",
  });

  VertexBufferLayoutDesc vertexLayout{
      .stride = sizeof(Vertex),
      .inputRate = VertexInputRate::PerVertex,
      .attributes = {{
                         .location = 0,
                         .format = VertexFormat::Float32x3,
                         .offset = static_cast<u32>(offsetof(Vertex, pos)),
                     },
                     {
                         .location = 1,
                         .format = VertexFormat::Float32x3,
                         .offset = static_cast<u32>(offsetof(Vertex, color)),
                     }},
  };

  Extent2D initialDims = device->GetSwapchainDimensions();

  ImageHandle depthImage = device->CreateImage({
      .width = initialDims.width,
      .height = initialDims.height,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Format::D32_FLOAT,
      .usage = ImageUsage::DepthStencil,
      .debugName = "Main Depth Image",
  });

  ImageViewHandle depthView = device->CreateImageView({
      .image = depthImage,
      .format = Format::D32_FLOAT,
      .aspect = ImageAspect::Depth,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = "Main Depth View",
  });

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  pipelineDesc.vertexLayouts.push_back(vertexLayout);
  pipelineDesc.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.raster.cullBackFaces = false;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = false;
  pipelineDesc.colorFormat = Format::BGRA8_UNORM;
  pipelineDesc.depth = {
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthFormat = Format::D32_FLOAT,
  };
  pipelineDesc.debugName = "Cube Pipeline";

  PipelineHandle pipeline = device->CreateGraphicsPipeline(pipelineDesc);

  DepthAttachmentDesc depthAttachment{};
  depthAttachment.view = depthView;
  depthAttachment.loadOp = LoadOp::Clear;
  depthAttachment.storeOp = StoreOp::Store;
  depthAttachment.clearDepth = 1.0f;
  depthAttachment.clearStencil = 0;

  std::cout << "[Rodan] Entering render loop\n";

  float time = 0.0f;
  while (!app.GetWindow().ShouldClose()) {
    app.GetWindow().PollEvents();

    if (app.GetWindow().WasFramebufferResized()) {
      app.GetWindow().ResetFramebufferResizedFlag();

      const u32 fbWidth =
          static_cast<u32>(app.GetWindow().GetFramebufferWidth());
      const u32 fbHeight =
          static_cast<u32>(app.GetWindow().GetFramebufferHeight());

      if (fbWidth > 0 && fbHeight > 0) {
        device->WaitIdle();
        device->ResizeSwapchain(swapchain, fbWidth, fbHeight);

        Extent2D newDims = device->GetSwapchainDimensions();

        device->DestroyImageView(depthView);
        device->DestroyImage(depthImage);

        depthImage = device->CreateImage({
            .width = newDims.width,
            .height = newDims.height,
            .depth = 1,
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = Format::D32_FLOAT,
            .usage = ImageUsage::DepthStencil,
            .debugName = "Main Depth Image",
        });

        depthView = device->CreateImageView({
            .image = depthImage,
            .format = Format::D32_FLOAT,
            .aspect = ImageAspect::Depth,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .debugName = "Main Depth View",
        });

        depthAttachment.view = depthView;
      }

      continue;
    }

    Extent2D dims = device->GetSwapchainDimensions();
    if (dims.width == 0 || dims.height == 0) {
      continue;
    }

    time += 0.016f;

    FrameBeginResult frame = device->BeginFrame(swapchain);
    if (!frame.success) {
      continue;
    }

    ICommandList &cmd = device->GetCommandList(frame.commandList);

    ColorAttachmentDesc colorAttachment{};
    colorAttachment.view = frame.backbuffer;
    colorAttachment.loadOp = LoadOp::Clear;
    colorAttachment.storeOp = StoreOp::Store;
    colorAttachment.clearValue = {0.08f, 0.08f, 0.12f, 1.0f};

    RenderingInfo renderingInfo{};
    renderingInfo.renderArea = {{0, 0}, {dims.width, dims.height}};
    renderingInfo.colorAttachments = &colorAttachment;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.depthAttachment = &depthAttachment;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, time, glm::vec3(0.4f, 1.0f, 0.2f));

    glm::mat4 view =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 2.5f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      static_cast<float>(dims.width) /
                                          static_cast<float>(dims.height),
                                      0.1f, 100.0f);
    proj[1][1] *= -1.0f;

    glm::mat4 mvp = proj * view * model;

    cmd.Begin();

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

    cmd.Barrier({
        .image = frame.backbufferImage,
        .newLayout = ImageLayout::ColorAttachment,
        .aspect = ImageAspect::Color,
    });

    cmd.Barrier({
        .image = depthImage,
        .newLayout = ImageLayout::DepthAttachment,
        .aspect = ImageAspect::Depth,
    });

    cmd.BeginRendering(renderingInfo);
    cmd.BindPipeline(pipeline);
    cmd.BindVertexBuffer(0, vertexBuffer, 0);
    cmd.BindIndexBuffer(indexBuffer, IndexType::U16, 0);
    cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(glm::mat4), &mvp);
    cmd.DrawIndexed(static_cast<u32>(indices.size()));
    cmd.EndRendering();

    cmd.Barrier({
        .image = frame.backbufferImage,
        .newLayout = ImageLayout::Present,
        .aspect = ImageAspect::Color,
    });

    cmd.End();

    device->SubmitAndPresent(frame.commandList, swapchain);
  }

  std::cout << "[Rodan] Shutting down\n";

  device->WaitIdle();

  device->DestroyPipeline(pipeline);
  device->DestroyBuffer(indexBuffer);
  device->DestroyBuffer(vertexBuffer);
  device->DestroyShader(fragmentShader);
  device->DestroyShader(vertexShader);
  device->DestroyImageView(depthView);
  device->DestroyImage(depthImage);
  device->DestroySwapchain(swapchain);
  DestroyDevice(device);

  std::cout << "[Rodan] Shutdown complete\n";
}

} // namespace Rodan
