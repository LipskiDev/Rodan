#include "duck_scene.h"

#include "imgui.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

namespace Rodan {

using namespace Velos;
using namespace Velos::RHI;

void DuckScene::Initialize(IDevice *device, SwapchainHandle swapchain,
                           Format colorFormat, Format depthFormat) {
  device_ = device;
  swapchain_ = swapchain;
  colorFormat_ = colorFormat;
  depthFormat_ = depthFormat;

  leftDuckModel_ =
      glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.5f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

  rightDuckModel_ =
      glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.5f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

  camera_.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);

  CreateResources(device_);
  CreateDescriptors(device_);
  CreatePipeline(device_);
}

void DuckScene::Shutdown(IDevice *device) {
  if (!device) {
    return;
  }

  device->DestroyPipeline(duck_.pipeline);
  device->DestroyShader(duck_.fragmentShader);
  device->DestroyShader(duck_.vertexShader);

  device->DestroyDescriptorPool(duck_.descriptorPool);
  device->DestroyDescriptorSetLayout(duck_.setLayout);

  device->DestroySampler(duck_.sampler);
  device->DestroyImageView(duck_.imageView);
  device->DestroyImage(duck_.image);

  device->DestroyBuffer(duck_.stagingBuffer);
  device->DestroyBuffer(duck_.lodIndexBuffer);
  device->DestroyBuffer(duck_.indexBuffer);
  device->DestroyBuffer(duck_.vertexBuffer);

  duck_ = {};
  device_ = nullptr;
  swapchain_ = {};
}

void DuckScene::OnResize(IDevice *device, u32 width, u32 height) {
  (void)device;
  (void)width;
  (void)height;
}

void DuckScene::Update(float deltaSeconds, const SceneUpdateContext &ctx) {
  if (ctx.framebufferWidth == 0 || ctx.framebufferHeight == 0) {
    return;
  }

  camera_.SetPerspective(60.0f,
                         static_cast<float>(ctx.framebufferWidth) /
                             static_cast<float>(ctx.framebufferHeight),
                         0.1f, 100.0f);

  if (ctx.input) {
    for (const InputEvent &event : ctx.input->GetEvents()) {
      if (event.type == InputEventType::KeyDown ||
          event.type == InputEventType::KeyUp) {
        camera_.OnKeyboard(event);
      } else if (event.type == InputEventType::MouseMove) {
        float dx = 0.0f;
        float dy = 0.0f;

        if (firstMouse_) {
          lastMouseX_ = event.mouseMove.x;
          lastMouseY_ = event.mouseMove.y;
          firstMouse_ = false;
        } else {
          dx = event.mouseMove.x - lastMouseX_;
          dy = event.mouseMove.y - lastMouseY_;
          lastMouseX_ = event.mouseMove.x;
          lastMouseY_ = event.mouseMove.y;
        }

        if (ctx.input->IsMouseDown(MouseButton::Right)) {
          camera_.OnMouseMove(dx, dy);
        }
      }
    }
  }

  camera_.Update(deltaSeconds);
}

void DuckScene::Prepare(ICommandList &cmd) { UploadTextureIfNeeded(cmd); }

void DuckScene::Render(ICommandList &cmd) {

  RenderDuckInstance(cmd, leftDuckModel_, duck_.indexBuffer, duck_.indexCount);
  RenderDuckInstance(cmd, rightDuckModel_, duck_.lodIndexBuffer,
                     duck_.lodIndexCount);
}

void DuckScene::RenderImGui() {
  ImGui::Begin("Duck Scene");
  ImGui::Text("Left duck: full mesh");
  ImGui::Text("Right duck: LOD mesh");
  ImGui::Separator();
  ImGui::Text("Vertices: %u", duck_.vertexCount);
  ImGui::Text("Full indices: %u", duck_.indexCount);
  ImGui::Text("LOD indices: %u", duck_.lodIndexCount);
  ImGui::Text("LOD ratio: %.2f", lodRatio_);
  ImGui::End();
}

DuckScene::DuckMeshData DuckScene::LoadDuckMesh() const {
  const aiScene *scene = aiImportFile(
      "assets/meshes/rubber_duck.gltf",
      aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
          aiProcess_ImproveCacheLocality | aiProcess_PreTransformVertices);

  if (!scene) {
    const char *err = aiGetErrorString();
    throw std::runtime_error(
        std::string("Unable to load assets/meshes/rubber_duck.gltf: ") +
        (err ? err : "unknown assimp error"));
  }

  if (!scene->HasMeshes()) {
    aiReleaseImport(scene);
    throw std::runtime_error("rubber_duck.gltf loaded, but contains no meshes");
  }

  const aiMesh *mesh = scene->mMeshes[0];
  if (!mesh) {
    aiReleaseImport(scene);
    throw std::runtime_error("rubber_duck.gltf has no valid mesh");
  }

  std::vector<DuckVertex> vertices;
  vertices.reserve(mesh->mNumVertices);

  for (u32 i = 0; i < mesh->mNumVertices; ++i) {
    const aiVector3D &v = mesh->mVertices[i];

    aiVector3D t(0.0f, 0.0f, 0.0f);
    if (mesh->HasTextureCoords(0)) {
      t = mesh->mTextureCoords[0][i];
    }

    DuckVertex vert{};
    vert.position = glm::vec3(v.x, v.y, v.z);
    vert.uv = glm::vec2(t.x, t.y);

    vertices.push_back(vert);
  }

  std::vector<u32> indices;
  indices.reserve(mesh->mNumFaces * 3);

  for (u32 i = 0; i < mesh->mNumFaces; ++i) {
    const aiFace &face = mesh->mFaces[i];
    if (face.mNumIndices != 3) {
      continue;
    }

    indices.push_back(static_cast<u32>(face.mIndices[0]));
    indices.push_back(static_cast<u32>(face.mIndices[1]));
    indices.push_back(static_cast<u32>(face.mIndices[2]));
  }

  aiReleaseImport(scene);

  if (vertices.empty() || indices.empty()) {
    throw std::runtime_error(
        "rubber_duck.gltf produced empty vertex/index data");
  }

  std::vector<u32> remap(indices.size());
  const size_t remappedVertexCount = meshopt_generateVertexRemap(
      remap.data(), indices.data(), indices.size(), vertices.data(),
      vertices.size(), sizeof(DuckVertex));

  std::vector<DuckVertex> optimizedVertices(remappedVertexCount);
  std::vector<u32> optimizedIndices(indices.size());

  meshopt_remapVertexBuffer(optimizedVertices.data(), vertices.data(),
                            vertices.size(), sizeof(DuckVertex), remap.data());

  meshopt_remapIndexBuffer(optimizedIndices.data(), indices.data(),
                           indices.size(), remap.data());

  meshopt_optimizeVertexCache(optimizedIndices.data(), optimizedIndices.data(),
                              optimizedIndices.size(),
                              optimizedVertices.size());

  meshopt_optimizeOverdraw(
      optimizedIndices.data(), optimizedIndices.data(), optimizedIndices.size(),
      reinterpret_cast<const float *>(&optimizedVertices[0].position),
      optimizedVertices.size(), sizeof(DuckVertex), 1.05f);

  meshopt_optimizeVertexFetch(optimizedVertices.data(), optimizedIndices.data(),
                              optimizedIndices.size(), optimizedVertices.data(),
                              optimizedVertices.size(), sizeof(DuckVertex));

  std::vector<u32> indicesLod;
  {
    const size_t targetIndexCount =
        static_cast<size_t>(optimizedIndices.size() * lodRatio_);
    const float targetError = 1e-2f;

    indicesLod.resize(optimizedIndices.size());

    const size_t lodIndexCount = meshopt_simplify(
        indicesLod.data(), optimizedIndices.data(), optimizedIndices.size(),
        reinterpret_cast<const float *>(&optimizedVertices[0].position),
        optimizedVertices.size(), sizeof(DuckVertex), targetIndexCount,
        targetError);

    indicesLod.resize(lodIndexCount);
  }

  std::cout << "[DuckScene] Vertices: " << optimizedVertices.size() << "\n";
  std::cout << "[DuckScene] Full indices: " << optimizedIndices.size() << "\n";
  std::cout << "[DuckScene] LOD indices: " << indicesLod.size() << "\n";

  return {
      .vertices = std::move(optimizedVertices),
      .indices = std::move(optimizedIndices),
      .lodIndices = std::move(indicesLod),
  };
}

void DuckScene::CreateResources(IDevice *device) {
  const DuckMeshData mesh = LoadDuckMesh();

  duck_.vertexCount = static_cast<u32>(mesh.vertices.size());
  duck_.indexCount = static_cast<u32>(mesh.indices.size());
  duck_.lodIndexCount = static_cast<u32>(mesh.lodIndices.size());

  duck_.vertexBuffer = device->CreateBuffer({
      .size = static_cast<u64>(mesh.vertices.size() * sizeof(DuckVertex)),
      .usage = BufferUsage::Vertex,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = mesh.vertices.data(),
      .debugName = "Duck Vertex Buffer",
  });

  duck_.indexBuffer = device->CreateBuffer({
      .size = static_cast<u64>(mesh.indices.size() * sizeof(u32)),
      .usage = BufferUsage::Index,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = mesh.indices.data(),
      .debugName = "Duck Index Buffer",
  });

  duck_.lodIndexBuffer = device->CreateBuffer({
      .size = static_cast<u64>(mesh.lodIndices.size() * sizeof(u32)),
      .usage = BufferUsage::Index,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = mesh.lodIndices.data(),
      .debugName = "Duck LOD Index Buffer",
  });

  stbi_set_flip_vertically_on_load(1);

  int texW = 0;
  int texH = 0;
  int texComp = 0;
  stbi_uc *pixels = stbi_load("assets/textures/Duck_baseColor.png", &texW,
                              &texH, &texComp, 4);

  if (!pixels) {
    throw std::runtime_error(
        "Failed to load assets/textures/Duck_baseColor.png");
  }

  duck_.textureWidth = static_cast<u32>(texW);
  duck_.textureHeight = static_cast<u32>(texH);

  const u64 imageSize =
      static_cast<u64>(duck_.textureWidth) * duck_.textureHeight * 4ull;

  duck_.stagingBuffer = device->CreateBuffer({
      .size = imageSize,
      .usage = BufferUsage::TransferSrc,
      .memoryUsage = MemoryUsage::CPUToGPU,
      .initialData = pixels,
      .debugName = "Duck Texture Upload Buffer",
  });

  stbi_image_free(pixels);

  duck_.image = device->CreateImage({
      .width = duck_.textureWidth,
      .height = duck_.textureHeight,
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Format::RGBA8_UNORM,
      .type = ImageType::Image2D,
      .usage = ImageUsage::TransferDst | ImageUsage::Sampled,
  });

  duck_.imageView = device->CreateImageView({
      .image = duck_.image,
      .format = Format::RGBA8_UNORM,
      .type = ImageViewType::View2D,
      .aspect = ImageAspect::Color,
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 1,
      .debugName = "Duck Texture View",
  });

  duck_.sampler = device->CreateSampler({
      .minFilter = Filter::Linear,
      .magFilter = Filter::Linear,
      .addressU = SamplerAddressMode::Repeat,
      .addressV = SamplerAddressMode::Repeat,
      .addressW = SamplerAddressMode::Repeat,
      .debugName = "Duck Sampler",
  });
}

void DuckScene::CreateDescriptors(IDevice *device) {
  DescriptorBindingDesc bindings[] = {
      {
          .binding = 0,
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
          .visibility = ShaderStage::Fragment,
      },
  };

  duck_.setLayout = device->CreateDescriptorSetLayout({
      .bindings = bindings,
      .bindingCount = 1,
      .debugName = "Duck Descriptor Set Layout",
  });

  DescriptorPoolSize poolSizes[] = {
      {
          .type = DescriptorType::CombinedImageSampler,
          .count = 1,
      },
  };

  duck_.descriptorPool = device->CreateDescriptorPool({
      .poolSizes = poolSizes,
      .poolSizeCount = 1,
      .maxSets = 1,
      .debugName = "Duck Descriptor Pool",
  });

  duck_.descriptorSet = device->AllocateDescriptorSet(
      duck_.descriptorPool, duck_.setLayout, "Duck Descriptor Set");

  DescriptorImageInfo imageInfo{};
  imageInfo.sampler = duck_.sampler;
  imageInfo.imageView = duck_.imageView;
  imageInfo.imageLayout = ImageLayout::ShaderReadOnly;

  device->UpdateDescriptorSet({
      .dstSet = duck_.descriptorSet,
      .binding = 0,
      .arrayElement = 0,
      .type = DescriptorType::CombinedImageSampler,
      .bufferInfo = nullptr,
      .imageInfo = &imageInfo,
      .descriptorCount = 1,
  });
}

void DuckScene::CreatePipeline(IDevice *device) {
  auto vertSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/duck.vert",
      .stage = ShaderStage::Vertex,
      .entryPoint = "main",
  });

  auto fragSpv = ShaderCompiler::CompileFile({
      .path = "assets/shaders/duck.frag",
      .stage = ShaderStage::Fragment,
      .entryPoint = "main",
  });

  duck_.vertexShader = device->CreateShader({
      .stage = ShaderStage::Vertex,
      .bytecode = vertSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(vertSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = vertSpv.reflection,
      .debugName = "Duck Vertex Shader",
  });

  duck_.fragmentShader = device->CreateShader({
      .stage = ShaderStage::Fragment,
      .bytecode = fragSpv.spirv.data(),
      .bytecodeSize =
          static_cast<u64>(fragSpv.spirv.size() * sizeof(std::uint32_t)),
      .entryPoint = "main",
      .reflection = fragSpv.reflection,
      .debugName = "Duck Fragment Shader",
  });

  VertexBufferLayoutDesc layout{
      .stride = sizeof(DuckVertex),
      .inputRate = VertexInputRate::PerVertex,
      .attributes = {{
                         .location = 0,
                         .format = VertexFormat::Float32x3,
                         .offset = offsetof(DuckVertex, position),
                     },
                     {
                         .location = 1,
                         .format = VertexFormat::Float32x2,
                         .offset = offsetof(DuckVertex, uv),
                     }},
  };

  DescriptorSetLayoutHandle setLayouts[] = {duck_.setLayout};

  GraphicsPipelineDesc pipelineDesc{};
  pipelineDesc.vertexShader = duck_.vertexShader;
  pipelineDesc.fragmentShader = duck_.fragmentShader;
  pipelineDesc.vertexLayouts.push_back(layout);
  pipelineDesc.layout.descriptorSetLayouts = setLayouts;
  pipelineDesc.layout.descriptorSetLayoutCount = 1;
  pipelineDesc.topology = PrimitiveTopology::TriangleList;
  pipelineDesc.raster.cullBackFaces = false;
  pipelineDesc.raster.frontFaceCCW = true;
  pipelineDesc.raster.wireframe = true;
  pipelineDesc.blend = {.enable = false};
  pipelineDesc.colorFormat = colorFormat_;
  pipelineDesc.depth = {
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthFormat = depthFormat_,
  };
  pipelineDesc.debugName = "Duck Scene Pipeline";

  duck_.pipeline = device->CreateGraphicsPipeline(pipelineDesc);
}

void DuckScene::UploadTextureIfNeeded(ICommandList &cmd) {
  if (duck_.uploaded) {
    return;
  }

  cmd.Barrier({
      .image = duck_.image,
      .newLayout = ImageLayout::TransferDst,
      .aspect = ImageAspect::Color,
  });

  BufferImageCopyRegion region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.mipLevel = 0;
  region.baseArrayLayer = 0;
  region.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {duck_.textureWidth, duck_.textureHeight, 1};
  region.aspect = ImageAspect::Color;

  cmd.CopyBufferToImage(duck_.stagingBuffer, duck_.image, region);

  cmd.Barrier({
      .image = duck_.image,
      .newLayout = ImageLayout::ShaderReadOnly,
      .aspect = ImageAspect::Color,
  });

  duck_.uploaded = true;
}

void DuckScene::RenderDuckInstance(ICommandList &cmd, const glm::mat4 &model,
                                   BufferHandle indexBuffer, u32 indexCount) {
  DuckPushConstants push{};
  push.model = model;
  push.view = camera_.GetView();
  push.proj = camera_.GetProjection();

  cmd.BindPipeline(duck_.pipeline);
  cmd.BindDescriptorSet(duck_.pipeline, 0, duck_.descriptorSet);
  cmd.BindVertexBuffer(0, duck_.vertexBuffer, 0);
  cmd.BindIndexBuffer(indexBuffer, IndexType::U32, 0);
  cmd.PushConstants(ShaderStage::Vertex, 0, sizeof(DuckPushConstants), &push);
  cmd.DrawIndexed(indexCount);
}

} // namespace Rodan
