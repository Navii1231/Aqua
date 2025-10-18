#include "Application.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"

#define ENABLE_VALIDATION   _DEBUG

Application::Application(const ApplicationCreateInfo& info)
	: mWindow(std::make_unique<OpenGLWindow>(info.WindowInfo)), mCreateInfo(info)
{
	_STL_ASSERT(!sApplicationInstance, "Application has already been created!");
	sApplicationInstance = this;

	CreateInstance(info);
	CreateSurface();

	if (info.EnableValidationLayers)
		SetupMessenger();

	mPhysicalDevices = std::make_shared<vkLib::PhysicalDeviceMenagerie>(mInstance);

	SetupContext(info);

	vkLib::SwapchainInfo swapchainInfo{};

	swapchainInfo.Width = mWindow->GetWindowSize().x;
	swapchainInfo.Height = mWindow->GetWindowSize().y;
	swapchainInfo.PresentMode = vk::PresentModeKHR::eMailbox;
	swapchainInfo.Surface = mSurface;

	mContext->CreateSwapchain(swapchainInfo);
	mSwapchain = mContext->GetSwapchain();

	Aqua::ImGuiInit(*mContext, mWindow->GetNativeHandle());
}

void Application::Run()
{
	std::chrono::nanoseconds timeSlice(uint64_t(1.0f / mCreateInfo.FramesPerSeconds * 1e9));

	std::chrono::high_resolution_clock clock;
	std::chrono::time_point<std::chrono::high_resolution_clock> prev = clock.now();
	std::chrono::time_point<std::chrono::high_resolution_clock> nextSlice = clock.now();

	auto duration = clock.now() - prev;

	mRunning.store(true);

	OnStart();

	while (mRunning.load() && !mWindow->IsWindowClosed())
	{
		// to limit the GPU usage, we could quantize the time into slices
		std::this_thread::sleep_until(nextSlice);
		nextSlice = clock.now() + timeSlice;

		OnUpdate(duration);
		OnUIUpdate(duration);
		mWindow->PollUserEvents();

		duration = clock.now() - prev;
		prev = clock.now();
	}

	mRunning.store(false);
}

void Application::CreateInstance(const ApplicationCreateInfo& info)
{
	std::vector<const char*> extensions{};

	if (info.EnableValidationLayers)
		extensions.push_back("VK_EXT_debug_utils");

	std::vector<const char*> layers{};

	if (info.EnableValidationLayers)
		layers.push_back("VK_LAYER_KHRONOS_validation"
		/*"VK_LAYER_NV_GPU_Trace_release_public_2024_1_1"*/
		/*"VK_LAYER_NV_nomad_release_public_2024_1_1"*/);

	mInstanceMenagerie = std::make_shared<vkLib::InstanceMenagerie>(extensions, layers);

	vkLib::InstanceCreateInfo instanceInfo{};
	instanceInfo.AppName = info.AppName;
	instanceInfo.EngineName = info.EngineName;
	instanceInfo.AppVersion = { 1, 0, 0 };
	instanceInfo.EngineVersion = { 1, 0, 0 };

	mInstance = mInstanceMenagerie->Create(instanceInfo);
}

void Application::CreateSurface()
{
	auto [Result, Surface] = mInstanceMenagerie->CreateSurface(mInstance, mWindow->GetNativeHandle());
	_STL_ASSERT(Result == vk::Result::eSuccess, "Could not create a surface!");
	mSurface = Surface;
}

void Application::SetupMessenger()
{
	vkLib::DebugMessengerCreateInfo messengerInfo{};

	messengerInfo.messageType =
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

	messengerInfo.messageSeverity =
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

	mMessenger = vkLib::InstanceMenagerie::CreateDebugMessenger(mInstance, messengerInfo, DebugCallback);
}

void Application::SetupContext(const ApplicationCreateInfo& info)
{
	vkLib::ContextCreateInfo deviceInfo{};

	deviceInfo.DeviceCapabilities =
		vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute |
		vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding;

	deviceInfo.Layers = {};
	//deviceInfo.Layers.push_back("VK_EXT_shader_atomic_float");

	if (info.EnableValidationLayers)
	{
		deviceInfo.Layers = { "VK_LAYER_KHRONOS_validation" };
		deviceInfo.Extensions = { VK_EXT_TOOLING_INFO_EXTENSION_NAME };
	}

	deviceInfo.MaxQueueCount = info.WorkerCount;
	deviceInfo.PhysicalDevice = (*mPhysicalDevices)[0];
	deviceInfo.RequiredFeatures = deviceInfo.PhysicalDevice.Features;

	try
	{
		mContext = std::make_shared<vkLib::Context>(deviceInfo);
	}
	catch (vk::SystemError& e)
	{
		std::cout << e.what() << std::endl;
		std::cin.get();
		__debugbreak();
	}
}

