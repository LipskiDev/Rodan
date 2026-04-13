# Building Rodan on Windows

## Prerequisites

1. **CMake** - https://cmake.org/download/
2. **Premake5** - https://premake.github.io/download (must be in PATH)
3. **Vulkan SDK** - https://vulkan.lunarg.com/sdk/home (LunarG installer, sets `VULKAN_SDK` env var automatically)
4. **Visual Studio 2022** (or Build Tools for VS 2022) with C++ desktop workload

You can install Premake5 and Vulkan SDK via winget:

```powershell
winget install Premake.Premake.5.Beta
winget install KhronosGroup.VulkanSDK
```

> After installing, restart your terminal so PATH and VULKAN_SDK are picked up.

## Clone

```bash
git clone --recursive https://github.com/LipskiDev/Rodan
cd Rodan
```

## Generate Project Files

```bash
premake5 vs2022
```

## Build

### Option A: Visual Studio IDE

Open `Rodan.sln`, set **Runtime** as the startup project, select **Release | x64**, and build.

### Option B: Command Line (MSBuild)

```powershell
msbuild Rodan.sln /p:Configuration=Release /p:Platform=x64 /m
```

> **Note:** The Vulkan SDK only ships release-mode `shaderc_combined.lib`, so building in **Release** configuration is recommended. Debug builds will produce linker errors due to runtime library mismatches with the SDK's shaderc library.

## Run

Launch from the Rodan root directory (assets/shaders are loaded relative to the working directory):

```powershell
.\bin\Release-windows-x86_64\Runtime\Runtime.exe
```
