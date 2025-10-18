#pragma once
#include "../Core/Config.h"

VK_BEGIN

template <typename Property, typename PropNameFn>
std::set<std::string> GetCompliment(const std::vector<Property>& properties,
	const std::vector<const char*>& propertyNames, PropNameFn getNameFn)
{
	std::set<std::string> Remaining{ propertyNames.begin(), propertyNames.end() };

	std::for_each(properties.begin(), properties.end(), [&Remaining, getNameFn](const Property& prop)
	{
		Remaining.erase(getNameFn(prop));
	});

	return Remaining;
}

class UnsupportedLayersAndExtensions : public std::exception
{
public:
	UnsupportedLayersAndExtensions(const std::set<std::string>& unsupportedExtensions, 
		const std::set<std::string>& unsupportedLayers)
		: std::exception("All layers and extensions are not supported!"),
		mUnsupportedExtensions(unsupportedExtensions), mUnsupportedLayers(unsupportedLayers) {}

	const std::set<std::string>& GetUnsupportedExtensions() const { return mUnsupportedExtensions; }
	const std::set<std::string>& GetUnsupportedLayers() const { return mUnsupportedLayers; }
private:

	const std::set<std::string> mUnsupportedExtensions;
	const std::set<std::string> mUnsupportedLayers;
};

class UnsupportedQueue : public std::exception
{
public:
	UnsupportedQueue(vk::QueueFlags flags)
		: std::exception("Some of the requested queues are unsupported!"), mUnsupportedFlags(flags) {}

	vk::QueueFlags GetUnsupportedFlags() const { return mUnsupportedFlags; }
private:
	vk::QueueFlags mUnsupportedFlags;
};

VK_END
