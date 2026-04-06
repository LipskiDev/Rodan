#include "glm/ext/vector_float4.hpp"
#include "rhi/rhi_types.h"
#include <glm/gtc/random.hpp>
#include <iostream>
#include <samples/scenes/million_cubes.h>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

void MillionCubesScene::Initialize(Velos::RHI::IDevice *device,
                                   Velos::RHI::SwapchainHandle swapchain,
                                   Velos::RHI::Format colorFormat,
                                   Velos::RHI::Format depthFormat) {
  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1500.0f);

  CreateResources(device_);
  CreateDescriptors(device_);
  CreatePipeline(device_);
}

void MillionCubesScene::Shutdown(Velos::RHI::IDevice *device) {
  if (!device) {
    return;
  }

  device->DestroyPipeline(cubes_.pipeline);
  device->DestroyShader(cubes_.fragmentShader);
  device->DestroyShader(cubes_.vertexShader);

  device->DestroyDescriptorPool(cubes_.descriptorPool);
  device->DestroyDescriptorSetLayout(cubes_.setLayout);

  device->DestroySampler(cubes_.xorPatternSampler);
  device->DestroyImageView(cubes_.xorPatternImageView);
  device->DestroyImage(cubes_.xorPatternImage);

  device->DestroyBuffer(cubes_.stagingBuffer);
  device->DestroyBuffer(cubes_.positionBuffer);

  cubes_ = {};
  device_ = nullptr;
  swapchain_ = {};
}

void MillionCubesScene::OnResize(Velos::RHI::IDevice *device, Velos::u32 width,
                                 Velos::u32 height) {}

void MillionCubesScene::Update(float deltaSeconds,
                               const SceneUpdateContext &ctx) {

  if (ctx.framebufferWidth == 0 || ctx.framebufferHeight == 0) {
    return;
  }

  camera_.SetPerspective(60.0f,
                         static_cast<float>(ctx.framebufferWidth) /
                             static_cast<float>(ctx.framebufferHeight),
                         0.1f, 1500.0f);

  if (ctx.input) {
    for (const InputEvent &event : ctx.input->GetEvents()) {
      if (event.type == InputEventType::KeyDown ||
          event.type == InputEventType::KeyUp) {
        camera_.OnKeyboard(event);
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

        if (ctx.input->IsMouseDown(MouseButton::Right)) {
          camera_.OnMouseMove(dx, dy);
        }
      }
    }
  }

  camera_.Update(deltaSeconds);
}

void MillionCubesScene::Prepare(Velos::RHI::ICommandList &cmd) {
  UploadTextureIfNeeded(cmd);
}
void MillionCubesScene::Render(Velos::RHI::ICommandList &cmd) {
  RenderMillionCubes(cmd);
}

void MillionCubesScene::RenderImGui() {}

void MillionCubesScene::CreateResources(Velos::RHI::IDevice *device) {

  std::vector<glm::vec4> cubeCenters(k_CubesCount);
  for (glm::vec4 &p : cubeCenters) {
    p = glm::vec4(glm::linearRand(-glm::vec3(500.0f), +glm::vec3(500.0f)),
                  glm::linearRand(0.0f, 3.14159f));
  }

  cubes_.positionBuffer = device->CreateBuffer(
      {.size = static_cast<u64>(k_CubesCount * sizeof(glm::vec4)),
       .usage = Velos::RHI::BufferUsage::Storage |
                Velos::RHI::BufferUsage::ShaderDeviceAddress,
       .memoryUsage = Velos::RHI::MemoryUsage::CPUToGPU,
       .initialData = cubeCenters.data(),
       .debugName = "Cube Positions Buffer"});

  const uint32_t texWidth = 256;
  const uint32_t texHeight = 256;
  cubes_.textureHeight = texHeight;
  cubes_.textureWidth = texWidth;
  std::vector<uint32_t> pixels(texWidth * texHeight);
  for (uint32_t y = 0; y < texHeight; y++) {
    for (uint32_t x = 0; x < texWidth; x++) {
      // create a XOR pattern
      pixels[y * texWidth + x] =
          0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
    }
  }

  cubes_.xorPatternImage = device->CreateImage({
      .width = texWidth,
      .height = texHeight,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Velos::RHI::Format::BGRA8_UNORM,
      .type = Velos::RHI::ImageType::Image2D,
      .usage =
          Velos::RHI::ImageUsage::Sampled | Velos::RHI::ImageUsage::TransferDst,
  });

  cubes_.xorPatternImageView =
      device->CreateImageView({.image = cubes_.xorPatternImage,
                               .format = Format::BGRA8_UNORM,
                               .type = ImageViewType::View2D,
                               .aspect = ImageAspect::Color,
                               .baseMipLevel = 0,
                               .mipLevelCount = 1,
                               .baseArrayLayer = 0,
                               .arrayLayerCount = 1,
                               .debugName = "XOR Image View"});

  cubes_.xorPatternSampler = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::Repeat,
      .addressV = SamplerAddressMode::Repeat,
      .addressW = SamplerAddressMode::Repeat,
      .debugName = "XOR Sampler",
  });

  const u64 imageSize =
      static_cast<u64>(cubes_.textureHeight * cubes_.textureWidth * 4ull);

  cubes_.stagingBuffer = device->CreateBuffer({
      .size = imageSize,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = pixels.data(),
      .debugName = "Duck Texture Upload Buffer",
  });
}

void MillionCubesScene::CreateDescriptors(Velos::RHI::IDevice *device) {

  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  cubes_.setLayout = device->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "Cubes Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  cubes_.descriptorPool = device->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "Cubes Descriptor Pool",
  });

  cubes_.descriptorSet = device->AllocateDescriptorSet(
      cubes_.descriptorPool, cubes_.setLayout, "Cubes Descriptor Set");

  DescriptorImageInfo imageInfo{};
  imageInfo.sampler = cubes_.xorPatternSampler;
  imageInfo.imageView = cubes_.xorPatternImageView;
  imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device->UpdateDescriptorSet({
      .dstSet = cubes_.descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });
}

void MillionCubesScene::CreatePipeline(Velos::RHI::IDevice *device) {

  auto vertSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/cubes.vert",
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/cubes.frag",
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  cubes_.vertexShader = device->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Million Cubes Vertex Shader",
  });

  cubes_.fragmentShader = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Million Cubes Fragment Shader",
  });

  DescriptorSetLayoutHandle setLayouts[] = {cubes_.setLayout};

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = cubes_.vertexShader;
  pipelineDesc.fragmentShader = cubes_.fragmentShader;
  pipelineDesc.layout.descriptorSetLayouts = setLayouts;
  pipelineDesc.layout.descriptorSetLayoutCount = 1;
  pipelineDesc.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.raster.cullBackFaces = false;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = false;
  pipelineDesc.blend = {.enable = false};
  pipelineDesc.colorFormat = colorFormat_;
  pipelineDesc.depth = {
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthFormat = depthFormat_,
  };
  pipelineDesc.debugName = "Million Cubes Scene Pipeline";

  cubes_.pipeline = device->CreateGraphicsPipeline(pipelineDesc);
}

void MillionCubesScene::UploadTextureIfNeeded(Velos::RHI::ICommandList &cmd) {
  if (cubes_.uploaded) {
    return;
  }

  cmd.Barrier({
      .image = cubes_.xorPatternImage,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
  });

  BufferImageCopyRegion region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.mipLevel = 0;
  region.baseArrayLayer = 0;
  region.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {cubes_.textureWidth, cubes_.textureHeight, 1};
  region.aspect = ImageAspect::Color;

  cmd.CopyBufferToImage(cubes_.stagingBuffer, cubes_.xorPatternImage, region);

  cmd.Barrier({
      .image = cubes_.xorPatternImage,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
  });

  cubes_.uploaded = true;
}

void MillionCubesScene::RenderMillionCubes(Velos::RHI::ICommandList &cmd) {
  MillionCubesPushConstants push{};
  push.viewproj = camera_.GetProjection() * camera_.GetView();

  const uint64_t addr = device_->GetBufferDeviceAddress(cubes_.positionBuffer);
  // std::cout << "Position buffer device address: " << addr << "\n";
  push.bufIdLo = static_cast<uint32_t>(addr & 0xffffffffull);
  push.bufIdHi = static_cast<uint32_t>((addr >> 32) & 0xffffffffull);

  cmd.BindPipeline(cubes_.pipeline);
  cmd.BindDescriptorSet(cubes_.pipeline, 0, cubes_.descriptorSet);
  cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(MillionCubesPushConstants),
                    &push);

  cmd.Draw(36, k_CubesCount);
}
} // namespace Rodan
