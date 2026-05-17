#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <core/path.h>
#include <graphics/environment/ibl_baker.h>

namespace Rodan {

namespace {
constexpr uint32_t k_IrradianceSize = 32;

struct IrradiancePushConstants {
  glm::mat4 view;
  glm::mat4 proj;
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
}

IBLResources IBLBaker::BakeIrradiance(ICommandList &cmd,
                                      const EnvironmentMap &environment) {
  if (!device_) {
    throw std::runtime_error("IBLBaker::BakeIrradiance: baker not initialized");
  }

  IBLResources result{};

  CreateIrradianceResources(result);
  RenderIrradiance(cmd, environment, result);
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
        .debugName = "IBL Irradiance Face View",
    });
  }

  result.irradianceTexture.sampler = device_->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::ClampToEdge,
      .addressV = SamplerAddressMode::ClampToEdge,
      .addressW = SamplerAddressMode::ClampToEdge,
      .debugName = "IBL Irradiance Sampler",
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

void IBLBaker::CreateIBLDescriptorSet(IBLResources &result) {
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  result.descriptorSetLayout = device_->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "IBL Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
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

  device_->UpdateDescriptorSet({
      .dstSet = result.descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &irradianceInfo,
      .descriptorCount = 1,
  });
}

void IBLBaker::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  if (irradiancePipeline_) {
    device->DestroyPipeline(irradiancePipeline_);
    irradiancePipeline_ = {};
  }

  if (irradianceFS_) {
    device->DestroyShader(irradianceFS_);
    irradianceFS_ = {};
  }

  if (irradianceVS_) {
    device->DestroyShader(irradianceVS_);
    irradianceVS_ = {};
  }

  if (cubeVertexBuffer_) {
    device->DestroyBuffer(cubeVertexBuffer_);
    cubeVertexBuffer_ = {};
  }

  cubeVertexCount_ = 0;
  device_ = nullptr;
}
} // namespace Rodan
