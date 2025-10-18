#pragma once
#include "../../Core/AqCore.h"
#include "../../Core/SharedRef.h"

AQUA_BEGIN

// Syntax to specify an attachment --> [@/$][name]_[type]
// Given that the vkLib::Framebuffer separates out the color attachments
// from the depth attachments, all the depth tags must appear after the color attachments

#define ENTRY_POSITION            "position"
#define ENTRY_NORMAL              "normal"
#define ENTRY_TANGENT             "tangent"
#define ENTRY_BITANGENT           "bitangent"
#define ENTRY_TANGENT_SPACE       "tangent_space"
#define ENTRY_TEXCOORDS           "texcoords"
#define ENTRY_METADATA            "metadata"

enum class VertexError
{
	eDuplicateVertexLocations         = 1,
	eDuplicateBindingNames            = 2,
	eBindingDoesntExist               = 3,
};

struct VertexAttribute
{
	uint32_t Location = 0;
	std::string Format;

	VertexAttribute(uint32_t location = 0, const std::string& format = "")
		: Location(location), Format(format) {}
};

struct ImageAttribute
{
	std::string Name;
	std::string Format;

	ImageAttribute(const std::string& name = "", const std::string& format = "")
		: Name(name), Format(format) {}

	void SetName(const std::string& name) { Name = name; }
	void SetFormat(const std::string& format) { Format = format; }
};

using VertexBindingAttributes = std::vector<VertexAttribute>;

struct VertexBinding
{
	std::string Name;
	VertexBindingAttributes Attributes;
	vk::VertexInputRate InputRate = vk::VertexInputRate::eVertex;

	void SetName(const std::string& name) { Name = name; }
	void AddAttribute(uint32_t location, const std::string& format) { Attributes.emplace_back(location, format); }
	void SetInputRate(vk::VertexInputRate rate) { InputRate = rate; }
};

struct VertexBindingProperties
{
	uint32_t BindingIdx;
	vk::BufferUsageFlags Usage;
	vk::MemoryPropertyFlags MemProps;

	VertexBindingProperties() = default;

	void SetBindingIdx(uint32_t val) { BindingIdx = val; }
	void SetBufferUsage(vk::BufferUsageFlags usage) { Usage = usage; }
	void SetMemoryPropertyFlags(vk::MemoryPropertyFlags memProps) { MemProps = memProps; }
};

struct ImageAttributeProperties
{
	uint32_t AttachIdx;
	vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eClear;
	vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eStore;
	vk::SampleCountFlagBits Samples = vk::SampleCountFlagBits::e1;
	vk::AttachmentLoadOp StencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	vk::AttachmentStoreOp StencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	vk::ImageLayout Layout = vk::ImageLayout::eColorAttachmentOptimal;
	vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eColorAttachment;
	vk::MemoryPropertyFlags MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	void SetAttachIdx(uint32_t val) { AttachIdx = val; }
	void SetStencilLoadOp(vk::AttachmentLoadOp val) { StencilLoadOp = val; }
	void SetStencilStoreOp(vk::AttachmentStoreOp val) { StencilStoreOp = val; }
	void SetLayout(vk::ImageLayout val) { Layout = val; }
	void SetUsage(vk::ImageUsageFlags val) { Usage = val; }
	void SetLoadOp(vk::AttachmentLoadOp loadOp) { LoadOp = loadOp; }
	void SetStoreOp(vk::AttachmentStoreOp storeOp) { StoreOp = storeOp; }
	void SetSamples(vk::SampleCountFlagBits samples) { Samples = samples; }
	void SetMemProps(vk::MemoryPropertyFlags val) { MemProps = val; }
};

// Either the vertex binding map is mapped by name or the vertex resource map, and the other one by the binding index
// I don't know yet which one will be better or more convenient in the future
using VertexBindingMap = std::unordered_map<uint32_t, VertexBinding>;
using VertexBindingPropertiesMap = std::unordered_map<std::string, VertexBindingProperties>;
using VertexResourceMap = std::unordered_map<std::string, vkLib::GenericBuffer>;

using ImageAttributeList = std::vector<ImageAttribute>;
using ImageAttributePropertiesMap = std::unordered_map<std::string, ImageAttributeProperties>;
using ImageResourceMap = std::unordered_map<std::string, vkLib::Image>;

using ImageViewList = std::vector<vkLib::ImageView>;

using TagAttributes = std::tuple<vk::Format, vk::ImageLayout, vk::ImageUsageFlagBits, uint32_t>;

TagAttributes ConvertIntoTagAttributes(const std::string&);
uint32_t GetVertexSize(const std::string);
std::string GetImageAttachmentInfoString(vk::Format);

AQUA_END
