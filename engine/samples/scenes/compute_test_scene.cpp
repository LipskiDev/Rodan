#include "samples/scenes/compute_test_scene.h"

#include "imgui.h"
#include "path.h"
#include "shader/shader_compiler.h"

#include <stdexcept>

namespace Rodan {

namespace VRHI = Velos::RHI;

void ComputeTestScene::Initialize(VRHI::IDevice *device,
                                  VRHI::SwapchainHandle swapchain,
                                  VRHI::Format colorFormat,
                                  VRHI::Format depthFormat) {
  (void)colorFormat;
  (void)depthFormat;

  if (!device) {
    throw std::runtime_error("ComputeTestScene::Initialize: device is null");
  }

  device_ = device;
  swapchain_ = swapchain;

  auto csOutput = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/brdf_lut.comp").string(),
      .stage = VRHI::ShaderStage::Compute,
      .entryPoint = "main",
      .language = Velos::ShaderSourceLanguage::GLSL,
  });

  computeShader_ = device_->CreateShader({
      .stage = VRHI::ShaderStage::Compute,
      .bytecode = csOutput.spirv.data(),
      .bytecodeSize = csOutput.spirv.size() * sizeof(uint32_t),
      .entryPoint = "main",
      .reflection = csOutput.reflection,
      .debugName = "Compute Test Shader",
  });

  VRHI::DescriptorBindingDesc computeBindings[2]{};

  computeBindings[0].binding = 0;
  computeBindings[0].type = VRHI::DescriptorType::StorageImage;
  computeBindings[0].count = 1;
  computeBindings[0].visibility = VRHI::ShaderStage::Compute;

  computeBindings[1].binding = 1;
  computeBindings[1].type = VRHI::DescriptorType::StorageBuffer;
  computeBindings[1].count = 1;
  computeBindings[1].visibility = VRHI::ShaderStage::Compute;

  VRHI::DescriptorSetLayoutDesc layoutDesc{};
  layoutDesc.bindings = computeBindings;
  layoutDesc.bindingCount = 2;
  layoutDesc.debugName = "Compute Test Descriptor Set Layout";

  descriptorSetLayout_ = device_->CreateDescriptorSetLayout(layoutDesc);

  VRHI::DescriptorPoolSize poolSizes[2]{};

  poolSizes[0].type = VRHI::DescriptorType::StorageImage;
  poolSizes[0].count = 1;

  poolSizes[1].type = VRHI::DescriptorType::StorageBuffer;
  poolSizes[1].count = 1;

  VRHI::DescriptorPoolDesc poolDesc{};
  poolDesc.poolSizes = poolSizes;
  poolDesc.poolSizeCount = 2;
  poolDesc.maxSets = 1;
  poolDesc.debugName = "Compute Test Descriptor Pool";

  descriptorPool_ = device_->CreateDescriptorPool(poolDesc);

  descriptorSet_ = device_->AllocateDescriptorSet(
      descriptorPool_, descriptorSetLayout_, "Compute Test Descriptor Set");

  std::array<glm::vec4, kPatternColorCount> patternColors{};

  for (Velos::u32 i = 0; i < kPatternColorCount; ++i) {
    float t =
        static_cast<float>(i) / static_cast<float>(kPatternColorCount - 1);

    patternColors[i] = glm::vec4(1.0f);
  }

  patternBuffer_ = device_->CreateBuffer({
      .size = sizeof(glm::vec4) * kPatternColorCount,
      .usage = VRHI::BufferUsage::Storage,
      .memoryUsage = VRHI::MemoryUsage::CPUToGPU,
      .initialData = patternColors.data(),
      .debugName = "Compute Test Pattern Storage Buffer",
  });

  VRHI::DescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = patternBuffer_;
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(glm::vec4) * kPatternColorCount;

  VRHI::WriteDescriptorDesc bufferWrite{};
  bufferWrite.dstSet = descriptorSet_;
  bufferWrite.binding = 1;
  bufferWrite.arrayElement = 0;
  bufferWrite.type = VRHI::DescriptorType::StorageBuffer;
  bufferWrite.bufferInfo = &bufferInfo;
  bufferWrite.descriptorCount = 1;

  device_->UpdateDescriptorSet(bufferWrite);

  VRHI::DescriptorSetLayoutHandle layouts[] = {descriptorSetLayout_};

  VRHI::ComputePipelineDesc pipelineDesc{};
  pipelineDesc.computeShader = computeShader_;
  pipelineDesc.layout.descriptorSetLayouts = layouts;
  pipelineDesc.layout.descriptorSetLayoutCount = 1;
  pipelineDesc.debugName = "Compute Test Pipeline";

  computePipeline_ = device_->CreateComputePipeline(pipelineDesc);

  CreateOutputImage(width_, height_);

  outputSampler_ = device_->CreateSampler({
      .minFilter = VRHI::Filter::Linear,
      .magFilter = VRHI::Filter::Linear,
      .addressU = VRHI::SamplerAddressMode::ClampToEdge,
      .addressV = VRHI::SamplerAddressMode::ClampToEdge,
      .addressW = VRHI::SamplerAddressMode::ClampToEdge,
      .debugName = "Compute Test Output Sampler",
  });

  VRHI::DescriptorBindingDesc displayBinding{};
  displayBinding.binding = 0;
  displayBinding.type = VRHI::DescriptorType::CombinedImageSampler;
  displayBinding.count = 1;
  displayBinding.visibility = VRHI::ShaderStage::Fragment;

  VRHI::DescriptorSetLayoutDesc displayLayoutDesc{};
  displayLayoutDesc.bindings = &displayBinding;
  displayLayoutDesc.bindingCount = 1;
  displayLayoutDesc.debugName = "Compute Test Display Layout";

  displaySetLayout_ = device_->CreateDescriptorSetLayout(displayLayoutDesc);

  VRHI::DescriptorPoolSize displayPoolSize{};
  displayPoolSize.type = VRHI::DescriptorType::CombinedImageSampler;
  displayPoolSize.count = 1;

  VRHI::DescriptorPoolDesc displayPoolDesc{};
  displayPoolDesc.poolSizes = &displayPoolSize;
  displayPoolDesc.poolSizeCount = 1;
  displayPoolDesc.maxSets = 1;
  displayPoolDesc.debugName = "Compute Test Display Pool";

  displayDescriptorPool_ = device_->CreateDescriptorPool(displayPoolDesc);

  displayDescriptorSet_ = device_->AllocateDescriptorSet(
      displayDescriptorPool_, displaySetLayout_, "Compute Test Display Set");

  VRHI::DescriptorImageInfo sampledInfo{};
  sampledInfo.sampler = outputSampler_;
  sampledInfo.imageView = outputImageView_;
  sampledInfo.imageLayout = VRHI::ImageLayout::ShaderReadOnly;

  VRHI::WriteDescriptorDesc sampledWrite{};
  sampledWrite.dstSet = displayDescriptorSet_;
  sampledWrite.binding = 0;
  sampledWrite.arrayElement = 0;
  sampledWrite.type = VRHI::DescriptorType::CombinedImageSampler;
  sampledWrite.imageInfo = &sampledInfo;
  sampledWrite.descriptorCount = 1;

  device_->UpdateDescriptorSet(sampledWrite);

  auto vsOutput = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/fullscreen_texture.vert")
                  .string(),
      .stage = VRHI::ShaderStage::Vertex,
      .entryPoint = "main",
      .language = Velos::ShaderSourceLanguage::GLSL,
  });

  auto fsOutput = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/fullscreen_texture.frag")
                  .string(),
      .stage = VRHI::ShaderStage::Fragment,
      .entryPoint = "main",
      .language = Velos::ShaderSourceLanguage::GLSL,
  });

  fullscreenVS_ = device_->CreateShader({
      .stage = VRHI::ShaderStage::Vertex,
      .bytecode = vsOutput.spirv.data(),
      .bytecodeSize = vsOutput.spirv.size() * sizeof(uint32_t),
      .entryPoint = "main",
      .reflection = vsOutput.reflection,
      .debugName = "Fullscreen Texture VS",
  });

  fullscreenFS_ = device_->CreateShader({
      .stage = VRHI::ShaderStage::Fragment,
      .bytecode = fsOutput.spirv.data(),
      .bytecodeSize = fsOutput.spirv.size() * sizeof(uint32_t),
      .entryPoint = "main",
      .reflection = fsOutput.reflection,
      .debugName = "Fullscreen Texture FS",
  });

  VRHI::DescriptorSetLayoutHandle displayLayouts[] = {displaySetLayout_};

  VRHI::GraphicsPipelineDesc fullscreenDesc{};
  fullscreenDesc.vertexShader = fullscreenVS_;
  fullscreenDesc.fragmentShader = fullscreenFS_;
  fullscreenDesc.topology = VRHI::PrimitiveTopology::TriangleList;
  fullscreenDesc.colorFormat = colorFormat;
  fullscreenDesc.depth.depthFormat = VRHI::Format::Undefined;
  fullscreenDesc.depth.depthTestEnable = false;
  fullscreenDesc.depth.depthWriteEnable = false;
  fullscreenDesc.layout.descriptorSetLayouts = displayLayouts;
  fullscreenDesc.layout.descriptorSetLayoutCount = 1;
  fullscreenDesc.debugName = "Compute Test Fullscreen Pipeline";

  fullscreenDesc.raster.cullBackFaces = false;
  fullscreenDesc.raster.frontFaceCCW = true;
  fullscreenDesc.raster.wireframe = false;
  fullscreenDesc.blend.enable = false;

  fullscreenPipeline_ = device_->CreateGraphicsPipeline(fullscreenDesc);
}

void ComputeTestScene::Shutdown(VRHI::IDevice *device) {
  if (!device) {
    return;
  }

  device->WaitIdle();

  DestroyOutputImage();

  if (patternBuffer_.IsValid()) {
    device->DestroyBuffer(patternBuffer_);
    patternBuffer_ = {};
  }

  if (fullscreenPipeline_.IsValid()) {
    device->DestroyPipeline(fullscreenPipeline_);
    fullscreenPipeline_ = {};
  }

  if (fullscreenVS_.IsValid()) {
    device->DestroyShader(fullscreenVS_);
    fullscreenVS_ = {};
  }

  if (fullscreenFS_.IsValid()) {
    device->DestroyShader(fullscreenFS_);
    fullscreenFS_ = {};
  }

  if (displayDescriptorPool_.IsValid()) {
    device->DestroyDescriptorPool(displayDescriptorPool_);
    displayDescriptorPool_ = {};
  }

  if (displaySetLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(displaySetLayout_);
    displaySetLayout_ = {};
  }

  if (outputSampler_.IsValid()) {
    device->DestroySampler(outputSampler_);
    outputSampler_ = {};
  }

  if (computePipeline_.IsValid()) {
    device->DestroyPipeline(computePipeline_);
    computePipeline_ = {};
  }

  if (descriptorPool_.IsValid()) {
    device->DestroyDescriptorPool(descriptorPool_);
    descriptorPool_ = {};
  }

  if (descriptorSetLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(descriptorSetLayout_);
    descriptorSetLayout_ = {};
  }

  if (computeShader_.IsValid()) {
    device->DestroyShader(computeShader_);
    computeShader_ = {};
  }

  device_ = nullptr;
  swapchain_ = {};
}

void ComputeTestScene::OnResize(VRHI::IDevice *device, Velos::u32 width,
                                Velos::u32 height) {
  (void)device;
  (void)width;
  (void)height;
}

void ComputeTestScene::Update(float deltaSeconds,
                              const SceneUpdateContext &ctx) {
  (void)deltaSeconds;

  if (ctx.framebufferWidth == 0 || ctx.framebufferHeight == 0) {
    return;
  }
}

void ComputeTestScene::Prepare(VRHI::ICommandList &cmd) {
  if (dispatched_) {
    return;
  }

  cmd.Barrier(VRHI::ImageBarrier{
      .image = outputImage_,
      .oldLayout = VRHI::ImageLayout::Undefined,
      .newLayout = VRHI::ImageLayout::General,
      .aspect = VRHI::ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });

  cmd.BindComputePipeline(computePipeline_);
  cmd.BindComputeDescriptorSet(computePipeline_, 0, descriptorSet_);

  constexpr Velos::u32 localSizeX = 8;
  constexpr Velos::u32 localSizeY = 8;

  const Velos::u32 groupsX = (width_ + localSizeX - 1) / localSizeX;
  const Velos::u32 groupsY = (height_ + localSizeY - 1) / localSizeY;

  printf("DISPATCHING\n");
  cmd.Dispatch(groupsX, groupsY, 1);
  printf("DONE\n");

  cmd.Barrier(VRHI::ImageBarrier{
      .image = outputImage_,
      .oldLayout = VRHI::ImageLayout::General,
      .newLayout = VRHI::ImageLayout::ShaderReadOnly,
      .aspect = VRHI::ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });

  dispatched_ = true;
}

void ComputeTestScene::Render(VRHI::ICommandList &cmd,
                              const FrameRenderContext &frame) {
  cmd.Barrier(VRHI::ImageBarrier{
      .image = frame.backbufferImage,
      .oldLayout = VRHI::ImageLayout::Undefined,
      .newLayout = VRHI::ImageLayout::ColorAttachment,
      .aspect = VRHI::ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });

  VRHI::ColorAttachmentDesc color{};
  color.view = frame.backbufferView;
  color.loadOp = VRHI::LoadOp::Clear;
  color.storeOp = VRHI::StoreOp::Store;
  color.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

  VRHI::RenderingInfo info{};
  info.renderArea.offset = {0, 0};
  info.renderArea.extent = frame.extent;
  info.colorAttachments = &color;
  info.colorAttachmentCount = 1;
  info.depthAttachment = nullptr;

  cmd.BeginRendering(info);

  cmd.SetViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(frame.extent.width),
      .height = static_cast<float>(frame.extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  });

  cmd.SetScissor({
      .offset = {0, 0},
      .extent = frame.extent,
  });

  cmd.BindPipeline(fullscreenPipeline_);
  cmd.BindDescriptorSet(fullscreenPipeline_, 0, displayDescriptorSet_);
  cmd.Draw(3);

  if (frame.renderUi) {
    frame.renderUi(cmd);
  }

  cmd.EndRendering();

  cmd.Barrier(VRHI::ImageBarrier{
      .image = frame.backbufferImage,
      .oldLayout = VRHI::ImageLayout::ColorAttachment,
      .newLayout = VRHI::ImageLayout::Present,
      .aspect = VRHI::ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });
}

void ComputeTestScene::RenderImGui() {
  ImGui::Begin("Compute Test");

  ImGui::Text("Output image: %ux%u", width_, height_);
  ImGui::Text("Dispatched: %s", dispatched_ ? "yes" : "no");

  if (ImGui::Button("Dispatch Again")) {
    dispatched_ = false;
  }

  ImGui::End();
}

void ComputeTestScene::CreateOutputImage(Velos::u32 width, Velos::u32 height) {
  DestroyOutputImage();

  width_ = width;
  height_ = height;

  outputImage_ = device_->CreateImage({
      .width = width_,
      .height = height_,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = VRHI::Format::RGBA8_UNORM,
      .usage = VRHI::ImageUsage::Storage | VRHI::ImageUsage::Sampled,
      .debugName = "Compute Test Output Image",
  });

  outputImageView_ = device_->CreateImageView({
      .image = outputImage_,
      .format = VRHI::Format::RGBA8_UNORM,
      .aspect = VRHI::ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = "Compute Test Output Image View",
  });

  VRHI::DescriptorImageInfo imageInfo{};
  imageInfo.imageView = outputImageView_;
  imageInfo.imageLayout = VRHI::ImageLayout::General;

  VRHI::WriteDescriptorDesc write{};
  write.dstSet = descriptorSet_;
  write.binding = 0;
  write.arrayElement = 0;
  write.type = VRHI::DescriptorType::StorageImage;
  write.bufferInfo = nullptr;
  write.imageInfo = &imageInfo;
  write.descriptorCount = 1;

  device_->UpdateDescriptorSet(write);

  dispatched_ = false;
}

void ComputeTestScene::DestroyOutputImage() {
  if (!device_) {
    return;
  }

  if (outputImageView_.IsValid()) {
    device_->DestroyImageView(outputImageView_);
    outputImageView_ = {};
  }

  if (outputImage_.IsValid()) {
    device_->DestroyImage(outputImage_);
    outputImage_ = {};
  }
}

} // namespace Rodan
