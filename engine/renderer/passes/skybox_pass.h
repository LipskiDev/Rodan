// renderer/passes/skybox_pass.h
#pragma once

#include "rhi/rhi_command_list.h"
#include "rhi/rhi_device.h"
#include "rhi/rhi_types.h"

#include <glm/glm.hpp>

namespace Rodan {

class EnvironmentMap;

class SkyboxPass {
public:
  void Initialize(Velos::RHI::IDevice *device, Velos::RHI::Format colorFormat,
                  Velos::RHI::DescriptorSetLayoutHandle environmentSetLayout);

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
