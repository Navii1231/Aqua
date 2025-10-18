#pragma once
#include "../Core/Config.h"
#include "UnsupportedLayerAndExtensionException.h"
#include "../Core/Ref.h"
#include "GLFW/glfw3.h"

VK_BEGIN

#define DELETE_TOKEN    "$(vkLib.DeleteMessenger)"

struct Version
{
	uint32_t Major = 1;
	uint32_t Minor = 0;
	uint32_t Patch = 0;
};

struct InstanceCreateInfo
{
	std::string EngineName;
	Version EngineVersion;

	std::string AppName;
	Version AppVersion;
};

struct DebugMessengerCreateInfo
{
	vk::DebugUtilsMessageSeverityFlagsEXT messageSeverity = {};
	vk::DebugUtilsMessageTypeFlagsEXT     messageType = {};
};

class InstanceMenagerie
{
public:
	VKLIB_API InstanceMenagerie(const std::vector<const char*>& extensions, const std::vector<const char*>& layers);

	VKLIB_API Core::Ref<vk::Instance> Create(const InstanceCreateInfo& info) const;

	VKLIB_API static std::vector<std::string> GetAllLayerNames();
	VKLIB_API static std::vector<std::string> GetAllExtensionNames();

	template <typename Callback> 
	static Core::Ref<vk::DebugUtilsMessengerEXT> CreateDebugMessenger(Core::Ref<vk::Instance> instance, const DebugMessengerCreateInfo& createInfo, Callback callback);

	VKLIB_API vk::ResultValue<Core::Ref<vk::SurfaceKHR>> CreateSurface(Core::Ref<vk::Instance> instance, GLFWwindow* context);

	~InstanceMenagerie() {}

	InstanceMenagerie(const InstanceMenagerie&) = delete;
	InstanceMenagerie& operator=(const InstanceMenagerie&) = delete;

	InstanceMenagerie(InstanceMenagerie&& Other) noexcept
		: mExtensions(Other.mExtensions), 
		mLayers(Other.mLayers),
		mAPI_Version(Other.mAPI_Version) {}

	const std::vector<const char*>& GetExtensions() const { return mExtensions; }
	const std::vector<const char*>& GetLayers() const { return mLayers; }

private:
	std::vector<const char*> mExtensions;
	std::vector<const char*> mLayers;

	bool mSurfaceSupported = false;

	const uint32_t mAPI_Version = -1;

	void TryAddingWindowSurfaceExtensions(const std::vector<vk::ExtensionProperties>& AllExtensions);
};

template <typename Callback>
Core::Ref<vk::DebugUtilsMessengerEXT> VK_NAMESPACE::InstanceMenagerie::CreateDebugMessenger(
	Core::Ref<vk::Instance> instance, const DebugMessengerCreateInfo& createInfo, Callback callback)
{
	vk::detail::DispatchLoaderDynamic dispatcher(*instance, vkGetInstanceProcAddr);

	vk::DebugUtilsMessengerCreateInfoEXT rawCreateInfo{};

	rawCreateInfo.messageSeverity = createInfo.messageSeverity;
	rawCreateInfo.messageType = createInfo.messageType;
	rawCreateInfo.pfnUserCallback = [](vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)->vk::Bool32
	{
		Callback& UserCallback = *(Callback*) pUserData;
		std::string DeleteToken = DELETE_TOKEN;

		if (DeleteToken == pCallbackData->pMessage)
		{
			Callback* pUserCallback = &UserCallback;
			delete pUserCallback;
			return VK_TRUE;
		}

		return UserCallback(messageSeverity, messageTypes, pCallbackData->pMessage);
	};

	rawCreateInfo.pUserData = (void*) new Callback(callback);

	return vkLib::Core::CreateRef<vk::DebugUtilsMessengerEXT>([instance](vk::DebugUtilsMessengerEXT& handle)
	{
		vk::detail::DispatchLoaderDynamic dispatcher(*instance, vkGetInstanceProcAddr);

		// Submitting the a callback to the messenger to delete the allocated user memory
		vk::DebugUtilsMessengerCallbackDataEXT callbackData;
		callbackData.pMessage = DELETE_TOKEN;

		instance->submitDebugUtilsMessageEXT(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
			callbackData, dispatcher);

		instance->destroyDebugUtilsMessengerEXT(handle, nullptr, dispatcher);
	}, instance->createDebugUtilsMessengerEXT(rawCreateInfo, nullptr, dispatcher));
}

VK_END
