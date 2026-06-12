#include "render_graph_builder.h"
#include "rhi/rhi_types.h"

namespace Rodan {

void RenderGraphBuilder::Read(const std::string &resource,
                              Velos::RHI::ImageLayout layout,
                              SubresourceRange range) {
  accesses_.push_back({.name = resource,
                       .type = AccessType::Read,
                       .requiredLayout = layout,
                       .range = range});
}

void RenderGraphBuilder::Write(const std::string &resource,
                               Velos::RHI::ImageLayout layout,
                               SubresourceRange range) {
  accesses_.push_back({.name = resource,
                       .type = AccessType::Write,
                       .requiredLayout = layout,
                       .range = range});
}

} // namespace Rodan
