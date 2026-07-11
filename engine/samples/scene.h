#pragma once

#include "core/frame_render_context.h"
#include "core/input_system.h"
#include "core/types.h"
#include "rhi/command_list.h"
#include "rhi/device.h"
#include "rhi/handles.h"
#include "rhi/types.h"
#include <glm/glm.hpp>

namespace Rodan {

enum class SceneType {
  GltfViewer,
  ComputeTest,
};

struct SceneUpdateContext {
  InputSystem *input = nullptr;
  Velos::u32 framebufferWidth = 0;
  Velos::u32 framebufferHeight = 0;
};

class IScene {
public:
  virtual ~IScene() = default;

  virtual void Initialize(Velos::RHI::IDevice *device,
                          Velos::RHI::SwapchainHandle swapchain,
                          Velos::RHI::Format colorFormat,
                          Velos::RHI::Format depthFormat) = 0;

  virtual void Shutdown(Velos::RHI::IDevice *device) = 0;

  virtual void OnResize(Velos::RHI::IDevice *device, Velos::u32 width,
                        Velos::u32 height) = 0;

  virtual void Update(float deltaSeconds, const SceneUpdateContext &ctx) = 0;

  virtual void Prepare(Velos::RHI::ICommandList &cmd) = 0;
  virtual void Render(Velos::RHI::ICommandList &cmd,
                      const FrameRenderContext &frame) = 0;

  virtual void RenderImGui() = 0;
};

} // namespace Rodan
