#include "assets/gltf_loader.h"
#include "assets/imported_scene.h"
#include <iostream>

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

Transform GetNodeTransform(const tinygltf::Node &node) {
  Transform transform{};

  if (node.translation.size() == 3) {
    transform.position = glm::vec3(static_cast<float>(node.translation[0]),
                                   static_cast<float>(node.translation[1]),
                                   static_cast<float>(node.translation[2]));
  }

  if (node.rotation.size() == 4) {
    transform.rotation = glm::quat(static_cast<float>(node.rotation[3]),
                                   static_cast<float>(node.rotation[0]),
                                   static_cast<float>(node.rotation[1]),
                                   static_cast<float>(node.rotation[2]));
  }

  if (node.scale.size() == 3) {
    transform.scale = glm::vec3(static_cast<float>(node.scale[0]),
                                static_cast<float>(node.scale[1]),
                                static_cast<float>(node.scale[2]));
  }

  if (node.matrix.size() == 16) {
    glm::mat4 m(1.0f);

    for (int i = 0; i < 16; i++) {
      m[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
    }

    transform = Transform::FromMatrix(m);
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

void ReadVec4Attribute(const tinygltf::Model &model, int accessorIndex,
                       std::vector<glm::vec4> &out, size_t expectedCount) {
  if (accessorIndex < 0) {
    out.assign(expectedCount, glm::vec4(0.0f));
    return;
  }

  const tinygltf::Accessor &accessor = model.accessors.at(accessorIndex);

  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      accessor.type != TINYGLTF_TYPE_VEC4) {
    throw std::runtime_error("Unsupported VEC4 attribute format in glTF");
  }

  const unsigned char *data = GetAccessorDataPtr(model, accessor);
  const size_t stride = GetAccessorStride(model, accessor);

  out.resize(accessor.count);

  for (size_t i = 0; i < accessor.count; ++i) {
    const float *v = reinterpret_cast<const float *>(data + i * stride);
    out[i] = glm::vec4(v[0], v[1], v[2], v[3]);
  }
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
  std::vector<glm::vec4> tangents;

  ReadVec3Attribute(model, positionAccessorIndex, positions, vertexCount);

  auto normalIt = primitive.attributes.find("NORMAL");
  ReadVec3Attribute(
      model, normalIt != primitive.attributes.end() ? normalIt->second : -1,
      normals, vertexCount);

  auto uvIt = primitive.attributes.find("TEXCOORD_0");
  ReadVec2Attribute(model,
                    uvIt != primitive.attributes.end() ? uvIt->second : -1, uvs,
                    vertexCount);

  auto tangentIt = primitive.attributes.find("TANGENT");
  const bool hasTangents = tangentIt != primitive.attributes.end();

  ReadVec4Attribute(model, hasTangents ? tangentIt->second : -1, tangents,
                    vertexCount);

  ImportedPrimitive out;
  out.hasTangents = hasTangents;
  out.vertices.resize(vertexCount);

  for (size_t i = 0; i < vertexCount; ++i) {
    out.vertices[i].position = positions[i];
    out.vertices[i].normal = normals[i];
    out.vertices[i].uv = uvs[i];
    out.vertices[i].tangent = tangents[i];

    const glm::vec3 &p = positions[i];

    out.localBounds.Expand(p);
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
    ImportedPrimitive importedPrimitive = LoadPrimitive(model, primitive);
    out.localBounds.Expand(importedPrimitive.localBounds.lower);
    out.localBounds.Expand(importedPrimitive.localBounds.upper);

    out.primitives.push_back(std::move(importedPrimitive));
  }

  return out;
}

static ImportedMaterial LoadMaterial(const tinygltf::Model &model,
                                     const tinygltf::Material &material) {
  ImportedMaterial out{};

  const auto &pbr = material.pbrMetallicRoughness;

  if (pbr.baseColorFactor.size() == 4) {
    out.baseColorFactor = glm::vec4(static_cast<float>(pbr.baseColorFactor[0]),
                                    static_cast<float>(pbr.baseColorFactor[1]),
                                    static_cast<float>(pbr.baseColorFactor[2]),
                                    static_cast<float>(pbr.baseColorFactor[3]));
  }

  out.metallicFactor = static_cast<float>(pbr.metallicFactor);
  out.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
  out.doubleSided = material.doubleSided;
  out.alphaCutoff = material.alphaCutoff;
  out.alphaMode = ToAlphaMode(material.alphaMode);

  if (pbr.baseColorTexture.index >= 0) {
    const int textureIndex = pbr.baseColorTexture.index;
    if (textureIndex < 0 ||
        textureIndex >= static_cast<int>(model.textures.size())) {
      throw std::runtime_error(
          "glTF material references invalid base color texture");
    }

    const tinygltf::Texture &tex = model.textures[textureIndex];
    out.baseColorTexture.imageIndex = tex.source;
    out.baseColorTexture.samplerIndex = tex.sampler;
    out.baseColorTexture.texCoord = pbr.baseColorTexture.texCoord;

    if (tex.source < 0 || tex.source >= static_cast<int>(model.images.size())) {
      throw std::runtime_error(
          "glTF material references invalid base color image");
    }
  }

  if (material.normalTexture.index >= 0) {
    const int textureIndex = material.normalTexture.index;
    if (textureIndex < 0 ||
        textureIndex >= static_cast<int>(model.textures.size())) {
      throw std::runtime_error(
          "glTF material references invalid normal texture");
    }

    const tinygltf::Texture &tex = model.textures[textureIndex];
    out.normalTexture.imageIndex = tex.source;
    out.normalTexture.samplerIndex = tex.sampler;
    out.normalTexture.texCoord = material.normalTexture.texCoord;

    if (tex.source < 0 || tex.source >= static_cast<int>(model.images.size())) {
      throw std::runtime_error("glTF material references invalid normal image");
    }
  }

  if (pbr.metallicRoughnessTexture.index >= 0) {
    const int textureIndex = pbr.metallicRoughnessTexture.index;
    if (textureIndex < 0 ||
        textureIndex >= static_cast<int>(model.textures.size())) {
      throw std::runtime_error(
          "glTF material references invalid metallic roughness texture");
    }

    const tinygltf::Texture &tex = model.textures[textureIndex];
    out.metallicRoughnessTexture.imageIndex = tex.source;
    out.metallicRoughnessTexture.samplerIndex = tex.sampler;
    out.metallicRoughnessTexture.texCoord =
        pbr.metallicRoughnessTexture.texCoord;

    if (tex.source < 0 || tex.source >= static_cast<int>(model.images.size())) {
      throw std::runtime_error(
          "glTF material references invalid metallic roughness image");
    }
  }

  return out;
}

static ImportedImage LoadImage(const tinygltf::Image &image) {
  ImportedImage out{};
  out.name = image.name;
  out.width = image.width;
  out.height = image.height;
  out.components = 4;

  if (image.image.empty()) {
    throw std::runtime_error("glTF image has no pixel data");
  }

  if (image.component == 4) {
    out.pixelsRGBA8 = image.image;
  } else {
    const size_t pixelCount =
        static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    out.pixelsRGBA8.resize(pixelCount * 4);

    for (size_t i = 0; i < pixelCount; ++i) {
      const size_t src = i * static_cast<size_t>(image.component);
      const size_t dst = i * 4;

      out.pixelsRGBA8[dst + 0] = image.image[src + 0];
      out.pixelsRGBA8[dst + 1] =
          image.component > 1 ? image.image[src + 1] : image.image[src + 0];
      out.pixelsRGBA8[dst + 2] =
          image.component > 2 ? image.image[src + 2] : image.image[src + 0];
      out.pixelsRGBA8[dst + 3] = 255;
    }
  }

  return out;
}

static ImportedSampler LoadSampler(const tinygltf::Sampler &sampler) {
  ImportedSampler out{};
  out.minFilter = sampler.minFilter;
  out.magFilter = sampler.magFilter;
  out.wrapS = sampler.wrapS;
  out.wrapT = sampler.wrapT;
  return out;
}

static void ExpandAABB(AABB &aabb, const glm::vec3 &p) {
  aabb.lower = glm::min(aabb.lower, p);
  aabb.upper = glm::max(aabb.upper, p);
}

static AABB ComputeSceneAABB(const ImportedScene &scene) {
  AABB aabb{};
  bool hasAnyPoint = false;

  std::function<void(int, const glm::mat4 &)> visit;

  visit = [&](int nodeIndex, const glm::mat4 &parentWorld) {
    const ImportedNode &node = scene.nodes.at(nodeIndex);

    const glm::mat4 world = parentWorld * node.transform.ToMatrix();

    if (node.meshIndex >= 0) {
      const ImportedMesh &mesh = scene.meshes.at(node.meshIndex);

      for (const ImportedPrimitive &primitive : mesh.primitives) {
        for (const ImportedVertex &vertex : primitive.vertices) {
          const glm::vec3 worldPos =
              glm::vec3(world * glm::vec4(vertex.position, 1.0f));

          if (!hasAnyPoint) {
            aabb.lower = worldPos;
            aabb.upper = worldPos;
            hasAnyPoint = true;
          } else {
            ExpandAABB(aabb, worldPos);
          }
        }
      }
    }

    for (int child : node.children) {
      visit(child, world);
    }
  };

  for (int rootNode : scene.rootNodes) {
    visit(rootNode, glm::mat4(1.0f));
  }

  if (!hasAnyPoint) {
    aabb.lower = glm::vec3(0.0f);
    aabb.upper = glm::vec3(0.0f);
  }

  return aabb;
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
    std::cout << err.c_str() << std::endl;
  }

  if (!ok) {
    throw std::runtime_error("Failed to load glTF file: " + path);
  }

  ImportedScene scene;

  scene.meshes.reserve(model.meshes.size());
  for (const tinygltf::Mesh &mesh : model.meshes) {
    scene.meshes.push_back(LoadMesh(model, mesh));
  }

  scene.materials.reserve(model.materials.size());
  for (const tinygltf::Material &material : model.materials) {
    scene.materials.push_back(LoadMaterial(model, material));
  }

  scene.images.reserve(model.images.size());
  for (const tinygltf::Image &image : model.images) {
    scene.images.push_back(LoadImage(image));
  }

  scene.samplers.reserve(model.samplers.size());
  for (const tinygltf::Sampler &sampler : model.samplers) {
    scene.samplers.push_back(LoadSampler(sampler));
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

  scene.worldBounds = ComputeSceneAABB(scene);

  return scene;
}

} // namespace Rodan
