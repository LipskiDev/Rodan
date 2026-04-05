workspace "Rodan"
	architecture "x86_64"
	startproject "Runtime"

	configurations
	{
		"Debug",
		"Release"
	}

	multiprocessorcompile "On"

	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "external/velos/premake/embedded.lua"

project "Rodan"
	location "build/Rodan"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"engine/**.h",
		"engine/**.hpp",
		"engine/**.cpp",
		"engine/**.c"
	}

	includedirs
	{
		"engine",
		"external/velos/velos",
		"external/velos/external/glfw/include",
		"external/velos/external/glm",
		"external/velos/external/volk",
		"external/velos/external/vma/include",
		"external/velos/external/stb",
		"external/velos/external/SPIRV-Reflect",
		"external/velos/tools/imgui",
		"external/velos/external/imgui",
		"external/implot",
		"external/assimp-install/include",
		"external/assimp/include",
		"external/meshoptimizer/src"
	}

	libdirs
	{
		"external/assimp-install/lib"
	}

	links
	{
		"Velos",
		"imgui",
		"implot",
		"meshoptimizer"
	}

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		systemversion "latest"
		pic "On"

	filter "configurations:Debug"
		defines
		{
			"TRACY_ENABLE"
		}
		links
		{
			"assimpd"
		}
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		links
		{
			"assimp"
		}
		runtime "Release"
		optimize "Speed"

	filter {}

project "Runtime"
	location "build/Runtime"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"runtime/**.h",
		"runtime/**.hpp",
		"runtime/**.cpp",
		"runtime/**.c"
	}

	includedirs
	{
		"engine",
		"external/velos/velos",
		"external/velos/external/glfw/include",
		"external/velos/external/glm",
		"external/velos/external/volk",
		"external/velos/external/vma/include",
		"external/velos/external/stb",
		"external/velos/external/SPIRV-Reflect",
		"external/velos/tools/imgui",
		"external/velos/external/imgui",
		"external/implot",
		"external/assimp-install/include",
		"external/assimp/include",
		"external/meshoptimizer/src"
	}

	libdirs
	{
		"external/assimp-install/lib"
	}

	links
	{
		"Rodan",
		"Velos",
		"imgui",
		"implot",
		"meshoptimizer"
	}

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		systemversion "latest"
		linkoptions
		{
			"-Wl,-rpath,'$$ORIGIN/../../../external/assimp-install/lib'"
		}
		links
		{
			"glfw",
			"vulkan",
			"shaderc_shared",
			"dl",
			"pthread",
			"X11",
			"Xrandr",
			"Xi",
			"Xxf86vm",
			"Xinerama",
			"Xcursor"
		}

	filter "configurations:Debug"
		defines
		{
			"TRACY_ENABLE"
		}
		links
		{
			"assimpd"
		}
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		links
		{
			"assimp"
		}
		runtime "Release"
		optimize "Speed"

	filter {}

project "implot"
	location "build/implot"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"external/implot/implot.h",
		"external/implot/implot.cpp",
		"external/implot/implot_items.cpp",
		"external/implot/implot_internal.h"
	}

	includedirs
	{
		"external/implot",
		"external/velos/external/imgui"
	}

	filter "system:linux"
		pic "On"

	filter {}

project "meshoptimizer"
	location "build/meshoptimizer"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"external/meshoptimizer/src/*.h",
		"external/meshoptimizer/src/*.cpp"
	}

	includedirs
	{
		"external/meshoptimizer/src"
	}

	filter "system:linux"
		pic "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "Speed"

	filter {}
