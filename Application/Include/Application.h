#pragma once
#include "Device/Context.h"
#include "Window/GLFW_Window.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Utils/EditorCamera.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"

#include <chrono>

struct ApplicationCreateInfo
{
	WindowProps WindowInfo;
	std::string AppName;
	std::string EngineName;
	std::filesystem::path AssetDirectory = "../Aqua/Assets/";

	uint32_t WorkerCount = 8;

	float FramesPerSeconds = 60.0f;

	bool EnableValidationLayers = false;
};

struct CameraMovementKeys
{
	ImGuiKey Forward           = ImGuiKey_W;
	ImGuiKey Backward          = ImGuiKey_S;
	ImGuiKey Left              = ImGuiKey_A;
	ImGuiKey Right             = ImGuiKey_D;
	ImGuiKey Up                = ImGuiKey_E;
	ImGuiKey Down              = ImGuiKey_Q;
};

inline vk::Bool32 DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type, const std::string& message)
{
	std::cout << "Vulkan API Core ";

	std::string bufMesg;
	std::string comp = "vkCreateSwapchainKHR() : pCreateInfo->imageFormat VK_FORMAT_R8G8B8A8_SRGB with tiling VK_IMAGE_TILING_OPTIMAL does not support usage that includes VK_IMAGE_USAGE_STORAGE_BIT.";

	switch (severity)
	{
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
			std::cout << "[Verbose] ";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
			std::cout << "[Info] ";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
			std::cout << "[Warning] ";
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
			std::cout << "[Error] ";
			std::cout << message << '\n';

			char buffer[2048];
			sprintf_s(buffer, "%s", message.c_str());

			bufMesg = buffer;

			if (bufMesg.find(comp, 0) == 0)
				return VK_TRUE;

			_STL_ASSERT(false, buffer);

			return VK_FALSE;
		default:
			break;
	}

	std::cout << message << std::endl;

	return VK_TRUE;
}

class Application
{
public:
	Application(const ApplicationCreateInfo& info);

	virtual bool OnStart() = 0;
	virtual bool OnUpdate(std::chrono::nanoseconds elaspedTime) = 0;
	virtual bool OnUIUpdate(std::chrono::nanoseconds elaspedTime) { return true; }

	void Run();
	bool IsRunning() const { return mRunning.load(); }

	std::filesystem::path GetAssetDirectory() const { return std::filesystem::absolute(mCreateInfo.AssetDirectory); }

	virtual ~Application() = default;

protected:
	std::unique_ptr<OpenGLWindow> mWindow;
	std::shared_ptr<vkLib::Context> mContext;
	std::shared_ptr<vkLib::Swapchain> mSwapchain;

	vkLib::Core::Ref<vk::Instance> mInstance;
	vkLib::Core::Ref<vk::SurfaceKHR> mSurface;
	vkLib::Core::Ref<vk::DebugUtilsMessengerEXT> mMessenger;

	std::shared_ptr<vkLib::InstanceMenagerie> mInstanceMenagerie;
	std::shared_ptr<vkLib::PhysicalDeviceMenagerie> mPhysicalDevices;

	std::atomic_bool mRunning = false;

	ApplicationCreateInfo mCreateInfo;

	static Application* sApplicationInstance;

protected:

	template <typename _Camera>
	Aqua::CameraMovementFlags MoveCamera(_Camera& camera, std::chrono::nanoseconds elaspedTime, bool allowOrientation = true, const CameraMovementKeys& keys = {});

private:
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void CreateInstance(const ApplicationCreateInfo& info);
	void CreateSurface();
	void SetupMessenger();
	void SetupContext(const ApplicationCreateInfo& info);
};

__declspec(selectany) Application* Application::sApplicationInstance = nullptr;

template <typename _Camera>
Aqua::CameraMovementFlags Application::MoveCamera(_Camera& camera, std::chrono::nanoseconds elaspedTime, bool allowOrientation /*= true*/, const CameraMovementKeys& keys /*= {}*/)
{
	if (!ImGui::IsWindowHovered())
		return {};

	Aqua::CameraMovementFlags movement;

	auto addMovement = [&movement](ImGuiKey key, Aqua::CameraMovement direction)
		{
			if (ImGui::IsKeyDown(key))
			{
				movement.SetFlag(direction);
			}
		};

	addMovement(keys.Forward, Aqua::CameraMovement::eForward);
	addMovement(keys.Backward, Aqua::CameraMovement::eBackward);
	addMovement(keys.Left, Aqua::CameraMovement::eLeft);
	addMovement(keys.Right, Aqua::CameraMovement::eRight);
	addMovement(keys.Up, Aqua::CameraMovement::eUp);
	addMovement(keys.Down, Aqua::CameraMovement::eDown);

	auto mousePosition = ImGui::GetMousePos();

	camera.OnUpdate(elaspedTime, movement, { mousePosition.x, mousePosition.y }, allowOrientation && ImGui::IsMouseDown(ImGuiMouseButton_Left));

	return movement;
}
