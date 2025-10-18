outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"
outputSuffix = ""

project "Aqua"
	location ""
	kind "SharedLib"
	language "C++"

    flags { "MultiProcessorCompile" }

    targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")

    targetname "%{prj.name}"

    pchheader "Core/aqpch.h"
    pchsource "%{prj.location}/Src/Core/aqpch.cpp"

	files
	{
		"%{prj.location}/**.h",
		"%{prj.location}/**.hpp",
		"%{prj.location}/**.c",
		"%{prj.location}/**.cpp",
        "%{prj.location}/**.txt",
        "%{prj.location}/**.glsl",
		"%{prj.location}/**.vert",
		"%{prj.location}/**.frag",
		"%{prj.location}/**.geom",
		"%{prj.location}/**.comp",
        "%{prj.location}/**.yaml",
		"%{prj.location}/**.lua",
	}

	includedirs
	{
        -- Aqua own stuff...
		"%{prj.location}/Dependencies/Include/",
		"%{prj.location}/Include/",

        -- Vulkan library...
        "%{prj.location}/../VulkanLibrary/Include/",
		"%{prj.location}/../VulkanLibrary/Dependencies/Include/",

        -- ImGui library...
        "%{prj.location}/../ImGuiLib/Include/",

        -- Yaml library...
        "%{prj.location}/../Yaml/Include/",
	}

    libdirs
    {
    	"%{prj.location}/Dependencies/lib/",
    }

    links
    {
        "VulkanLibrary",
        "ImGuiLib",
        "Yaml",
        "Lua/lua54.lib"
    }

    defines
    {
        --"YAML_CPP_STATIC_DEFINE",
        "yaml_cpp_EXPORTS",
        "IMGUI_BUILD_DLL",
        "VKLIB_BUILD_DLL",
        "AQUA_BUILD_DLL"
    }

    linkoptions
    {
        "/IGNORE:4099",
        "/IGNORE:4217"
    }

    filter "configurations:Debug"
        outputSuffix = "d"

    filter "toolset:msc*"
        linkoptions 
        { 
            "/WHOLEARCHIVE:VulkanLibrary" .. outputSuffix,
            "/WHOLEARCHIVE:ImGuiLib" .. outputSuffix,
            "/WHOLEARCHIVE:Yaml" .. outputSuffix
        }

	filter "system:windows"
        cppdialect "C++23"
        staticruntime "On"
        systemversion "10.0"

        filter "configurations:Debug"
            targetname "%{prj.name}d"

            defines 
            {
                "_DEBUG",
            }

            links
            {
                "Vulkan/glslangd.lib",
                "Vulkan/GenericCodeGend.lib",
                "Vulkan/glslang-default-resource-limitsd.lib",
                "Vulkan/SPIRVd.lib",
                "Vulkan/SPIRV-Toolsd.lib",
                "Vulkan/SPIRV-Tools-linkd.lib",
                "Vulkan/SPIRV-Tools-optd.lib",
                "Vulkan/spirv-cross-cored.lib",
                "Vulkan/spirv-cross-glsld.lib",
                "Vulkan/OSDependentd.lib",
                "Vulkan/MachineIndependentd.lib",
                "Assimp/Debug/assimp-vc143-mtd.lib",
                "Assimp/Debug/zlibstaticd.lib",
            }

            inlining "Disabled"
            symbols "On"
            staticruntime "Off"
            runtime "Debug"

        filter "configurations:Release"
            links
            {
                "Vulkan/glslang.lib",
                "Vulkan/GenericCodeGen.lib",
                "Vulkan/glslang-default-resource-limits.lib",
                "Vulkan/SPIRV.lib",
                "Vulkan/SPIRV-Tools.lib",
                "Vulkan/SPIRV-Tools-link.lib",
                "Vulkan/SPIRV-Tools-opt.lib",
                "Vulkan/spirv-cross-core.lib",
                "Vulkan/spirv-cross-glsl.lib",
                "Vulkan/OSDependent.lib",
                "Vulkan/MachineIndependent.lib",
                "Assimp/Release/assimp-vc143-mt.lib",
                "Assimp/Release/zlibstatic.lib",
            }

            defines "NDEBUG"
            optimize "Full"
            inlining "Auto"
            staticruntime "Off"
            runtime "Release"
