# Rodan

Rodan is a real-time rendering engine built on top of the Velos Render Hardware Interface (RHI).  
It is designed as a modular, modern rendering framework for experimenting with real-time graphics techniques and engine architecture.

## Overview

Rodan sits above the Velos RHI and provides higher-level rendering systems such as:

- Scene representation (meshes, cameras, lights)
- Material and shader systems
- GPU resource management
- Render pipeline orchestration
- Rendering techniques (forward, deferred, clustered, etc.)

The project emphasizes **clean separation of concerns**:

- **Velos** → low-level GPU abstraction (Vulkan backend)
- **Rodan** → high-level rendering engine

This allows Rodan to evolve independently of the underlying graphics API.

---

## Architecture

Rodan follows a layered architecture:

[Runtime / App]

↓

[Renderer / Scene / Assets]

↓

[Graphics Abstraction]

↓

[Velos (RHI)]

↓

[Vulkan]


## Relationship to Velos

Rodan depends on Velos as a submodule:
external/velos/

Velos provides:
- Device and swapchain management
- Command lists and submission
- Buffers, images, pipelines
- Synchronization primitives

Rodan builds on top of these primitives and does **not** expose backend-specific APIs (e.g., Vulkan) to higher-level systems.

---

## Goals

- Build a clean and extensible rendering architecture
- Explore modern real-time rendering techniques
- Maintain strict separation between API abstraction and rendering logic
- Serve as a foundation for experimentation (graphics research, engine design)

---

## Current Status

Early development.

Implemented / in progress:
- Engine structure and module layout
- Integration with Velos RHI
- Basic rendering pipeline bootstrap

Planned:
- Mesh and material system
- Texture support
- Depth testing and render targets
- Camera and scene system
- Lighting (forward / clustered)
- Shadow mapping
- Render graph

---

## Building

Rodan uses Premake for project generation.

### Setup

```bash
git clone https://github.com/LipskiDev/Rodan
cd rodan
git submodule update --init --recursive

premake5 gmake
make
```
