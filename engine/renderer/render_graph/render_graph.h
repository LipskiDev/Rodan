#pragma once

#include "assets/imported_scene.h"
#include "graphics/texture.h"
#include "render_graph_builder.h"

#include <functional>
#include <string>
#include <vector>

#include "rhi/command_list.h"
#include "rhi/device.h"
#include "rhi/handles.h"
#include "rhi/types.h"

namespace Rodan {

using Velos::RHI::ICommandList;

struct ImageResource {
  TextureDesc desc;

  Velos::RHI::ImageHandle image;
  Velos::RHI::ImageViewHandle view;
  Velos::RHI::ImageViewHandle renderView;
  Velos::RHI::SamplerHandle sampler;

  Velos::RHI::ImageAspect aspect = Velos::RHI::ImageAspect::Color;
  uint32_t mipLevels = 1;
  uint32_t arrayLayers = 1;

  std::vector<Velos::RHI::ImageLayout> mipLayouts;
  std::vector<Velos::RHI::ResourceState> mipStates;

  std::unordered_map<uint32_t, std::vector<Velos::RHI::ImageLayout>>
      layoutsByImage;
  std::unordered_map<uint32_t, std::vector<Velos::RHI::ResourceState>>
      statesByImage;

  bool imported = false;
};

class RenderGraph {
public:
  using SetupCallback = std::function<void(RenderGraphBuilder &)>;
  using ExecuteCallback = std::function<void(ICommandList &)>;

  void AddPass(const std::string &name, SetupCallback setup,
               ExecuteCallback execute);

  void Compile();
  void Execute(ICommandList &cmd);

  void ImportImage(const std::string &name, Velos::RHI::ImageHandle image,
                   Velos::RHI::ImageAspect aspect, uint32_t mipLevels = 1,
                   uint32_t arrayLayers = 1,
                   Velos::RHI::ImageLayout currentLayout =
                       Velos::RHI::ImageLayout::Undefined,
                   Velos::RHI::ResourceState currentState =
                       Velos::RHI::ResourceState::Undefined,
                   bool hasCurrentState = false);

  bool RegisterImage(Velos::RHI::IDevice &device, const std::string &name,
                     TextureDesc desc);

  const ImageResource &GetImage(const std::string &name) const;

  void Reset();
  void Shutdown(IDevice &device);

private:
  struct CompiledEdge {
    std::string producerPass;
    std::string consumerPass;
    std::string resource;
  };

  struct ResourceLifetime {
    std::string name;

    std::string firstUser;
    std::string lastUser;
  };

  struct CompiledTransition {
    std::string resource;
    std::string fromPass;
    std::string toPass;
    Velos::RHI::ImageLayout newLayout;
    Velos::RHI::ResourceState oldState = Velos::RHI::ResourceState::Undefined;
    Velos::RHI::ResourceState newState = Velos::RHI::ResourceState::Undefined;

    SubresourceRange range;
  };

  struct Pass {
    std::string name;

    SetupCallback setup;
    ExecuteCallback execute;

    std::vector<ResourceAccess> accesses;

    std::vector<CompiledTransition> requiredTransitions;
  };

  std::vector<Pass> passes_;
  std::vector<CompiledEdge> edges_;

  std::vector<std::string> executionOrder_;
  std::vector<ResourceLifetime> lifetimes_;

  std::vector<CompiledTransition> transitions_;
  std::unordered_map<std::string, ImageResource> imageResources_;

private:
  Pass *FindPass(const std::string &name);
  const Pass *FindPass(const std::string &name) const;
  void EmitTransitions(ICommandList &cmd,
                       const std::vector<CompiledTransition> &transitions);
  void ResetCompiledData();

  void BuildDependencies();
  void BuildExecutionOrder();
  void BuildResourceLifetimes();
  void BuildTransitions();

  void PrintDebugInfo() const;
};

} // namespace Rodan
