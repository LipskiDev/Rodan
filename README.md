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
