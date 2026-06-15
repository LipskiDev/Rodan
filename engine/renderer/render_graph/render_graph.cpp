#include "render_graph.h"
#include "renderer/render_graph/render_graph_builder.h"
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

  if (frameCounter % 600 == 0) {
    PrintDebugInfo();
  }
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

  for (const auto &[name, image] : importedImages_) {
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
      auto importedIt = importedImages_.find(access.name);
      if (importedIt == importedImages_.end()) {
        continue;
      }

      const ImportedImage &image = importedIt->second;

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
    auto it = importedImages_.find(transition.resource);
    if (it == importedImages_.end()) {
      continue;
    }

    ImportedImage &resource = it->second;
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

    if (oldLayout == transition.newLayout &&
        oldState == transition.newState) {
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
  ImportedImage &imported = importedImages_[name];
  if (imported.image.IsValid() && !imported.mipLayouts.empty()) {
    imported.layoutsByImage[imported.image.id] = imported.mipLayouts;
  }

  if (imported.image.IsValid() && !imported.mipStates.empty()) {
    imported.statesByImage[imported.image.id] = imported.mipStates;
  }

  const bool isNewImage = !imported.image.IsValid() ||
                          imported.image.id != image.id;
  const bool subresourceShapeChanged = imported.mipLevels != mipLevels ||
                                       imported.arrayLayers != arrayLayers;

  imported.image = image;
  imported.aspect = aspect;
  imported.mipLevels = mipLevels;
  imported.arrayLayers = arrayLayers;

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

void RenderGraph::Reset() {
  passes_.clear();
  edges_.clear();
  executionOrder_.clear();
  lifetimes_.clear();
  transitions_.clear();
}

} // namespace Rodan
