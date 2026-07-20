#include "graphics/environment/environment_map.h"
#include "graphics/shader_cache.h"
#include <core/path.h>
#include <renderer/passes/skybox_pass.h>

namespace Rodan {
using namespace Velos;
using namespace Velos::RHI;
void SkyboxPass::Initialize(IDevice *device, Format colorFormat,
                            BindingLayoutHandle environmentSetLayout) {

  device_ = device;

  const std::vector<glm::vec3> vertices = {
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

  vertexCount_ = static_cast<uint32_t>(vertices.size());

  vertexBuffer_ = device_->CreateBuffer({
      .size = static_cast<uint64_t>(vertices.size() * sizeof(glm::vec3)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = vertices.data(),
      .debugName = "Skybox Vertex Buffer",
  });

  auto vertSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/skybox.vert").string(),
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCache::LoadOrCompile({
      .path = Velos::Path::Resolve("assets/shaders/skybox.frag").string(),
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  vertexShader_ = device_->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Skybox Vertex Shader",
  });

  fragmentShader_ = device_->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Skybox Fragment Shader",
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

  BindingLayoutHandle setLayouts[] = {
      environmentSetLayout,
  };

  GraphicsPipelineDesc desc{};
  desc.vertexShader = vertexShader_;
  desc.fragmentShader = fragmentShader_;
  desc.vertexLayouts.push_back(vertexLayout);

  desc.layout.descriptorSetLayouts = setLayouts;
  desc.layout.descriptorSetLayoutCount = 1;

  desc.topology = PrimitiveTopology::TriangleList;

  desc.depth.depthFormat = Format::D32_FLOAT;
  desc.depth.depthTestEnable = true;
  desc.depth.depthWriteEnable = false;
  desc.raster.cullBackFaces = false;
  desc.raster.frontFaceCCW = true;
  desc.raster.wireframe = false;

  desc.blend.enable = false;

  desc.colorFormat = colorFormat;

  desc.debugName = "Skybox Pipeline";

  pipeline_ = device_->CreateGraphicsPipeline(desc);
}

void SkyboxPass::Render(ICommandList &cmd, const EnvironmentMap &environment,
                        const glm::mat4 &view, const glm::mat4 &proj) {
  struct PushConstants {
    glm::mat4 view;
    glm::mat4 proj;
  };

  PushConstants push{};
  push.view = glm::mat4(glm::mat3(view));
  push.proj = proj;

  cmd.BindPipeline(pipeline_);
  cmd.SetBindings(pipeline_, 0, environment.GetBindingSet());
  cmd.BindVertexBuffer(0, vertexBuffer_, 0);
  cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(PushConstants), &push);
  cmd.Draw(vertexCount_);
}

void SkyboxPass::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  device->DestroyPipeline(pipeline_);
  device->DestroyShader(fragmentShader_);
  device->DestroyShader(vertexShader_);
  device->DestroyBuffer(vertexBuffer_);

  pipeline_ = {};
  fragmentShader_ = {};
  vertexShader_ = {};
  vertexBuffer_ = {};
  vertexCount_ = 0;
  device_ = nullptr;
}
} // namespace Rodan
