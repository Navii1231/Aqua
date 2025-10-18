outputDir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "ImGuiLib"
	location ""
	kind "StaticLib"
	language "C++"

    targetdir ("../out/bin/" .. outputDir .. "/%{prj.name}")
    objdir ("../out/int/" .. outputDir .. "/%{prj.name}")

    targetname "%{prj.name}"

    flags { "MultiProcessorCompile" }

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
		"%{prj.location}/Include/",
		"%{prj.location}/../VulkanLibrary/Dependencies/Include/",
		"%{prj.location}/../VulkanLibrary/Include/",
	}

    defines
    {
        "IMGUI_BUILD_DLL"
    }

    libdirs
    {
    }

    links
    {
    }

		filter "system:windows"
        cppdialect "C++23"
        staticruntime "On"
        systemversion "10.0"

        defines
        {
            "_CONSOLE",
            "WIN32",
        }

        filter "configurations:Debug"

            targetname "%{prj.name}d"

            defines
            {
                "_DEBUG",
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
