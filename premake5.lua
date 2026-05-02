workspace "Rodan"

	VULKAN_SDK = os.getenv("VULKAN_SDK")

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

-- =========================
-- RODAN
-- =========================
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
		"external/velos/velos/core",
		"external/velos/external/glfw/include",
		"external/velos/external/glm",
		"external/velos/external/volk",
		"external/velos/external/vma/include",
		"external/velos/external/stb",
		"external/velos/external/SPIRV-Reflect",
		"external/velos/tools/imgui",
		"external/velos/external/imgui",
		"external/implot",
		"external/meshoptimizer/src",
		"external/tinygltf",
    "external/velos/external/tracy/public"
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
		defines
		{
			"RODAN_PLATFORM_WINDOWS"
		}
		includedirs
		{
			"external/assimp-install-windows/include",
			VULKAN_SDK .. "/Include"
		}
		libdirs
		{
			"external/assimp-install-windows/lib"
		}

	filter "system:linux"
		systemversion "latest"
		pic "On"
		defines
		{
			"RODAN_PLATFORM_LINUX"
		}
		includedirs
		{
			"external/assimp-install/include"
		}
		libdirs
		{
			"external/assimp-install/lib"
		}

	-- Debug
	filter { "system:windows", "configurations:Debug" }
		prebuildcommands
		{
			"call ../../scripts/build_assimp.bat Debug"
		}
		defines { "TRACY_ENABLE" }
		links { "assimp-vc143-mtd" }
		runtime "Debug"
		symbols "On"

	filter { "system:linux", "configurations:Debug" }
		prebuildcommands
		{
			"bash ../../scripts/build_assimp.sh Debug"
		}
		defines { "TRACY_ENABLE" }
		links { "assimpd" }
		runtime "Debug"
		symbols "On"

	-- Release
	filter { "system:windows", "configurations:Release" }
		prebuildcommands
		{
			"call ../../scripts/build_assimp.bat Release"
		}
		links { "assimp-vc143-mt" }
		runtime "Release"
		optimize "Speed"

	filter { "system:linux", "configurations:Release" }
		prebuildcommands
		{
			"bash ../../scripts/build_assimp.sh Release"
		}
		links { "assimp" }
		runtime "Release"
		optimize "Speed"

	filter {}

-- =========================
-- RUNTIME
-- =========================
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
		"external/velos/velos/core",
		"external/velos/external/glfw/include",
		"external/velos/external/glm",
		"external/velos/external/volk",
		"external/velos/external/vma/include",
		"external/velos/external/stb",
		"external/velos/external/SPIRV-Reflect",
		"external/velos/tools/imgui",
		"external/velos/external/imgui",
		"external/implot",
		"external/meshoptimizer/src",
		"external/tinygltf"
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
		debugdir "%{wks.location}"
		defines
		{
			"RODAN_PLATFORM_WINDOWS"
		}
		includedirs
		{
			"external/assimp-install-windows/include",
			VULKAN_SDK .. "/Include"
		}
		libdirs
		{
			"external/assimp-install-windows/lib"
		}

	filter "system:linux"
		systemversion "latest"
		defines
		{
			"RODAN_PLATFORM_LINUX"
		}
		includedirs
		{
			"external/assimp-install/include"
		}
		libdirs
		{
			"external/assimp-install/lib"
		}
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
			"Xcursor",
			"z"
		}

	-- Debug
	filter { "system:windows", "configurations:Debug" }
		prebuildcommands
		{
			"call ../../scripts/build_assimp.bat Debug"
		}
		postbuildcommands
		{
			'{COPY} "%{wks.location}/external/assimp-install-windows/bin/assimp-vc143-mtd.dll" "%{cfg.targetdir}"'
		}
		defines { "TRACY_ENABLE" }
		links { "assimp-vc143-mtd" }
		runtime "Debug"
		symbols "On"

	filter { "system:linux", "configurations:Debug" }
		prebuildcommands
		{
			"bash ../../scripts/build_assimp.sh Debug"
		}
		defines { "TRACY_ENABLE" }
		links { "assimpd" }
		runtime "Debug"
		symbols "On"

	-- Release
	filter { "system:windows", "configurations:Release" }
		prebuildcommands
		{
			"call ../../scripts/build_assimp.bat Release"
		}
		postbuildcommands
		{
			'{COPY} "%{wks.location}/external/assimp-install-windows/bin/assimp-vc143-mt.dll" "%{cfg.targetdir}"'
		}
		links { "assimp-vc143-mt" }
		runtime "Release"
		optimize "Speed"

	filter { "system:linux", "configurations:Release" }
		prebuildcommands
		{
			"bash ../../scripts/build_assimp.sh Release"
		}
		links { "assimp" }
		runtime "Release"
		optimize "Speed"

	filter {}

-- =========================
-- IMPLOT
-- =========================
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

-- =========================
-- MESHOPTIMIZER
-- =========================
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
