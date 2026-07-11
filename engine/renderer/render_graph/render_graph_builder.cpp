#include "render_graph_builder.h"
#include "rhi/types.h"

namespace Rodan {

namespace {

AccessType ToAccessType(RenderGraphAccess access) {
  switch (access) {
  case RenderGraphAccess::ReadColorAttachment:
  case RenderGraphAccess::ReadTexture:
  case RenderGraphAccess::CopySrc:
  case RenderGraphAccess::StorageRead:
    return AccessType::Read;

  case RenderGraphAccess::WriteColorAttachment:
  case RenderGraphAccess::WriteDepthAttachment:
  case RenderGraphAccess::CopyDst:
  case RenderGraphAccess::StorageWrite:
    return AccessType::Write;
  }

  return AccessType::Read;
}

Velos::RHI::ImageLayout ToImageLayout(RenderGraphAccess access) {
  using Velos::RHI::ImageLayout;

  switch (access) {
  case RenderGraphAccess::ReadColorAttachment:
  case RenderGraphAccess::WriteColorAttachment:
    return ImageLayout::ColorAttachment;

  case RenderGraphAccess::WriteDepthAttachment:
    return ImageLayout::DepthAttachment;

  case RenderGraphAccess::ReadTexture:
    return ImageLayout::ShaderReadOnly;

  case RenderGraphAccess::CopySrc:
    return ImageLayout::TransferSrc;

  case RenderGraphAccess::CopyDst:
    return ImageLayout::TransferDst;

  case RenderGraphAccess::StorageRead:
  case RenderGraphAccess::StorageWrite:
    return ImageLayout::General;
  }

  return ImageLayout::Undefined;
}

Velos::RHI::ResourceState ToResourceState(RenderGraphAccess access) {
  using Velos::RHI::ResourceState;

  switch (access) {
  case RenderGraphAccess::ReadColorAttachment:
    return ResourceState::ColorAttachmentRead;

  case RenderGraphAccess::WriteColorAttachment:
    return ResourceState::ColorAttachmentWrite;

  case RenderGraphAccess::WriteDepthAttachment:
    return ResourceState::DepthWrite;

  case RenderGraphAccess::ReadTexture:
  case RenderGraphAccess::StorageRead:
    return ResourceState::ShaderRead;

  case RenderGraphAccess::CopySrc:
    return ResourceState::TransferSrc;

  case RenderGraphAccess::CopyDst:
    return ResourceState::TransferDst;

  case RenderGraphAccess::StorageWrite:
    return ResourceState::ShaderWrite;
  }

  return ResourceState::Undefined;
}

} // namespace

void RenderGraphBuilder::AddAccess(const std::string &resource,
                                   RenderGraphAccess access,
                                   SubresourceRange range) {
  accesses_.push_back({.name = resource,
                       .type = ToAccessType(access),
                       .access = access,
                       .requiredLayout = ToImageLayout(access),
                       .requiredState = ToResourceState(access),
                       .range = range});
}

void RenderGraphBuilder::ReadColorAttachment(const std::string &resource,
                                             SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::ReadColorAttachment, range);
}

void RenderGraphBuilder::ReadTexture(const std::string &resource,
                                     SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::ReadTexture, range);
}

void RenderGraphBuilder::WriteColorAttachment(const std::string &resource,
                                              SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::WriteColorAttachment, range);
}

void RenderGraphBuilder::WriteDepthAttachment(const std::string &resource,
                                              SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::WriteDepthAttachment, range);
}

void RenderGraphBuilder::CopySrc(const std::string &resource,
                                 SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::CopySrc, range);
}

void RenderGraphBuilder::CopyDst(const std::string &resource,
                                 SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::CopyDst, range);
}

void RenderGraphBuilder::StorageRead(const std::string &resource,
                                     SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::StorageRead, range);
}

void RenderGraphBuilder::StorageWrite(const std::string &resource,
                                      SubresourceRange range) {
  AddAccess(resource, RenderGraphAccess::StorageWrite, range);
}

} // namespace Rodan
