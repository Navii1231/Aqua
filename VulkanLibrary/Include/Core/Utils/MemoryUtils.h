#pragma once
#include "../Config.h"
#include "DeviceCreation.h"

VK_BEGIN

struct ImageLayoutInfo
{
	vk::ImageLayout Layout = vk::ImageLayout::eUndefined;
	vk::PipelineStageFlags Stages = vk::PipelineStageFlagBits::eTopOfPipe;
};

struct ImageViewCreateInfo
{
	vk::ImageViewType Type;
	vk::Format Format;
	vk::ComponentMapping ComponentMaps;
	vk::ImageSubresourceRange Subresource;

	bool operator ==(const ImageViewCreateInfo& Other) const
	{
		return Type == Other.Type && Format == Other.Format &&
			ComponentMaps == Other.ComponentMaps && Subresource == Other.Subresource;
	}

	bool operator !=(const ImageViewCreateInfo& Other) const
	{
		return !(*this == Other);
	}
};

VK_CORE_BEGIN

// Buffer structs
struct BufferConfig
{
	vk::Device LogicalDevice;
	vk::PhysicalDevice PhysicalDevice;

	mutable uint32_t ResourceOwner = 0;

	vk::BufferUsageFlags Usage = vk::BufferUsageFlagBits::eTransferDst | 
		vk::BufferUsageFlagBits::eTransferSrc;

	vk::DeviceSize ElemCount = 0;
	vk::DeviceSize TypeSize = 0;
	vk::MemoryPropertyFlags MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	void SetProperty(vk::BufferUsageFlags flags)
	{ Usage = flags | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc; }

	void SetProperty(size_t elemCount) { ElemCount = elemCount; }
	void SetProperty(vk::MemoryPropertyFlags flags) { MemProps = flags; }
};

struct Buffer
{
	vk::Buffer Handle{};
	vk::DeviceMemory Memory{};
	vk::MemoryRequirements MemReq{};

	size_t ElemCount = 0;

	BufferConfig Config{};
};

struct BufferOwnershipTransferInfo
{
	vk::CommandBuffer CmdBuffer;
	Core::Buffer Buffer;
	vk::AccessFlags SrcAccess;
	vk::AccessFlags DstAccess;
	vk::PipelineStageFlags SrcStage;
	vk::PipelineStageFlags DstStage;

	uint32_t SrcFamilyIndex = static_cast<uint32_t>(-1);
	uint32_t DstFamilyIndex = static_cast<uint32_t>(-1);
};

// Image structs

struct ImageConfig
{
	vk::Device LogicalDevice;
	vk::PhysicalDevice PhysicalDevice;

	mutable uint32_t ResourceOwner = 0;

	vk::ImageType Type = vk::ImageType::e2D;
	vk::ImageViewType ViewType = vk::ImageViewType::e2D;

	vk::Format Format = vk::Format::eR8G8B8A8Unorm;
	vk::Extent3D Extent = { 0, 0, 0 };
	vk::ImageTiling Tiling = vk::ImageTiling::eOptimal;
	vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eColorAttachment;

	mutable vk::ImageLayout CurrLayout = vk::ImageLayout::eUndefined;
	mutable vk::PipelineStageFlags PrevStage = vk::PipelineStageFlagBits::eTopOfPipe;

	vk::MemoryPropertyFlags MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

};

struct Image
{
	vk::Image Handle;
	vk::DeviceMemory Memory;
	vk::MemoryRequirements MemReq;

	ImageConfig Config;

	vk::ImageView IdentityView;
	ImageViewCreateInfo IdentityViewInfo;

	// Keeps track of the layout transitions during scoped operations
	mutable ImageLayoutInfo RecordedLayout;
};

struct ImageLayoutTransitionInfo
{
	const Image& Handles;

	vk::ImageLayout OldLayout;
	vk::ImageLayout NewLayout;

	vk::AccessFlags SrcAccessFlags;
	vk::AccessFlags DstAccessFlags;

	vk::PipelineStageFlags SrcStageFlags;
	vk::PipelineStageFlags DstStageFlags;

	vk::ImageSubresourceRange Subresource;

	vk::CommandBuffer CmdBuffer;
};

VK_UTILS_BEGIN

VKLIB_API uint32_t FindMemoryTypeIndex(vk::PhysicalDevice device,
	uint32_t memTypeBits, vk::MemoryPropertyFlags memProps);

VKLIB_API vk::DeviceMemory AllocateMemory(const vk::MemoryRequirements& memReq, vk::MemoryPropertyFlags props,
	vk::Device logicalDevice, vk::PhysicalDevice physicalDevice);

// Buffer functionality

VKLIB_API Buffer CreateBuffer(BufferConfig& bufferInput);

VKLIB_API void RecordBufferTransferBarrier(const BufferOwnershipTransferInfo& barrierInfo);

VKLIB_API vk::AccessFlags GetAllBufferAccessFlags(vk::QueueFlagBits flag);
VKLIB_API vk::PipelineStageFlags GetAllBufferStageFlags(vk::QueueFlagBits flag);

VKLIB_API void FillBufferReleaseMasks(BufferOwnershipTransferInfo& Info, vk::QueueFlags SrcFlags);
VKLIB_API void FillBufferAcquireMasks(BufferOwnershipTransferInfo& Info, vk::QueueFlags DstFlags);

// Image functionality

VKLIB_API Image CreateImage(ImageConfig& config);

VKLIB_API void RecordImageLayoutTransition(const ImageLayoutTransitionInfo& transitionInfo);
VKLIB_API void RecordImageLayoutTransition(const ImageLayoutTransitionInfo& transitionInfo,
	uint32_t srcQueueFamily, uint32_t dstQueueFamily);

// Image views
VKLIB_API vk::ImageView CreateImageView(vk::Device device, vk::Image image, const ImageViewCreateInfo& info);

VKLIB_API void FillImageTransitionLayoutMasks(ImageLayoutTransitionInfo& Info, vk::QueueFlags flags, vk::ImageLayout initLayout, vk::ImageLayout finalLayout,
	vk::PipelineStageFlags SrcFlags, vk::PipelineStageFlags DstFlags);

VKLIB_API void FillImageReleaseMasks(ImageLayoutTransitionInfo& info, vk::QueueFlags flags, vk::ImageLayout layout, vk::PipelineStageFlags SrcFlags);
VKLIB_API void FillImageAcquireMasks(ImageLayoutTransitionInfo& info, vk::QueueFlags flags, vk::ImageLayout layout, vk::PipelineStageFlags DstFlags);

VK_UTILS_END
VK_CORE_END
VK_END

