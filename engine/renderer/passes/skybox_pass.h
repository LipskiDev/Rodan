// renderer/passes/skybox_pass.h
#pragma once

#include "rhi/command_list.h"
#include "rhi/device.h"
#include "rhi/types.h"

#include <glm/glm.hpp>

namespace Rodan {

class EnvironmentMap;

class SkyboxPass {
public:
  void Initialize(Velos::RHI::IDevice *device, Velos::RHI::Format colorFormat,
                  Velos::RHI::BindingLayoutHandle environmentSetLayout);

  void Render(Velos::RHI::ICommandList &cmd, const EnvironmentMap &environment,
              const glm::mat4 &view, const glm::mat4 &proj);

  void Shutdown(Velos::RHI::IDevice *device);

private:
  Velos::RHI::IDevice *device_ = nullptr;

  Velos::RHI::BufferHandle vertexBuffer_{};
  Velos::RHI::ShaderHandle vertexShader_{};
  Velos::RHI::ShaderHandle fragmentShader_{};
  Velos::RHI::PipelineHandle pipeline_{};

  Velos::u32 vertexCount_ = 0;
};

} // namespace Rodan
