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
		"external/velos/external/imgui"
	}

	links
	{
		"Velos"
	}

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		systemversion "latest"
		pic "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
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
		"external/velos/external/imgui"
	}

	links
	{
		"Rodan",
		"Velos"
	}

	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "Speed"

	filter {}
