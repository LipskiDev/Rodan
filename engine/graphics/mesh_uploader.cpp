#include "graphics/mesh_uploader.h"
#include "rhi/rhi_upload_context.h"

#include <stdexcept>
#include <vector>

namespace Rodan {

std::shared_ptr<MeshResource> MeshUploader::Upload(IDevice *device,
                                                   IUploadContext *upload,
                                                   const ImportedMesh &mesh) {
  if (!device) {
    throw std::runtime_error("MeshUploader::Upload: device is null");
  }

  if (!upload) {
    throw std::runtime_error("MeshUploader::Upload: upload is null");
  }

  std::vector<ImportedVertex> combinedVertices;
  std::vector<uint32_t> combinedIndices;
  std::vector<Submesh> submeshes;

  for (const ImportedPrimitive &primitive : mesh.primitives) {
    const uint32_t baseVertex = static_cast<uint32_t>(combinedVertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(combinedIndices.size());

    combinedVertices.insert(combinedVertices.end(), primitive.vertices.begin(),
                            primitive.vertices.end());

    combinedIndices.reserve(combinedIndices.size() + primitive.indices.size());
    for (uint32_t index : primitive.indices) {
      combinedIndices.push_back(baseVertex + index);
    }

    Submesh submesh{};
    submesh.firstIndex = firstIndex;
    submesh.indexCount = static_cast<uint32_t>(primitive.indices.size());
    submesh.materialSlot = primitive.materialIndex >= 0
                               ? static_cast<uint32_t>(primitive.materialIndex)
                               : 0;
    submesh.hasTangents = primitive.hasTangents;

    submeshes.push_back(submesh);
  }

  auto resource = std::make_shared<MeshResource>();
  resource->submeshes = std::move(submeshes);

  const uint64_t vertexBufferSize =
      static_cast<uint64_t>(combinedVertices.size() * sizeof(ImportedVertex));
  const uint64_t indexBufferSize =
      static_cast<uint64_t>(combinedIndices.size() * sizeof(uint32_t));

  if (vertexBufferSize == 0) {
    throw std::runtime_error(
        "MeshUploader::Upload: mesh produced an empty vertex buffer");
  }

  if (indexBufferSize == 0) {
    throw std::runtime_error(
        "MeshUploader::Upload: mesh produced an empty index buffer");
  }

  resource->vertexBuffer = device->CreateBuffer({
      .size = vertexBufferSize,
      .usage = BufferUsage::Vertex | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::GPUOnly,
      .initialData = nullptr,
      .debugName = "Mesh Vertex Buffer",
  });

  resource->indexBuffer = device->CreateBuffer({
      .size = indexBufferSize,
      .usage = BufferUsage::Index | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::GPUOnly,
      .initialData = nullptr,
      .debugName = "Mesh Index Buffer",
  });

  upload->UploadBuffer({
      .dstBuffer = resource->vertexBuffer,
      .dstOffset = 0,
      .size = vertexBufferSize,
      .data = combinedVertices.data(),
  });

  upload->UploadBuffer({
      .dstBuffer = resource->indexBuffer,
      .dstOffset = 0,
      .size = indexBufferSize,
      .data = combinedIndices.data(),
  });

  resource->aabb = mesh.localBounds;
  resource->vertexCount = vertexBufferSize;
  resource->indexCount = indexBufferSize;

  return resource;
}
} // namespace Rodan
