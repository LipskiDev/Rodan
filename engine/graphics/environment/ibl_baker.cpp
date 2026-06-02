#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "graphics/texture.h"
#include "rhi/rhi_handles.h"
#include "rhi/rhi_pipeline.h"
#include "rhi/rhi_resources.h"
#include "rhi/rhi_types.h"
#include <core/path.h>
#include <graphics/environment/ibl_baker.h>

namespace Rodan {

namespace {
constexpr uint32_t k_IrradianceSize = 32;
constexpr uint32_t k_PrefilterSize = 128;
constexpr uint32_t k_PrefilterMipLevels = 8;
constexpr uint32_t k_BRDFLutSize = 512;

struct IrradiancePushConstants {
  glm::mat4 view;
  glm::mat4 proj;
};

struct PrefilterPushConstants {
  glm::mat4 view;
  glm::mat4 proj;
  float roughness;
};

std::array<glm::mat4, 6> CreateCaptureViews() {
  return {
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
      glm::lookAtRH(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)),
  };
}

glm::mat4 CreateCaptureProjection() {
  glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  return proj;
}

std::vector<glm::vec3> CreateCubeVertices() {
  return {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1},
      {1, 1, -1},   {-1, 1, -1}, {-1, -1, -1},

      {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},
      {1, 1, 1},    {-1, 1, 1},  {-1, -1, 1},

      {-1, 1, 1},   {-1, 1, -1}, {-1, -1, -1},
      {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1},

      {1, 1, 1},    {1, 1, -1},  {1, -1, -1},
      {1, -1, -1},  {1, -1, 1},  {1, 1, 1},

      {-1, -1, -1}, {1, -1, -1}, {1, -1, 1},
      {1, -1, 1},   {-1, -1, 1}, {-1, -1, -1},

      {-1, 1, -1},  {1, 1, -1},  {1, 1, 1},
      {1, 1, 1},    {-1, 1, 1},  {-1, 1, -1},
  };
}
} // namespace

void IBLBaker::Initialize(IDevice *device,
                          DescriptorSetLayoutHandle environmentSetLayout) {
  if (!device) {
    throw std::runtime_error("IBLBaker::Initialize: device is null");
  }

  device_ = device;

  const std::vector<glm::vec3> vertices = CreateCubeVertices();
  cubeVertexCount_ = static_cast<uint32_t>(vertices.size());

  cubeVertexBuffer_ = device_->CreateBuffer({
      .size = static_cast<uint64_t>(vertices.size() * sizeof(glm::vec3)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = vertices.data(),
      .debugName = "IBLBaker Cube Vertex Buffer",
  });

  auto vertSpv = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/irradiance_convolution.vert")
                  .string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/irradiance_convolution.frag")
                  .string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  irradianceVS_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "IBL Irradiance VS",
  });

  irradianceFS_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "IBL Irradiance FS",
  });

  vertSpv = Velos::ShaderCompiler::CompileFile({
      .path =
          Velos::Path::Resolve("assets/shaders/prefilter_envmap.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  fragSpv = Velos::ShaderCompiler::CompileFile({
      .path =
          Velos::Path::Resolve("assets/shaders/prefilter_envmap.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  prefilterVS_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "IBL Prefilter VS",
  });

  prefilterFS_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "IBL Prefilter FS",
  });

  auto compSpv = Velos::ShaderCompiler::CompileFile({
      .path = Velos::Path::Resolve("assets/shaders/brdf_lut.comp").string(),
      .stage = ShaderStage::Compute,
      .entryPoint = "main",
  });

  brdfLutCS_ = device_->CreateShader({
      .stage = ShaderStage::Compute,
      .bytecode = compSpv.spirv.data(),
      .bytecodeSize =
          static_cast<uint64_t>(compSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = compSpv.reflection,
      .debugName = "BRDF LUT CS",
  });

  VertexBufferLayoutDesc vertexLayout{
      .stride = sizeof(glm::vec3),
      .inputRate = VertexInputRate::PerVertex,
      .attributes = {{
          .location = 0,
          .format = VertexFormat::Float32x3,
          .offset = 0,
      }},
  };

  DescriptorSetLayoutHandle setLayouts[] = {
      environmentSetLayout,
  };

  GraphicsPipelineDesc desc{};
  desc.vertexShader = irradianceVS_;
  desc.fragmentShader = irradianceFS_;
  desc.vertexLayouts.push_back(vertexLayout);

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 1;

  desc.topology = PrimitiveTopology::TriangleList;

  desc.raster.cullBackFaces = false;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  desc.blend.enable = false;

  desc.colorFormat = Format::RGBA32_FLOAT;
  desc.depth.depthFormat = Format::Undefined;
  desc.depth.depthTestEnable = false;
  desc.depth.depthWriteEnable = false;

  desc.debugName = "IBL Irradiance Pipeline";

  irradiancePipeline_ = device_->CreateGraphicsPipeline(desc);

  if (!irradiancePipeline_.IsValid()) {
    throw std::runtime_error(
        "IBLBaker::Initialize: failed to create irradiance pipeline");
  }

  GraphicsPipelineDesc prefilterDesc{};
  prefilterDesc.vertexShader = prefilterVS_;
  prefilterDesc.fragmentShader = prefilterFS_;
  prefilterDesc.vertexLayouts.push_back(vertexLayout);

  prefilterDesc.layout.descriptorSetLayouts = setLayouts;
  prefilterDesc.layout.descriptorSetLayoutCount = 1;

  prefilterDesc.topology = PrimitiveTopology::TriangleList;

  prefilterDesc.raster.cullBackFaces = false;
  prefilterDesc.raster.frontFaceCCW = true;
  prefilterDesc.raster.wireframe = false;

  prefilterDesc.blend.enable = false;

  prefilterDesc.colorFormat = Format::RGBA32_FLOAT;
  prefilterDesc.depth.depthFormat = Format::Undefined;
  prefilterDesc.depth.depthTestEnable = false;
  prefilterDesc.depth.depthWriteEnable = false;

  prefilterDesc.debugName = "IBL Prefilter Pipeline";

  prefilterPipeline_ = device_->CreateGraphicsPipeline(prefilterDesc);

  if (!prefilterPipeline_.IsValid()) {
    throw std::runtime_error(
        "IBLBaker::Initialize: failed to create prefilter pipeline");
  }

  DescriptorBindingDesc brdfLutBindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::StorageImage,
          .count = 1,
          .visibility = ShaderStage::Compute,
      },
  };

  brdfLutSetLayout_ = device_->CreateDescriptorSetLayout({
      .bindings = brdfLutBindings,
      .bindingCount = 1,
      .debugName = "BRDF LUT Bake Set Layout",
  });

  DescriptorSetLayoutHandle brdfSetLayouts[] = {brdfLutSetLayout_};

  ComputePipelineDesc brdfLutDesc{};
  brdfLutDesc.computeShader = brdfLutCS_;
  brdfLutDesc.layout.descriptorSetLayouts = brdfSetLayouts;
  brdfLutDesc.layout.descriptorSetLayoutCount = 1;
  brdfLutDesc.debugName = "BRDF LUT Pipeline";

  brdfLutPipeline_ = device_->CreateComputePipeline(brdfLutDesc);

  if (!brdfLutPipeline_.IsValid()) {
    throw std::runtime_error(
        "IBLBaker::Initialize: failed to create BRDF LUT pipeline");
  }
}

IBLResources IBLBaker::BakeIrradiance(ICommandList &cmd,
                                      const EnvironmentMap &environment) {
  if (!device_) {
    throw std::runtime_error("IBLBaker::BakeIrradiance: baker not initialized");
  }

  IBLResources result{};

  CreateIrradianceResources(result);
  RenderIrradiance(cmd, environment, result);
  RenderPrefilter(cmd, environment, result);
  RenderBRDFLut(cmd, environment, result);
  CreateIBLDescriptorSet(result);

  return result;
}

void IBLBaker::CreateIrradianceResources(IBLResources &result) {
  result.irradianceTexture.image = device_->CreateImage({
      .width = k_IrradianceSize,
      .height = k_IrradianceSize,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 6,
      .format = Format::RGBA32_FLOAT,
      .type = ImageType::Cube,
      .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled,
      .debugName = "IBL Irradiance Cubemap",
  });

  result.irradianceTexture.view = device_->CreateImageView({
      .image = result.irradianceTexture.image,
      .format = Format::RGBA32_FLOAT,
      .type = ImageViewType::Cube,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6,
      .debugName = "IBL Irradiance Cubemap View",
  });

  result.irradianceTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .debugName = "IBL Irradiance Sampler",
  });

  result.prefilterTexture.image = device_->CreateImage({
      .width = k_PrefilterSize,
      .height = k_PrefilterSize,
      .depth = 1,
      .mipLevels = k_PrefilterMipLevels,
      .arrayLayers = 6,
      .format = Format::RGBA32_FLOAT,
      .type = ImageType::Cube,
      .usage = ImageUsage::ColorAttachment | ImageUsage::Sampled,
      .debugName = "IBL Prefilter Cubemap",
  });

  result.prefilterTexture.view = device_->CreateImageView({
      .image = result.prefilterTexture.image,
      .format = Format::RGBA32_FLOAT,
      .type = ImageViewType::Cube,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = k_PrefilterMipLevels,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6,
  });

  result.prefilterTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .minLod = 0.0f,
      .maxLod = static_cast<float>(k_PrefilterMipLevels - 1),
  });

  for (uint32_t face = 0; face < 6; ++face) {
    result.irradianceFaceViews[face] = device_->CreateImageView({
        .image = result.irradianceTexture.image,
        .format = Format::RGBA32_FLOAT,
        .type = ImageViewType::View2D,
        .aspect = ImageAspect::Color,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = face,
        .arrayLayerCount = 1,
    });
  }

  for (uint32_t mip = 0; mip < k_PrefilterMipLevels; ++mip) {
    for (uint32_t face = 0; face < 6; ++face) {

      uint32_t idx = mip * 6 + face;

      result.prefilterFaceMipViews[idx] = device_->CreateImageView({
          .image = result.prefilterTexture.image,
          .format = Format::RGBA32_FLOAT,
          .type = ImageViewType::View2D,
          .aspect = ImageAspect::Color,
          .baseMipLevel = mip,
          .mipLevelCount = 1,
          .baseArrayLayer = face,
          .arrayLayerCount = 1,
      });
    }
  }

  result.brdfLutTexture.image = device_->CreateImage({
      .width = k_BRDFLutSize,
      .height = k_BRDFLutSize,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Format::RG32_FLOAT,
      .type = ImageType::Image2D,
      .usage = ImageUsage::Storage | ImageUsage::Sampled,
      .debugName = "BRDF LUT Texture",
  });

  result.brdfLutTexture.view = device_->CreateImageView({
      .image = result.brdfLutTexture.image,
      .format = Format::RG32_FLOAT,
      .type = ImageViewType::View2D,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = "BRDF LUT View",
  });

  result.brdfLutTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .minLod = 0.0,
      .maxLod = 7.0,
      .debugName = "BRDF LUT Sampler",
  });
}

void IBLBaker::RenderIrradiance(ICommandList &cmd,
                                const EnvironmentMap &environment,
                                IBLResources &result) {
  const auto captureViews = CreateCaptureViews();
  const glm::mat4 captureProj = CreateCaptureProjection();

  cmd.Barrier({
      .image = result.irradianceTexture.image,
      .oldLayout = ImageLayout::Undefined,
      .newLayout = ImageLayout::ColorAttachment,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });

  for (uint32_t face = 0; face < 6; ++face) {
    ColorAttachmentDesc colorAttachment{};
    colorAttachment.view = result.irradianceFaceViews[face];
    colorAttachment.loadOp = LoadOp::Clear;
    colorAttachment.storeOp = StoreOp::Store;
    colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

    RenderingInfo renderingInfo{};
    renderingInfo.renderArea = {
        .offset = {0, 0},
        .extent = {k_IrradianceSize, k_IrradianceSize},
    };
    renderingInfo.colorAttachments = &colorAttachment;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.depthAttachment = nullptr;

    cmd.BeginRendering(renderingInfo);

    cmd.SetViewport({
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(k_IrradianceSize),
        .height = static_cast<float>(k_IrradianceSize),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    });

    cmd.SetScissor({
        .offset = {0, 0},
        .extent = {k_IrradianceSize, k_IrradianceSize},
    });

    IrradiancePushConstants push{};
    push.view = captureViews[face];
    push.proj = captureProj;

    cmd.BindPipeline(irradiancePipeline_);
    cmd.BindDescriptorSet(irradiancePipeline_, 0,
                          environment.GetDescriptorSet());
    cmd.BindVertexBuffer(0, cubeVertexBuffer_, 0);
    cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(IrradiancePushConstants),
                      &push);
    cmd.Draw(cubeVertexCount_);

    cmd.EndRendering();
  }

  cmd.Barrier({
      .image = result.irradianceTexture.image,
      .oldLayout = ImageLayout::ColorAttachment,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });
}

void IBLBaker::RenderPrefilter(ICommandList &cmd,
                               const EnvironmentMap &environment,
                               IBLResources &result) {
  const auto captureViews = CreateCaptureViews();
  const glm::mat4 captureProj = CreateCaptureProjection();

  cmd.Barrier({
      .image = result.prefilterTexture.image,
      .oldLayout = ImageLayout::Undefined,
      .newLayout = ImageLayout::ColorAttachment,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = k_PrefilterMipLevels,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });

  for (uint32_t mip = 0; mip < k_PrefilterMipLevels; ++mip) {

    const uint32_t mipSize = k_PrefilterSize >> mip;

    const float roughness =
        static_cast<float>(mip) / static_cast<float>(k_PrefilterMipLevels - 1);

    for (uint32_t face = 0; face < 6; ++face) {

      const uint32_t idx = mip * 6 + face;

      ColorAttachmentDesc colorAttachment{};
      colorAttachment.view = result.prefilterFaceMipViews[idx];
      colorAttachment.loadOp = LoadOp::Clear;
      colorAttachment.storeOp = StoreOp::Store;
      colorAttachment.clearValue = {
          0.0f,
          0.0f,
          0.0f,
          1.0f,
      };

      RenderingInfo renderingInfo{};
      renderingInfo.renderArea = {
          .offset = {0, 0},
          .extent = {mipSize, mipSize},
      };
      renderingInfo.colorAttachments = &colorAttachment;
      renderingInfo.colorAttachmentCount = 1;
      renderingInfo.depthAttachment = nullptr;

      cmd.BeginRendering(renderingInfo);

      cmd.SetViewport({
          .x = 0.0f,
          .y = 0.0f,
          .width = static_cast<float>(mipSize),
          .height = static_cast<float>(mipSize),
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
      });

      cmd.SetScissor({
          .offset = {0, 0},
          .extent = {mipSize, mipSize},
      });

      PrefilterPushConstants push{};
      push.view = captureViews[face];
      push.proj = captureProj;
      push.roughness = roughness;

      cmd.BindPipeline(prefilterPipeline_);

      cmd.BindDescriptorSet(prefilterPipeline_, 0,
                            environment.GetDescriptorSet());

      cmd.BindVertexBuffer(0, cubeVertexBuffer_, 0);

      cmd.PushConstants(ShaderStage::Vertex | ShaderStage::Fragment, 0,
                        sizeof(PrefilterPushConstants), &push);

      cmd.Draw(cubeVertexCount_);

      cmd.EndRendering();
    }
  }

  cmd.Barrier({
      .image = result.prefilterTexture.image,
      .oldLayout = ImageLayout::ColorAttachment,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = k_PrefilterMipLevels,
      .baseArrayLayer = 0,
      .layerCount = 6,
  });
}

void IBLBaker::RenderBRDFLut(Velos::RHI::ICommandList &cmd,
                             const EnvironmentMap &environment,
                             IBLResources &result) {
  (void)environment;

  if (brdfBakeDescriptorPool_.IsValid()) {
    device_->DestroyDescriptorPool(brdfBakeDescriptorPool_);
    brdfBakeDescriptorPool_ = {};
    brdfBakeDescriptorSet_ = {};
  }

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::StorageImage,
          .count = 1,
      },
  };

  brdfBakeDescriptorPool_ = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "BRDF LUT Bake Descriptor Pool",
  });

  brdfBakeDescriptorSet_ =
      device_->AllocateDescriptorSet(brdfBakeDescriptorPool_, brdfLutSetLayout_,
                                     "BRDF LUT Bake Descriptor Set");

  DescriptorImageInfo imageInfo{};
  imageInfo.imageView = result.brdfLutTexture.view;
  imageInfo.imageLayout = ImageLayout::General;

  device_->UpdateDescriptorSet({
      .dstSet = brdfBakeDescriptorSet_,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::StorageImage,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });

  cmd.Barrier({
      .image = result.brdfLutTexture.image,
      .oldLayout = ImageLayout::Undefined,
      .newLayout = ImageLayout::General,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });

  cmd.BindComputePipeline(brdfLutPipeline_);
  cmd.BindComputeDescriptorSet(brdfLutPipeline_, 0, brdfBakeDescriptorSet_);

  cmd.Dispatch((k_BRDFLutSize + 7) / 8, (k_BRDFLutSize + 7) / 8, 1);

  cmd.Barrier({
      .image = result.brdfLutTexture.image,
      .oldLayout = ImageLayout::General,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  });
}

void IBLBaker::CreateIBLDescriptorSet(IBLResources &result) {
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      {
          .binding = 1,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
      {
          .binding = 2,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  result.descriptorSetLayout = device_->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 3,
      .debugName = "IBL Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 3,
      },
  };

  result.descriptorPool = device_->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "IBL Descriptor Pool",
  });

  result.descriptorSet = device_->AllocateDescriptorSet(
      result.descriptorPool, result.descriptorSetLayout, "IBL Descriptor Set");

  DescriptorImageInfo irradianceInfo{};
  irradianceInfo.sampler = result.irradianceTexture.sampler;
  irradianceInfo.imageView = result.irradianceTexture.view;
  irradianceInfo.imageLayout = ImageLayout::ShaderReadOnly;

  DescriptorImageInfo prefilterInfo{};
  prefilterInfo.sampler = result.prefilterTexture.sampler;
  prefilterInfo.imageView = result.prefilterTexture.view;
  prefilterInfo.imageLayout = ImageLayout::ShaderReadOnly;

  DescriptorImageInfo brdfLutInfo{};
  brdfLutInfo.sampler = result.brdfLutTexture.sampler;
  brdfLutInfo.imageView = result.brdfLutTexture.view;
  brdfLutInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device_->UpdateDescriptorSet({
      .dstSet = result.descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &irradianceInfo,
      .descriptorCount = 1,
  });

  device_->UpdateDescriptorSet({
      .dstSet = result.descriptorSet,
      .binding = 1,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &prefilterInfo,
      .descriptorCount = 1,
  });

  device_->UpdateDescriptorSet({
      .dstSet = result.descriptorSet,
      .binding = 2,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &brdfLutInfo,
      .descriptorCount = 1,
  });
}

void IBLBaker::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  if (brdfLutPipeline_.IsValid()) {
    device->DestroyPipeline(brdfLutPipeline_);
    brdfLutPipeline_ = {};
  }

  if (prefilterPipeline_.IsValid()) {
    device->DestroyPipeline(prefilterPipeline_);
    prefilterPipeline_ = {};
  }

  if (irradiancePipeline_.IsValid()) {
    device->DestroyPipeline(irradiancePipeline_);
    irradiancePipeline_ = {};
  }

  if (brdfLutCS_.IsValid()) {
    device->DestroyShader(brdfLutCS_);
    brdfLutCS_ = {};
  }

  if (prefilterFS_.IsValid()) {
    device->DestroyShader(prefilterFS_);
    prefilterFS_ = {};
  }

  if (prefilterVS_.IsValid()) {
    device->DestroyShader(prefilterVS_);
    prefilterVS_ = {};
  }

  if (irradianceFS_.IsValid()) {
    device->DestroyShader(irradianceFS_);
    irradianceFS_ = {};
  }

  if (irradianceVS_.IsValid()) {
    device->DestroyShader(irradianceVS_);
    irradianceVS_ = {};
  }

  if (brdfBakeDescriptorPool_.IsValid()) {
    device->DestroyDescriptorPool(brdfBakeDescriptorPool_);
    brdfBakeDescriptorPool_ = {};
    brdfBakeDescriptorSet_ = {};
  }

  if (brdfLutSetLayout_.IsValid()) {
    device->DestroyDescriptorSetLayout(brdfLutSetLayout_);
    brdfLutSetLayout_ = {};
  }

  if (cubeVertexBuffer_.IsValid()) {
    device->DestroyBuffer(cubeVertexBuffer_);
    cubeVertexBuffer_ = {};
  }

  cubeVertexCount_ = 0;
  device_ = nullptr;
}
} // namespace Rodan
