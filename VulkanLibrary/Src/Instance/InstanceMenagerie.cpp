#include "Core/vkpch.h"
#include "Instance/InstanceMenagerie.h"

VK_NAMESPACE::InstanceMenagerie::InstanceMenagerie(const std::vector<const char*>& extensions, const std::vector<const char*>& layers) : mExtensions(extensions), mLayers(layers), mAPI_Version(vk::enumerateInstanceVersion())
{
	auto SupportedExtensions = vk::enumerateInstanceExtensionProperties();
	auto SupportedLayers = vk::enumerateInstanceLayerProperties();

	TryAddingWindowSurfaceExtensions(SupportedExtensions);

	auto UnsupportedExtensions = GetCompliment(SupportedExtensions, mExtensions,
		[](const vk::ExtensionProperties& extension)
		{
			return extension.extensionName;
		});

	auto UnsupportedLayers = GetCompliment(SupportedLayers, mLayers,
		[](const vk::LayerProperties& extension)
		{
			return extension.layerName;
		});

	if (!UnsupportedLayers.empty() || !UnsupportedExtensions.empty())
		throw UnsupportedLayersAndExtensions(UnsupportedExtensions, UnsupportedLayers);
}

void VK_NAMESPACE::InstanceMenagerie::TryAddingWindowSurfaceExtensions(const std::vector<vk::ExtensionProperties>& AllExtensions)
{
	if (!glfwVulkanSupported())
		return;

	uint32_t ExtCount;
	auto SurfaceExts = glfwGetRequiredInstanceExtensions(&ExtCount);

	auto NoSupport = GetCompliment(
		AllExtensions,
		std::vector<const char*>(SurfaceExts, SurfaceExts + ExtCount),
		[](const vk::ExtensionProperties& props) { return props.extensionName;
		});

	mSurfaceSupported = NoSupport.empty();

	if (!mSurfaceSupported)
		return;

	mExtensions.insert(mExtensions.end(), SurfaceExts, SurfaceExts + ExtCount);
}

VK_NAMESPACE::Core::Ref<vk::Instance> VK_NAMESPACE::InstanceMenagerie::Create(const InstanceCreateInfo& info) const
{
	vk::ApplicationInfo appInfo{};

	appInfo.apiVersion = mAPI_Version;
	appInfo.applicationVersion = VK_MAKE_VERSION(
		info.AppVersion.Major, info.AppVersion.Minor, info.AppVersion.Patch);
	appInfo.engineVersion = VK_MAKE_VERSION(
		info.EngineVersion.Major, info.EngineVersion.Minor, info.EngineVersion.Patch);
	appInfo.pApplicationName = info.AppName.c_str();
	appInfo.pEngineName = info.EngineName.c_str();

	vk::InstanceCreateInfo createInfo{};

	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledExtensionNames = mExtensions.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(mExtensions.size());
	createInfo.ppEnabledLayerNames = mLayers.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(mLayers.size());

	return vkLib::Core::CreateRef<vk::Instance>([](vk::Instance& handle)
		{
			handle.destroy();
		}, vk::createInstance(createInfo));
}

std::vector<std::string> VK_NAMESPACE::InstanceMenagerie::GetAllLayerNames()
{
	auto SupportedLayers = vk::enumerateInstanceLayerProperties();

	std::vector<std::string> LayerNames;

	for (auto& prop : SupportedLayers)
		LayerNames.emplace_back(prop.layerName.data());

	return LayerNames;
}

std::vector<std::string> VK_NAMESPACE::InstanceMenagerie::GetAllExtensionNames()
{
	auto SupportedExtensions = vk::enumerateInstanceExtensionProperties();

	std::vector<std::string> ExtensionNames;

	for (auto& prop : SupportedExtensions)
		ExtensionNames.emplace_back(prop.extensionName.data());

	return ExtensionNames;
}

vk::ResultValue<VK_NAMESPACE::Core::Ref<vk::SurfaceKHR>> VK_NAMESPACE::InstanceMenagerie::CreateSurface(Core::Ref<vk::Instance> instance, GLFWwindow* context)
{
	if (!mSurfaceSupported)
		return { vk::Result::eErrorExtensionNotPresent, Core::CreateRef<vk::SurfaceKHR>([](vk::SurfaceKHR&) {}, vk::SurfaceKHR()) };

	VkSurfaceKHR c_surface;

	vk::Result result = vk::Result(glfwCreateWindowSurface(*instance, context, nullptr, &c_surface));

	return { result, Core::CreateRef<vk::SurfaceKHR>([instance](vk::SurfaceKHR& surface)
	{ instance->destroySurfaceKHR(surface); }, c_surface) };
}
