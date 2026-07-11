#pragma once

#include "graphics/environment/environment_map.h"
#include "graphics/environment/ibl_resources.h"

#include "rhi/command_list.h"
#include "rhi/device.h"
#include "rhi/handles.h"
#include "rhi/pipeline.h"
#include "rhi/types.h"

#include <cstdint>

namespace Rodan {

class IBLBaker {
public:
  void Initialize(Velos::RHI::IDevice *device,
                  Velos::RHI::BindingLayoutHandle environmentLayout);

  IBLResources BakeIrradiance(Velos::RHI::ICommandList &cmd,
                              const EnvironmentMap &environment);

  void Shutdown(Velos::RHI::IDevice *device);

private:
  void CreateIrradianceResources(IBLResources &resources);
  void RenderIrradiance(Velos::RHI::ICommandList &cmd,
                        const EnvironmentMap &environment,
                        IBLResources &resources);
  void RenderPrefilter(Velos::RHI::ICommandList &cmd,
                       const EnvironmentMap &environment, IBLResources &result);
  void RenderBRDFLut(Velos::RHI::ICommandList &cmd,
                     const EnvironmentMap &environment, IBLResources &result);
  void CreateIBLBindingSet(IBLResources &resources);

private:
  Velos::RHI::IDevice *device_ = nullptr;

  Velos::RHI::PipelineHandle irradiancePipeline_{};
  Velos::RHI::PipelineHandle prefilterPipeline_{};
  Velos::RHI::PipelineHandle brdfLutPipeline_{};

  Velos::RHI::ShaderHandle irradianceVS_{};
  Velos::RHI::ShaderHandle irradianceFS_{};

  Velos::RHI::ShaderHandle prefilterVS_{};
  Velos::RHI::ShaderHandle prefilterFS_{};

  Velos::RHI::ShaderHandle brdfLutCS_{};
  Velos::RHI::BindingLayoutHandle brdfLutSetLayout_;
  BindingPoolHandle brdfBakeBindingPool_;
  BindingSetHandle brdfBakeBindingSet_;

  Velos::RHI::BufferHandle cubeVertexBuffer_{};

  std::uint32_t cubeVertexCount_ = 0;
};

} // namespace Rodan
