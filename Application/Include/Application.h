#pragma once
#include "Device/Context.h"
#include "Window/GLFW_Window.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Utils/EditorCamera.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"
#include "Layer.h"

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

	void Run();
	bool IsRunning() const { return mRunning.load(); }

	void AddLayer(Aqua::SharedRef<Layer> layer);
	void RemoveLayer(size_t whichOne = 0);
	void SwapLayers(size_t first, size_t second);

	void SetDefaultEventCallbacks();

	bool InvokeFramebufferCallback(const glm::ivec2& position)
	{ return InvokeEvents(&Layer::FramebufferCallbackHelper, position); }

	bool InvokeKeyCallback(GLFWKeyAction action, int key, int mods, int scancode)
	{ return InvokeEvents(&Layer::KeyCallbackHelper, action, key, mods, scancode); }

	bool InvokeCharCallback(uint32_t par) { return InvokeEvents(&Layer::CharCallbackHelper, par); }
	bool InvokeMouseButtonCallback(GLFWKeyAction action, int button, int scancode) { return InvokeEvents(&Layer::MouseButtonCallbackHelper, action, button, scancode); }
	bool InvokeWindowPositionCallback(const glm::ivec2& position) { return InvokeEvents(&Layer::WindowPositionCallbackHelper, position); }
	bool InvokeWindowCloseCallback() { return InvokeEvents(&Layer::WindowCloseCallbackHelper); }
	bool InvokeCursorPosCallback(const glm::ivec2& position) { return InvokeEvents(&Layer::CursorPosCallbackHelper, position); }
	bool InvokeScrollCallback(const glm::ivec2& off) { return InvokeEvents(&Layer::ScrollCallbackHelper, off); }

	std::shared_ptr<vkLib::Context> GetContext() const { return mContext; }
	Aqua::SharedRef<Layer> GetLayer(size_t whichOne) const { return mLayers[whichOne]; }
	OpenGLWindow* GetWindow() const { return mWindow.get(); }

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

	std::vector<Aqua::SharedRef<Layer>> mLayers;

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

private:
	bool InvokeStart();
	bool InvokeUpdate(std::chrono::nanoseconds timer);

	template <typename Fn, typename ...Args>
	bool InvokeEvents(Fn&& eventFn, Args&& ...args)
	{
		// invoking event from top to bottom layer

		for (auto layerIt = mLayers.rbegin(); layerIt != mLayers.rend(); layerIt++)
		{
			auto& layer = **layerIt;
			
			bool blocked = (layer.*eventFn)(std::forward<Args>(args)...);

			if (blocked)
				return true;
		}

		return false;
	}
};

__declspec(selectany) Application* Application::sApplicationInstance = nullptr;
