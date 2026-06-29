#include "render_graph.h"
#include "renderer/render_graph/render_graph_builder.h"
#include "rhi/rhi_resources.h"
#include "rhi/rhi_types.h"

#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>

namespace Rodan {

static const char *ToString(Velos::RHI::ImageLayout layout) {
  switch (layout) {
  case Velos::RHI::ImageLayout::Undefined:
    return "Undefined";
  case Velos::RHI::ImageLayout::ColorAttachment:
    return "ColorAttachment";
  case Velos::RHI::ImageLayout::DepthAttachment:
    return "DepthAttachment";
  case Velos::RHI::ImageLayout::ShaderReadOnly:
    return "ShaderReadOnly";
  case Velos::RHI::ImageLayout::TransferSrc:
    return "TransferSrc";
  case Velos::RHI::ImageLayout::TransferDst:
    return "TransferDst";
  case Velos::RHI::ImageLayout::General:
    return "General";
  default:
    return "Unknown";
  }
}

void RenderGraph::AddPass(const std::string &name, SetupCallback setup,
                          ExecuteCallback execute) {
  RenderGraphBuilder builder;
  setup(builder);

  Pass pass{};
  pass.name = name;
  pass.setup = std::move(setup);
  pass.execute = std::move(execute);
  pass.accesses = std::move(builder.accesses_);

  passes_.push_back(std::move(pass));
}

void RenderGraph::Compile() {
  ResetCompiledData();

  BuildDependencies();
  BuildExecutionOrder();
  BuildResourceLifetimes();
  BuildTransitions();

  static uint32_t frameCounter = 0;
  frameCounter++;
}

void RenderGraph::ResetCompiledData() {
  edges_.clear();
  executionOrder_.clear();
  lifetimes_.clear();
  transitions_.clear();

  for (Pass &pass : passes_) {
    pass.requiredTransitions.clear();
  }
}

void RenderGraph::BuildDependencies() {
  std::unordered_map<std::string, std::string> lastWriterByResource;

  for (const Pass &pass : passes_) {
    for (const ResourceAccess &access : pass.accesses) {
      if (access.type == AccessType::Read) {
        auto it = lastWriterByResource.find(access.name);

        if (it != lastWriterByResource.end()) {
          edges_.push_back({
              .producerPass = it->second,
              .consumerPass = pass.name,
              .resource = access.name,
          });
        }
      }

      if (access.type == AccessType::Write) {
        lastWriterByResource[access.name] = pass.name;
      }
    }
  }
}

void RenderGraph::BuildExecutionOrder() {
  std::unordered_map<std::string, std::vector<std::string>> adjacency;
  std::unordered_map<std::string, uint32_t> indegree;

  for (const Pass &pass : passes_) {
    indegree.try_emplace(pass.name, 0);
    adjacency.try_emplace(pass.name, std::vector<std::string>{});
  }

  for (const CompiledEdge &edge : edges_) {
    adjacency[edge.producerPass].push_back(edge.consumerPass);
    indegree[edge.consumerPass]++;
  }

  std::queue<std::string> queue;

  for (const Pass &pass : passes_) {
    if (indegree[pass.name] == 0) {
      queue.push(pass.name);
    }
  }

  while (!queue.empty()) {
    std::string current = queue.front();
    queue.pop();

    executionOrder_.push_back(current);

    for (const std::string &next : adjacency[current]) {
      indegree[next]--;

      if (indegree[next] == 0) {
        queue.push(next);
      }
    }
  }

  if (executionOrder_.size() != passes_.size()) {
    std::cerr << "RenderGraph::BuildExecutionOrder: cycle detected in render "
                 "graph\n";
  }
}

void RenderGraph::BuildResourceLifetimes() {
  std::unordered_map<std::string, ResourceLifetime> lifetimeByResource;

  for (const std::string &passName : executionOrder_) {
    const Pass *pass = FindPass(passName);

    if (!pass) {
      continue;
    }

    for (const ResourceAccess &access : pass->accesses) {
      auto it = lifetimeByResource.find(access.name);

      if (it == lifetimeByResource.end()) {
        lifetimeByResource.emplace(access.name, ResourceLifetime{
                                                    .name = access.name,
                                                    .firstUser = passName,
                                                    .lastUser = passName,
                                                });
      } else {
        it->second.lastUser = passName;
      }
    }
  }

  for (auto &[_, lifetime] : lifetimeByResource) {
    lifetimes_.push_back(std::move(lifetime));
  }
}

void RenderGraph::BuildTransitions() {
  std::unordered_map<std::string, std::vector<Velos::RHI::ImageLayout>>
      currentLayouts;
  std::unordered_map<std::string, std::vector<Velos::RHI::ResourceState>>
      currentStates;

  std::unordered_map<std::string, std::vector<std::string>>
      lastPassUsingResource;

  for (const auto &[name, image] : imageResources_) {
    currentLayouts[name] = image.mipLayouts;
    currentStates[name] = image.mipStates;
    lastPassUsingResource[name].assign(image.mipLevels, "External");
  }

  for (const std::string &passName : executionOrder_) {
    const Pass *currentPass = FindPass(passName);
    if (!currentPass) {
      continue;
    }

    for (const ResourceAccess &access : currentPass->accesses) {
      auto importedIt = imageResources_.find(access.name);
      if (importedIt == imageResources_.end()) {
        continue;
      }

      const ImageResource &image = importedIt->second;

      const uint32_t baseMip = access.range.baseMip;
      const uint32_t mipCount = access.range.mipCount == UINT32_MAX
                                    ? image.mipLevels - baseMip
                                    : access.range.mipCount;

      if (baseMip >= image.mipLevels) {
        continue;
      }

      const uint32_t endMip = std::min(baseMip + mipCount, image.mipLevels);

      for (uint32_t mip = baseMip; mip < endMip; ++mip) {
        const Velos::RHI::ImageLayout oldLayout =
            currentLayouts[access.name][mip];
        const Velos::RHI::ImageLayout newLayout = access.requiredLayout;
        const Velos::RHI::ResourceState oldState =
            currentStates[access.name][mip];
        const Velos::RHI::ResourceState newState = access.requiredState;

        if (oldLayout != newLayout || oldState != newState) {
          CompiledTransition transition{
              .resource = access.name,
              .fromPass = lastPassUsingResource[access.name][mip],
              .toPass = passName,
              .newLayout = newLayout,
              .oldState = oldState,
              .newState = newState,
              .range =
                  {
                      .baseMip = mip,
                      .mipCount = 1,
                      .baseLayer = access.range.baseLayer,
                      .layerCount = access.range.layerCount,
                  },
          };

          transitions_.push_back(transition);

          Pass *targetPass = FindPass(passName);
          if (targetPass) {
            targetPass->requiredTransitions.push_back(transition);
          }

          currentLayouts[access.name][mip] = newLayout;
          currentStates[access.name][mip] = newState;
        }

        lastPassUsingResource[access.name][mip] = passName;
      }
    }
  }
}

void RenderGraph::PrintDebugInfo() const {
  std::cout << "\n====================\n";
  std::cout << "Render Graph Dependencies\n";
  std::cout << "====================\n";

  for (const CompiledEdge &edge : edges_) {
    std::cout << edge.producerPass << " -> " << edge.consumerPass << " : "
              << edge.resource << "\n";
  }

  std::cout << "\n====================\n";
  std::cout << "Render Graph Topological Order\n";
  std::cout << "====================\n";

  for (size_t i = 0; i < executionOrder_.size(); ++i) {
    std::cout << i << ": " << executionOrder_[i] << "\n";
  }

  std::cout << "\n====================\n";
  std::cout << "Render Graph Resource Lifetimes\n";
  std::cout << "====================\n";

  for (const ResourceLifetime &lifetime : lifetimes_) {
    std::cout << lifetime.name << " : " << lifetime.firstUser << " -> "
              << lifetime.lastUser << "\n";
  }

  std::cout << "\n====================\n";
  std::cout << "Render Graph Layout Transitions\n";
  std::cout << "====================\n";

  for (const CompiledTransition &transition : transitions_) {
    std::cout << transition.resource << " mip=" << transition.range.baseMip
              << " layers=" << transition.range.baseLayer << "..";

    if (transition.range.layerCount == UINT32_MAX) {
      std::cout << "remaining";
    } else {
      std::cout << transition.range.baseLayer + transition.range.layerCount - 1;
    }

    std::cout << " : " << transition.fromPass << " -> " << transition.toPass
              << " : -> " << ToString(transition.newLayout) << "\n";
  }

  std::cout << "\n====================\n";
  std::cout << "Render Graph Per-Pass Transitions\n";
  std::cout << "====================\n";

  for (const std::string &passName : executionOrder_) {
    const Pass *pass = FindPass(passName);

    if (!pass || pass->requiredTransitions.empty()) {
      continue;
    }

    std::cout << pass->name << "\n";

    for (const CompiledTransition &transition : pass->requiredTransitions) {
      std::cout << "  " << transition.resource
                << " mip=" << transition.range.baseMip << " : -> "
                << ToString(transition.newLayout) << "\n";
    }
  }

  std::cout << "\n";
}

void RenderGraph::Execute(ICommandList &cmd) {
  for (const std::string &passName : executionOrder_) {
    Pass *pass = FindPass(passName);

    if (!pass) {
      continue;
    }

    EmitTransitions(cmd, pass->requiredTransitions);

    pass->execute(cmd);
  }
}

RenderGraph::Pass *RenderGraph::FindPass(const std::string &name) {
  for (Pass &pass : passes_) {
    if (pass.name == name) {
      return &pass;
    }
  }

  return nullptr;
}

const RenderGraph::Pass *RenderGraph::FindPass(const std::string &name) const {
  for (const Pass &pass : passes_) {
    if (pass.name == name) {
      return &pass;
    }
  }

  return nullptr;
}

void RenderGraph::EmitTransitions(
    ICommandList &cmd, const std::vector<CompiledTransition> &transitions) {
  for (const CompiledTransition &transition : transitions) {
    auto it = imageResources_.find(transition.resource);
    if (it == imageResources_.end()) {
      continue;
    }

    ImageResource &resource = it->second;
    if (!resource.image.IsValid()) {
      continue;
    }

    const uint32_t baseMip = transition.range.baseMip;

    if (baseMip >= resource.mipLayouts.size()) {
      continue;
    }

    uint32_t mipCount = transition.range.mipCount;
    if (mipCount == UINT32_MAX) {
      mipCount = static_cast<uint32_t>(resource.mipLayouts.size()) - baseMip;
    }

    const uint32_t endMip = std::min<uint32_t>(
        baseMip + mipCount, static_cast<uint32_t>(resource.mipLayouts.size()));

    const Velos::RHI::ImageLayout oldLayout = resource.mipLayouts[baseMip];
    const Velos::RHI::ResourceState oldState = resource.mipStates[baseMip];

    if (oldLayout == transition.newLayout && oldState == transition.newState) {
      continue;
    }

    const uint32_t layerCount = transition.range.layerCount == UINT32_MAX
                                    ? resource.arrayLayers
                                    : transition.range.layerCount;

    cmd.Barrier({
        .image = resource.image,
        .oldLayout = oldLayout,
        .newLayout = transition.newLayout,
        .oldState = oldState,
        .newState = transition.newState,
        .useExplicitStates = true,
        .aspect = resource.aspect,
        .baseMipLevel = baseMip,
        .mipLevelCount = mipCount,
        .baseArrayLayer = transition.range.baseLayer,
        .layerCount = layerCount,
    });

    for (uint32_t mip = baseMip; mip < endMip; ++mip) {
      resource.mipLayouts[mip] = transition.newLayout;
      resource.mipStates[mip] = transition.newState;
    }
  }
}

void RenderGraph::ImportImage(const std::string &name,
                              Velos::RHI::ImageHandle image,
                              Velos::RHI::ImageAspect aspect,
                              uint32_t mipLevels, uint32_t arrayLayers,
                              Velos::RHI::ImageLayout currentLayout,
                              Velos::RHI::ResourceState currentState,
                              bool hasCurrentState) {
  ImageResource &imported = imageResources_[name];
  if (imported.image.IsValid() && !imported.mipLayouts.empty()) {
    imported.layoutsByImage[imported.image.id] = imported.mipLayouts;
  }

  if (imported.image.IsValid() && !imported.mipStates.empty()) {
    imported.statesByImage[imported.image.id] = imported.mipStates;
  }

  const bool isNewImage =
      !imported.image.IsValid() || imported.image.id != image.id;
  const bool subresourceShapeChanged =
      imported.mipLevels != mipLevels || imported.arrayLayers != arrayLayers;

  imported.image = image;
  imported.aspect = aspect;
  imported.mipLevels = mipLevels;
  imported.arrayLayers = arrayLayers;
  imported.imported = true;

  auto layoutIt = imported.layoutsByImage.find(image.id);
  if (hasCurrentState) {
    imported.mipLayouts.assign(mipLevels, currentLayout);
  } else if (!subresourceShapeChanged &&
             layoutIt != imported.layoutsByImage.end() &&
             layoutIt->second.size() == mipLevels) {
    imported.mipLayouts = layoutIt->second;
  } else if (!isNewImage && imported.mipLayouts.size() == mipLevels) {
    // Keep tracking state for stable imported images across frames.
  } else {
    imported.mipLayouts.assign(mipLevels, Velos::RHI::ImageLayout::Undefined);
  }

  auto stateIt = imported.statesByImage.find(image.id);
  if (hasCurrentState) {
    imported.mipStates.assign(mipLevels, currentState);
  } else if (!subresourceShapeChanged &&
             stateIt != imported.statesByImage.end() &&
             stateIt->second.size() == mipLevels) {
    imported.mipStates = stateIt->second;
  } else if (!isNewImage && imported.mipStates.size() == mipLevels) {
    // Keep tracking state for stable imported images across frames.
  } else {
    imported.mipStates.assign(mipLevels, Velos::RHI::ResourceState::Undefined);
  }
}

bool RenderGraph::RegisterImage(Velos::RHI::IDevice &device,
                                const std::string &name, TextureDesc desc) {
  ImageResource &registeredImage = imageResources_[name];

  const bool isNewImage =
      !registeredImage.image.IsValid() ||
      registeredImage.desc.width != desc.width ||
      registeredImage.desc.height != desc.height ||
      registeredImage.desc.format != desc.format ||
      registeredImage.desc.mipLevels != desc.mipLevels ||
      registeredImage.desc.arrayLayers != desc.arrayLayers ||
      registeredImage.desc.usage != desc.usage;

  if (!isNewImage) {
    return false;
  }

  if (registeredImage.renderView.IsValid()) {
    device.DestroyImageView(registeredImage.renderView);
    registeredImage.renderView = {};
  }

  if (registeredImage.view.IsValid()) {
    device.DestroyImageView(registeredImage.view);
    registeredImage.view = {};
  }

  if (registeredImage.sampler.IsValid()) {
    device.DestroySampler(registeredImage.sampler);
    registeredImage.sampler = {};
  }

  if (registeredImage.image.IsValid()) {
    device.DestroyImage(registeredImage.image);
    registeredImage.image = {};
  }

  registeredImage.desc = desc;

  Velos::RHI::ImageDesc imageDesc{};
  imageDesc.debugName = desc.debugName;
  imageDesc.width = desc.width;
  imageDesc.height = desc.height;
  imageDesc.depth = 1;
  imageDesc.mipLevels = desc.mipLevels;
  imageDesc.arrayLayers = desc.arrayLayers;
  imageDesc.format = desc.format;
  imageDesc.usage = desc.usage;

  registeredImage.image = device.CreateImage(imageDesc);

  // Full sampled view
  registeredImage.view = device.CreateImageView({
      .image = registeredImage.image,
      .format = desc.format,
      .type = desc.viewType,
      .aspect = desc.aspect,
      .baseMipLevel = 0,
      .mipLevelCount = desc.mipLevels,
      .baseArrayLayer = 0,
      .arrayLayerCount = desc.arrayLayers,
      .debugName = desc.debugName,
  });

  // Attachment view: mip 0 only
  registeredImage.renderView = device.CreateImageView({
      .image = registeredImage.image,
      .format = desc.format,
      .type = desc.viewType,
      .aspect = desc.aspect,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = desc.arrayLayers,
      .debugName = desc.debugName,
  });

  Velos::RHI::SamplerDesc samplerDesc{};
  samplerDesc.debugName = desc.debugName;
  samplerDesc.addressU = desc.addressU;
  samplerDesc.addressV = desc.addressV;
  samplerDesc.addressW = desc.addressW;
  samplerDesc.minFilter = desc.minFilter;
  samplerDesc.magFilter = desc.magFilter;
  samplerDesc.enableAnisotropy = desc.enableAnisotropy;
  samplerDesc.maxAnisotropy = desc.maxAnisotropy;

  registeredImage.sampler = device.CreateSampler(samplerDesc);

  registeredImage.mipLayouts.assign(desc.mipLevels,
                                    Velos::RHI::ImageLayout::Undefined);

  registeredImage.mipStates.assign(desc.mipLevels,
                                   Velos::RHI::ResourceState::Undefined);

  registeredImage.aspect = desc.aspect;
  registeredImage.mipLevels = desc.mipLevels;
  registeredImage.arrayLayers = desc.arrayLayers;

  registeredImage.imported = false;

  return true;
}

const ImageResource &RenderGraph::GetImage(const std::string &name) const {
  return imageResources_.at(name);
}

void RenderGraph::Reset() {
  passes_.clear();
  edges_.clear();
  executionOrder_.clear();
  lifetimes_.clear();
  transitions_.clear();
}

void RenderGraph::Shutdown(Velos::RHI::IDevice &device) {
  for (auto &[name, resource] : imageResources_) {
    if (resource.imported) {
      continue;
    }

    if (resource.renderView.IsValid()) {
      device.DestroyImageView(resource.renderView);
      resource.renderView = {};
    }

    if (resource.view.IsValid()) {
      device.DestroyImageView(resource.view);
      resource.view = {};
    }

    if (resource.sampler.IsValid()) {
      device.DestroySampler(resource.sampler);
      resource.sampler = {};
    }

    if (resource.image.IsValid()) {
      device.DestroyImage(resource.image);
      resource.image = {};
    }
  }

  imageResources_.clear();
}

} // namespace Rodan
