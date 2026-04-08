#include "assets/gltf_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace Rodan {

namespace {

glm::mat4 GetNodeTransform(const tinygltf::Node &node) {
  glm::mat4 transform(1.0f);

  if (node.matrix.size() == 16) {
    transform = glm::make_mat4(node.matrix.data());
    return transform;
  }

  if (node.translation.size() == 3) {
    transform = glm::translate(
        transform, glm::vec3(static_cast<float>(node.translation[0]),
                             static_cast<float>(node.translation[1]),
                             static_cast<float>(node.translation[2])));
  }

  if (node.rotation.size() == 4) {
    glm::quat q(static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]));
    transform *= glm::mat4_cast(q);
  }

  if (node.scale.size() == 3) {
    transform =
        glm::scale(transform, glm::vec3(static_cast<float>(node.scale[0]),
                                        static_cast<float>(node.scale[1]),
                                        static_cast<float>(node.scale[2])));
  }

  return transform;
}

const unsigned char *GetAccessorDataPtr(const tinygltf::Model &model,
                                        const tinygltf::Accessor &accessor) {
  const tinygltf::BufferView &bufferView =
      model.bufferViews.at(accessor.bufferView);
  const tinygltf::Buffer &buffer = model.buffers.at(bufferView.buffer);

  return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
}

size_t GetAccessorStride(const tinygltf::Model &model,
                         const tinygltf::Accessor &accessor) {
  const tinygltf::BufferView &bufferView =
      model.bufferViews.at(accessor.bufferView);
  const size_t stride = accessor.ByteStride(bufferView);
  if (stride != 0) {
    return stride;
  }

  return tinygltf::GetComponentSizeInBytes(accessor.componentType) *
         tinygltf::GetNumComponentsInType(accessor.type);
}

void ReadVec3Attribute(const tinygltf::Model &model, int accessorIndex,
                       std::vector<glm::vec3> &out, size_t expectedCount) {
  if (accessorIndex < 0) {
    out.assign(expectedCount, glm::vec3(0.0f));
    return;
  }

  const tinygltf::Accessor &accessor = model.accessors.at(accessorIndex);

  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      accessor.type != TINYGLTF_TYPE_VEC3) {
    throw std::runtime_error("Unsupported VEC3 attribute format in glTF");
  }

  const unsigned char *data = GetAccessorDataPtr(model, accessor);
  const size_t stride = GetAccessorStride(model, accessor);

  out.resize(accessor.count);

  for (size_t i = 0; i < accessor.count; ++i) {
    const float *v = reinterpret_cast<const float *>(data + i * stride);
    out[i] = glm::vec3(v[0], v[1], v[2]);
  }
}

void ReadVec2Attribute(const tinygltf::Model &model, int accessorIndex,
                       std::vector<glm::vec2> &out, size_t expectedCount) {
  if (accessorIndex < 0) {
    out.assign(expectedCount, glm::vec2(0.0f));
    return;
  }

  const tinygltf::Accessor &accessor = model.accessors.at(accessorIndex);

  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      accessor.type != TINYGLTF_TYPE_VEC2) {
    throw std::runtime_error("Unsupported VEC2 attribute format in glTF");
  }

  const unsigned char *data = GetAccessorDataPtr(model, accessor);
  const size_t stride = GetAccessorStride(model, accessor);

  out.resize(accessor.count);

  for (size_t i = 0; i < accessor.count; ++i) {
    const float *v = reinterpret_cast<const float *>(data + i * stride);
    out[i] = glm::vec2(v[0], v[1]);
  }
}

std::vector<uint32_t> ReadIndices(const tinygltf::Model &model,
                                  int accessorIndex) {
  if (accessorIndex < 0) {
    throw std::runtime_error("Primitive has no indices");
  }

  const tinygltf::Accessor &accessor = model.accessors.at(accessorIndex);
  const unsigned char *data = GetAccessorDataPtr(model, accessor);
  const size_t stride = GetAccessorStride(model, accessor);

  std::vector<uint32_t> indices(accessor.count);

  for (size_t i = 0; i < accessor.count; ++i) {
    const unsigned char *element = data + i * stride;

    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      indices[i] = *reinterpret_cast<const uint8_t *>(element);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      indices[i] = *reinterpret_cast<const uint16_t *>(element);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      indices[i] = *reinterpret_cast<const uint32_t *>(element);
      break;
    default:
      throw std::runtime_error("Unsupported index type in glTF");
    }
  }

  return indices;
}

ImportedPrimitive LoadPrimitive(const tinygltf::Model &model,
                                const tinygltf::Primitive &primitive) {
  if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    throw std::runtime_error("Only triangle primitives are supported");
  }

  auto posIt = primitive.attributes.find("POSITION");
  if (posIt == primitive.attributes.end()) {
    throw std::runtime_error("Primitive missing POSITION attribute");
  }

  const int positionAccessorIndex = posIt->second;
  const tinygltf::Accessor &positionAccessor =
      model.accessors.at(positionAccessorIndex);
  const size_t vertexCount = positionAccessor.count;

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;

  ReadVec3Attribute(model, positionAccessorIndex, positions, vertexCount);

  auto normalIt = primitive.attributes.find("NORMAL");
  ReadVec3Attribute(
      model, normalIt != primitive.attributes.end() ? normalIt->second : -1,
      normals, vertexCount);

  auto uvIt = primitive.attributes.find("TEXCOORD_0");
  ReadVec2Attribute(model,
                    uvIt != primitive.attributes.end() ? uvIt->second : -1, uvs,
                    vertexCount);

  ImportedPrimitive out;
  out.vertices.resize(vertexCount);

  for (size_t i = 0; i < vertexCount; ++i) {
    out.vertices[i].position = positions[i];
    out.vertices[i].normal = normals[i];
    out.vertices[i].uv = uvs[i];
  }

  out.indices = ReadIndices(model, primitive.indices);
  out.materialIndex = primitive.material;

  return out;
}

ImportedMesh LoadMesh(const tinygltf::Model &model,
                      const tinygltf::Mesh &mesh) {
  ImportedMesh out;
  out.name = mesh.name;

  for (const tinygltf::Primitive &primitive : mesh.primitives) {
    out.primitives.push_back(LoadPrimitive(model, primitive));
  }

  return out;
}

} // namespace

ImportedScene GltfLoader::Load(const std::string &path) {
  tinygltf::TinyGLTF loader;
  tinygltf::Model model;

  std::string warn;
  std::string err;

  bool ok = false;
  if (path.ends_with(".glb")) {
    ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
  } else {
    ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);
  }

  if (!warn.empty()) {
    // replace with your logger
  }

  if (!err.empty()) {
    // replace with your logger
  }

  if (!ok) {
    throw std::runtime_error("Failed to load glTF file: " + path);
  }

  ImportedScene scene;

  scene.meshes.reserve(model.meshes.size());
  for (const tinygltf::Mesh &mesh : model.meshes) {
    scene.meshes.push_back(LoadMesh(model, mesh));
  }

  scene.nodes.reserve(model.nodes.size());
  for (const tinygltf::Node &node : model.nodes) {
    ImportedNode importedNode;
    importedNode.transform = GetNodeTransform(node);
    importedNode.meshIndex = node.mesh;
    importedNode.children = node.children;
    scene.nodes.push_back(importedNode);
  }

  int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
  if (sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size())) {
    for (int rootNode : model.scenes[sceneIndex].nodes) {
      scene.rootNodes.push_back(rootNode);
    }
  }

  return scene;
}

} // namespace Rodan
