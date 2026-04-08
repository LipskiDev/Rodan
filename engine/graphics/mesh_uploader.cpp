#include "graphics/mesh_uploader.h"

#include <stdexcept>
#include <vector>

namespace Rodan {

std::shared_ptr<MeshResource> MeshUploader::Upload(IDevice *device,
                                                   const ImportedMesh &mesh) {
  if (!device) {
    throw std::runtime_error("MeshUploader::Upload: device is null");
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

    Submesh submesh;
    submesh.firstIndex = firstIndex;
    submesh.indexCount = static_cast<uint32_t>(primitive.indices.size());
    submesh.materialSlot = primitive.materialIndex >= 0
                               ? static_cast<uint32_t>(primitive.materialIndex)
                               : 0;

    submeshes.push_back(submesh);
  }

  auto resource = std::make_shared<MeshResource>();
  resource->submeshes = std::move(submeshes);

  // Replace this with your real buffer creation/upload path.
  // Ideally: GPU-only buffer + staging upload.
  resource->vertexBuffer = device->CreateBuffer({
      .size = static_cast<uint64_t>(combinedVertices.size() *
                                    sizeof(ImportedVertex)),
      .usage = BufferUsage::Vertex | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = combinedVertices.data(),
  });

  resource->indexBuffer = device->CreateBuffer({
      .size = static_cast<uint64_t>(combinedIndices.size() * sizeof(uint32_t)),
      .usage = BufferUsage::Index | BufferUsage::TransferDst,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = combinedIndices.data(),
  });

  return resource;
}

} // namespace Rodan
