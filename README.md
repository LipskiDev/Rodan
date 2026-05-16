<div align="center">

# Rodan

### Real-Time Renderer built with Modern C++ and Vulkan

Rodan is a real-time rendering project focused on modern graphics programming, rendering architecture, and GPU systems.

Built on top of **Velos**, a custom Rendering Hardware Interface, Rodan explores production-oriented rendering techniques while maintaining clean, engine-like architecture.

<!-- Replace with your own screenshot -->
<img src="docs/images/sponza.png" width="85%"/>

![C++](https://img.shields.io/badge/C%2B%2B-23-blue)
![Vulkan](https://img.shields.io/badge/API-Vulkan-red)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-green)

</div>

---

## Features

### Rendering
- Physically Based Rendering (PBR)
- glTF 2.0 scene loading
- Normal mapping
- Directional shadow mapping

### Engine Systems
- Scene graph
- Orbit + free camera controls
- Runtime asset loading
- Material system

### Velos RHI
- Vulkan backend
- Command buffer abstraction
- Pipeline & descriptor management
- Explicit resource transitions

---

## Screenshots

### Sponza
![Sponza](docs/images/sponza.png)

### Occlusion Textures
![Shadows](docs/images/occlusion_texture.png)

---

## Architecture

```text
Rodan Renderer
      ↓
Velos RHI
      ↓
Vulkan Backend
      ↓
GPU
```

---

## Building

### Requirements
- Linux
- C++23 compiler
- Vulkan SDK
- Premake5

### Build

```bash
git clone --recursive https://github.com/LipskiDev/Rodan.git
cd Rodan
premake5 gmake2
make -j$(nproc)
```

---

## Roadmap

- [x] PBR materials
- [x] Shadow mapping
- [x] glTF loading
- [ ] Skybox rendering
- [ ] Image Based Lighting (IBL)
- [ ] Compute pipelines
- [ ] GPU-driven rendering
- [ ] Frame graph

---

## Motivation

Rodan is a long-term graphics engineering project built to deepen expertise in:

- Real-time rendering
- GPU programming
- Engine architecture
- Graphics debugging & profiling
