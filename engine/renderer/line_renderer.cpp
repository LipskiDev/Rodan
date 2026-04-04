#include "core/input_types.h"
#include "imgui.h"
#include "rhi/rhi_pipeline.h"
#include "rhi/rhi_types.h"
#include "shader/shader_compiler.h"
#include <renderer/line_renderer.h>

namespace Rodan::Debug {

LineRenderer3D::LineRenderer3D(Velos::RHI::IDevice *device) {
  device_ = device;
  Velos::ShaderCompileOutput compiledVertexShader =
      Velos::ShaderCompiler::CompileFile({
          .path = "assets/shaders/line_renderer_3d.vert",
          .stage = Velos::RHI::ShaderStage::Vertex,
          .entryPoint = "main",
          .language = Velos::ShaderSourceLanguage::GLSL,
      });

  Velos::ShaderCompileOutput compiledFragmentShader =
      Velos::ShaderCompiler::CompileFile({
          .path = "assets/shaders/line_renderer_3d.frag",
          .stage = Velos::RHI::ShaderStage::Fragment,
          .entryPoint = "main",
          .language = Velos::ShaderSourceLanguage::GLSL,
      });

  vertexShader_ = device_->CreateShader({
      .stage = Velos::RHI::ShaderStage::Vertex,
      .bytecode = compiledVertexShader.spirv.data(),
      .bytecodeSize = compiledVertexShader.spirv.size() * sizeof(uint32_t),
      .entryPoint = "main",
      .reflection = compiledVertexShader.reflection,
      .debugName = "LineRenderer3D Vertex Shader",
  });

  fragmentShader_ = device_->CreateShader({
      .stage = Velos::RHI::ShaderStage::Fragment,
      .bytecode = compiledFragmentShader.spirv.data(),
      .bytecodeSize = compiledFragmentShader.spirv.size() * sizeof(uint32_t),
      .entryPoint = "main",
      .reflection = compiledFragmentShader.reflection,
      .debugName = "LineRenderer3D Fragment Shader",
  });

  pipeline_ = device_->CreateGraphicsPipeline({
      .vertexShader = vertexShader_,
      .fragmentShader = fragmentShader_,
      .topology = Velos::RHI::PrimitiveTopology::LineList,
      .raster =
          {
              .cullBackFaces = false,
              .frontFaceCCW = true,
              .wireframe = false,
          },
      .depth = {.depthTestEnable = true,
                .depthWriteEnable = false,
                .depthFormat = Velos::RHI::Format::D32_FLOAT},
      .blend =
          {
              .enable = false,
          },
      .colorFormat = Velos::RHI::Format::BGRA8_UNORM,
      .debugName = "LineRenderer3D Pipeline",
  });
}

LineRenderer3D::~LineRenderer3D() {
  device_->DestroyBuffer(vertexBuffer_);
  device_->DestroyPipeline(pipeline_);
  device_->DestroyShader(vertexShader_);
  device_->DestroyShader(fragmentShader_);
}

void LineRenderer3D::clear() { lines_.clear(); }

void LineRenderer3D::line(const vec3 &p1, const vec3 &p2, const vec4 &color) {
  lines_.push_back({.pos = vec4(p1, 1.0f), .color = color});
  lines_.push_back({.pos = vec4(p2, 1.0f), .color = color});
}

void LineRenderer3D::plane(const vec3 &orig, const vec3 &v1, const vec3 &v2,
                           int n1, int n2, float s1, float s2,
                           const vec4 &color, const vec4 &outlineColor) {
  if (n1 <= 0 || n2 <= 0) {
    return;
  }

  const vec3 axis1 = glm::normalize(v1);
  const vec3 axis2 = glm::normalize(v2);

  const float half1 = 0.5f * s1;
  const float half2 = 0.5f * s2;

  for (int i = 0; i <= n1; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n1);
    const float offset = glm::mix(-half1, half1, t);

    const vec3 base = orig + axis1 * offset;
    const vec3 p0 = base - axis2 * half2;
    const vec3 p1 = base + axis2 * half2;

    const bool isOutline = (i == 0 || i == n1);
    line(p0, p1, isOutline ? outlineColor : color);
  }

  for (int j = 0; j <= n2; ++j) {
    const float t = static_cast<float>(j) / static_cast<float>(n2);
    const float offset = glm::mix(-half2, half2, t);

    const vec3 base = orig + axis2 * offset;
    const vec3 p0 = base - axis1 * half1;
    const vec3 p1 = base + axis1 * half1;

    const bool isOutline = (j == 0 || j == n2);
    line(p0, p1, isOutline ? outlineColor : color);
  }
}

void LineRenderer3D::box(const mat4 &m, const vec3 &size, const vec4 &color) {
  const vec3 h = size * 0.5f;

  const vec3 localCorners[8] = {
      {-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z},
      {-h.x, h.y, -h.z},  {-h.x, -h.y, h.z}, {h.x, -h.y, h.z},
      {h.x, h.y, h.z},    {-h.x, h.y, h.z},
  };

  vec3 c[8];
  for (int i = 0; i < 8; ++i) {
    c[i] = vec3(m * vec4(localCorners[i], 1.0f));
  }

  // Bottom face
  line(c[0], c[1], color);
  line(c[1], c[2], color);
  line(c[2], c[3], color);
  line(c[3], c[0], color);

  // Top face
  line(c[4], c[5], color);
  line(c[5], c[6], color);
  line(c[6], c[7], color);
  line(c[7], c[4], color);

  // Vertical edges
  line(c[0], c[4], color);
  line(c[1], c[5], color);
  line(c[2], c[6], color);
  line(c[3], c[7], color);
}

void LineRenderer3D::frustum(const mat4 &camView, const mat4 &camProj,
                             const vec4 &color) {
  const mat4 invViewProj = glm::inverse(camProj * camView);

  auto unproject = [&](float x, float y, float z) -> vec3 {
    vec4 p = invViewProj * vec4(x, y, z, 1.0f);
    return vec3(p) / p.w;
  };

  // NDC corners
  // OpenGL-style NDC: z in [-1, 1]
  const vec3 nbl = unproject(-1.0f, -1.0f, -1.0f);
  const vec3 nbr = unproject(1.0f, -1.0f, -1.0f);
  const vec3 ntr = unproject(1.0f, 1.0f, -1.0f);
  const vec3 ntl = unproject(-1.0f, 1.0f, -1.0f);

  const vec3 fbl = unproject(-1.0f, -1.0f, 1.0f);
  const vec3 fbr = unproject(1.0f, -1.0f, 1.0f);
  const vec3 ftr = unproject(1.0f, 1.0f, 1.0f);
  const vec3 ftl = unproject(-1.0f, 1.0f, 1.0f);

  // Near plane
  line(nbl, nbr, color);
  line(nbr, ntr, color);
  line(ntr, ntl, color);
  line(ntl, nbl, color);

  // Far plane
  line(fbl, fbr, color);
  line(fbr, ftr, color);
  line(ftr, ftl, color);
  line(ftl, fbl, color);

  // Side edges
  line(nbl, fbl, color);
  line(nbr, fbr, color);
  line(ntr, ftr, color);
  line(ntl, ftl, color);
}

void LineRenderer3D::render(Velos::RHI::ICommandList &cmd,
                            const mat4 &viewProj) {
  if (lines_.empty()) {
    return;
  }

  const Velos::u64 requiredSize =
      static_cast<Velos::u64>(lines_.size() * sizeof(LineData));

  if (!vertexBuffer_.IsValid() || vertexBufferSize_ < requiredSize) {
    if (vertexBuffer_.IsValid()) {
      device_->WaitIdle(); // temporary crude fix
      device_->DestroyBuffer(vertexBuffer_);
    }

    vertexBuffer_ = device_->CreateBuffer({
        .size = requiredSize,
        .usage = Velos::RHI::BufferUsage::Storage |
                 Velos::RHI::BufferUsage::ShaderDeviceAddress,
        .memoryUsage = Velos::RHI::MemoryUsage::CPUToGPU,
        .initialData = lines_.data(),
        .debugName = "LineRenderer3D Vertex Pulling Buffer",
    });

    vertexBufferSize_ = requiredSize;
  } else {
    // better later: Map/UpdateBuffer path
    device_->WaitIdle(); // temporary only if you have no update API
    device_->DestroyBuffer(vertexBuffer_);

    vertexBuffer_ = device_->CreateBuffer({
        .size = requiredSize,
        .usage = Velos::RHI::BufferUsage::Storage |
                 Velos::RHI::BufferUsage::ShaderDeviceAddress,
        .memoryUsage = Velos::RHI::MemoryUsage::CPUToGPU,
        .initialData = lines_.data(),
        .debugName = "LineRenderer3D Vertex Pulling Buffer",
    });
  }

  struct PushConstants {
    mat4 mvp;
    uint64_t addr;
  } pc{
      .mvp = viewProj,
      .addr = device_->GetBufferDeviceAddress(vertexBuffer_),
  };

  cmd.BindPipeline(pipeline_);
  cmd.PushConstants(Velos::RHI::ShaderStage::Vertex, 0, sizeof(pc), &pc);
  cmd.Draw(static_cast<Velos::u32>(lines_.size()));
}

void LineRenderer2D::render(const char *nameImGuiWindow) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
  ImGui::Begin(
      nameImGuiWindow, nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

  ImDrawList *drawList = ImGui::GetBackgroundDrawList();

  for (const LineData &l : lines_) {
    drawList->AddLine(ImVec2(l.p1.x, l.p1.y), ImVec2(l.p2.x, l.p2.y),
                      ImColor(l.color.r, l.color.g, l.color.b, l.color.a));
  }

  ImGui::End();
}
} // namespace Rodan::Debug
