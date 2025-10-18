#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/FactoryConfig.h"

AQUA_BEGIN

using TagImageAttachmentMap = std::unordered_map<std::string, TagAttributes>;
using ImageAttachmentTagMap = std::unordered_map<vk::Format, std::string>;

// Shift the depth by 24, and the rest will go into the 8 bit slot
#define MAKE_DEPTH_STENCIL_SIZE(x, y)          (x << 8 | y & (uint8_t)(~0))
#define GET_DEPTH_SIZE(size)                   (size >> 8)
#define GET_STENCIL_SIZE(size)                 (size & (uint8_t)(~0))

static const TagImageAttachmentMap sTypeToFormat =
{
	{"r8i", { vk::Format::eR8Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 1}},
	{"rg8i", { vk::Format::eR8G8Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 2}},
	{"rgb8i", { vk::Format::eR8G8B8Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 3}},
	{"rgba8i", { vk::Format::eR8G8B8A8Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},
	{"r8un", { vk::Format::eR8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 1}},
	{"rg8un", { vk::Format::eR8G8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 2}},
	{"rgb8un", { vk::Format::eR8G8B8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 3}},
	{"rgba8un", { vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},

	{"r16i", {vk::Format::eR16Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 2}},
	{"rg16i", {vk::Format::eR16G16Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 8}},
	{"rgb16i", {vk::Format::eR16G16B16Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 16}},
	{"rgba16i", {vk::Format::eR16G16B16A16Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 32}},
	{"r16f", {vk::Format::eR16Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 2}},
	{"rg16f", {vk::Format::eR16G16Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},
	{"rgb16f", {vk::Format::eR16G16B16Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 6}},
	{"rgba16f", {vk::Format::eR16G16B16A16Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 8}},
	{"r16un", {vk::Format::eR16Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 2}},
	{"rg16un", {vk::Format::eR16G16Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},
	{"rgb16un", {vk::Format::eR16G16B16Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 6}},
	{"rgba16un", {vk::Format::eR16G16B16A16Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 8}},

	{"r32i", {vk::Format::eR32Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},
	{"rg32i", {vk::Format::eR32G32Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 8}},
	{"rgb32i", {vk::Format::eR32G32B32Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 12}},
	{"rgba32i", {vk::Format::eR32G32B32A32Sint, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 16}},
	{"r32f", {vk::Format::eR32Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 4}},
	{"rg32f", {vk::Format::eR32G32Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 8}},
	{"rgb32f", {vk::Format::eR32G32B32Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 12}},
	{"rgba32f", {vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageUsageFlagBits::eColorAttachment, 16}},

	// First 24 bits to the size will represent depth size, and the rest will represent stencil
	{"d16un", {vk::Format::eD16Unorm, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(2, 0)}},
	{"x8d24un_pack32", {vk::Format::eX8D24UnormPack32, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(3, 1)}},
	{"d32f", {vk::Format::eD32Sfloat, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(4, 0)}},
	{"d16un_s8u", {vk::Format::eD16UnormS8Uint, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(2, 1)}},
	{"d24un_s8u", {vk::Format::eD24UnormS8Uint, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(3, 1)}},
	{"d32f_s8u", {vk::Format::eD32SfloatS8Uint, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(4, 1)}},
	{"s8u", {vk::Format::eS8Uint, vk::ImageLayout::eStencilAttachmentOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, MAKE_DEPTH_STENCIL_SIZE(0, 1)} },
};

ImageAttachmentTagMap CreateInverseMap(const TagImageAttachmentMap& map)
{
	ImageAttachmentTagMap ret;
	for (const TagImageAttachmentMap::value_type& value : map)
		ret[std::get<0>(value.second)] = value.first;
	return ret;
}

std::string ToLowerCase(const std::string& str)
{
	std::string lowered = str;

	std::for_each(lowered.begin(), lowered.end(), [](char& c)
		{
			c = std::tolower(c);
		});

	return lowered;
}

static const ImageAttachmentTagMap sFormatToString = CreateInverseMap(sTypeToFormat);

AQUA_END

AQUA_NAMESPACE::TagAttributes AQUA_NAMESPACE::ConvertIntoTagAttributes(const std::string& tag)
{
	std::string rinsedName = ToLowerCase(tag);

	_STL_ASSERT(sTypeToFormat.find(rinsedName) != sTypeToFormat.end(), "invalid attribute format");

	return sTypeToFormat.at(rinsedName);
}

std::string AQUA_NAMESPACE::GetImageAttachmentInfoString(vk::Format format)
{
	return sFormatToString.at(format);
}
