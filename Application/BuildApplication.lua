outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "Application"
	location ""
	kind "ConsoleApp"
	language "C++"

	targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")
    flags {"MultiProcessorCompile"}

    defines
    {
        "WIN32",
    }

	files
	{
		"%{prj.location}/**.h",
		"%{prj.location}/**.hpp",
		"%{prj.location}/**.c",
		"%{prj.location}/**.cpp",
		"%{prj.location}/**.txt",
		"%{prj.location}/**.lua",
	}

	includedirs
	{
        -- Application stuff
		"%{prj.location}/Include/",

        -- VulkanLibrary
        "%{prj.location}/../VulkanLibrary/Include/",
		"%{prj.location}/../VulkanLibrary/Dependencies/Include/",

        -- Aqua project
        "%{prj.location}/../Aqua/Include/",
        "%{prj.location}/../Aqua/Dependencies/Include/",

        -- ImGui library
        "%{prj.location}/../ImGuiLib/Include/",

        -- Yaml library
        "%{prj.location}/../Yaml/Include/",
	}

    libdirs
    {
    	"%{prj.location}/Dependencies/Lib/",
    }

    links
    {
        "Aqua",
        "vulkan-1.lib",
    }

    defines
    {
        --"YAML_CPP_STATIC_DEFINE",
        --"VKLIB_BUILD_STATIC",
        --"IMGUI_BUILD_STATIC",
    }

    filter "toolset:msc*"
        linkoptions { "/IGNORE:4099" }

		filter "system:windows"
        cppdialect "C++23"
        staticruntime "On"
        systemversion "10.0"

        defines
        {
            "_CONSOLE"
        }

        filter "configurations:Debug"
            defines 
            {
                "_DEBUG"
            }

            inlining "Disabled"
            symbols "On"
            staticruntime "Off"
            runtime "Debug"

        filter "configurations:Release"

            defines "NDEBUG"
            optimize "Full"
            inlining "Auto"
            staticruntime "Off"
            runtime "Release"
