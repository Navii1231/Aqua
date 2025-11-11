#include "Core/vkpch.h"
#include "Core/Utils/MemoryUtils.h"

VK_BEGIN
struct MemoryUtilsHelper {
	std::unordered_map<vk::ImageLayout, vk::AccessFlags> mAccessFlagsByImageLayout;
	std::unordered_map<vk::PipelineStageFlagBits, vk::AccessFlags> mAccessFlagsByPipelineStage;
	std::unordered_map<vk::QueueFlagBits, vk::AccessFlags> mQueueFlagsAccessFlags;
	std::unordered_map<vk::Format, vk::ImageAspectFlags> mFormatToAspectFlags;

	// Constructor to initialize the mappings
	MemoryUtilsHelper() {
		mAccessFlagsByImageLayout = {
			{vk::ImageLayout::eUndefined, {}},
			{vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},
			{vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite},
			{vk::ImageLayout::eDepthAttachmentOptimal, vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite},
			{vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite},
			{vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal, vk::AccessFlagBits::eDepthStencilAttachmentRead},
			{vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eShaderRead},
			{vk::ImageLayout::eReadOnlyOptimal, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentRead },
			{vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferRead},
			{vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite},
			{vk::ImageLayout::ePreinitialized, vk::AccessFlagBits::eHostWrite},
			{vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eMemoryRead},
			{vk::ImageLayout::eSharedPresentKHR, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},
		};

		mAccessFlagsByPipelineStage = {
			{vk::PipelineStageFlagBits::eTopOfPipe, {}},
			{vk::PipelineStageFlagBits::eDrawIndirect, vk::AccessFlagBits::eIndirectCommandRead},
			{vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eIndexRead | vk::AccessFlagBits::eVertexAttributeRead},
			{vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead},
			{vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead},
			{vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentRead},
			{vk::PipelineStageFlagBits::eLateFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite},
			{vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite},
			{vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite},
			{vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite},
			{vk::PipelineStageFlagBits::eBottomOfPipe, {}},
			{vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite},
		};

		mQueueFlagsAccessFlags = {
			{vk::QueueFlagBits::eGraphics,
				vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eIndexRead | vk::AccessFlagBits::eVertexAttributeRead |
				vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
				vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite |
				vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite |
				vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite |
				vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},

			{vk::QueueFlagBits::eCompute,
				vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eUniformRead |
				vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
				vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite |
				vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},

			{vk::QueueFlagBits::eTransfer,
				vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite |
				vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},

			{vk::QueueFlagBits::eSparseBinding,
				vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite},
		};

		mFormatToAspectFlags = {
			{vk::Format::eD16Unorm, vk::ImageAspectFlagBits::eDepth},
			{vk::Format::eX8D24UnormPack32, vk::ImageAspectFlagBits::eDepth},
			{vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth},
			{vk::Format::eS8Uint, vk::ImageAspectFlagBits::eStencil},
			{vk::Format::eD16UnormS8Uint, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil},
			{vk::Format::eD24UnormS8Uint, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil},
			{vk::Format::eD32SfloatS8Uint, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil},
		};
	}

	// Function to get access flags for a given image layout
	vk::AccessFlags GetAccessFlagsForLayout(vk::ImageLayout layout) const 
	{ return mAccessFlagsByImageLayout.at(layout); }

	// Function to get access flags for a given pipeline stage
	vk::AccessFlags GetAccessFlagsForStage(vk::PipelineStageFlags stages) const 
	{ 
		vk::AccessFlags accessFlags{};
		for (auto& pair : mAccessFlagsByPipelineStage) 
		{
			if (stages & pair.first) {
				accessFlags |= pair.second;
			}
		}
		return accessFlags;
	}

	vk::AccessFlags GetAccessFlagsForQueueFlags(vk::QueueFlags queueFlags) const
	{
		vk::AccessFlags accessFlags(0);
		for (const auto& entry : mQueueFlagsAccessFlags) 
		{
			if ((vk::QueueFlags)queueFlags & entry.first) 
				accessFlags |= entry.second;
		}

		return accessFlags;
	}

	vk::ImageAspectFlags GetImageAspectFlagsForFormat(vk::Format format) const
	{
		auto found = mFormatToAspectFlags.find(format);

		if (found == mFormatToAspectFlags.end())
			return vk::ImageAspectFlagBits::eColor;

		return mFormatToAspectFlags.at(format);
	}

} static const sMemoryHelper;

VK_END

uint32_t VK_NAMESPACE::VK_CORE::VK_UTILS::FindMemoryTypeIndex(vk::PhysicalDevice device, uint32_t memTypeBits, vk::MemoryPropertyFlags memProps)
{
	auto deviceMemProps = device.getMemoryProperties();

	for (uint32_t i = 0; i < deviceMemProps.memoryTypeCount; i++)
	{
		bool supported = memTypeBits & (1 << i);
		bool sufficient = (deviceMemProps.memoryTypes[i].propertyFlags & memProps) == memProps;

		if (supported && sufficient)
			return i;
	}

	return -1;
}

VK_NAMESPACE::VK_CORE::Buffer VK_NAMESPACE::VK_CORE::VK_UTILS::CreateBuffer(BufferConfig& bufferInput)
{
	vk::BufferCreateInfo bufferInfo;
	bufferInfo.setSharingMode(vk::SharingMode::eExclusive);
	bufferInfo.setSize(bufferInput.DeviceSize);
	bufferInfo.setUsage(bufferInput.Usage);
	bufferInfo.setQueueFamilyIndices(bufferInput.ResourceOwner);

	vk::Buffer Handle = bufferInput.LogicalDevice.createBuffer(bufferInfo);
	auto memReq = bufferInput.LogicalDevice.getBufferMemoryRequirements(Handle);

	//bufferInput.ElemCount = memReq.size / bufferInput.TypeSize;

	auto Memory = AllocateMemory(memReq, bufferInput.MemProps, 
		bufferInput.LogicalDevice, bufferInput.PhysicalDevice);

	bufferInput.LogicalDevice.bindBufferMemory(Handle, Memory, 0);

	return { Handle, Memory, memReq, 0, bufferInput };
}

vk::DeviceMemory VK_NAMESPACE::VK_CORE::VK_UTILS::AllocateMemory(
	const vk::MemoryRequirements& memReq, vk::MemoryPropertyFlags props,
	vk::Device logicalDevice, vk::PhysicalDevice physicalDevice)
{
	vk::MemoryAllocateInfo allocInfo{};
	allocInfo.setAllocationSize(memReq.size);
	allocInfo.setMemoryTypeIndex(FindMemoryTypeIndex(physicalDevice,
		memReq.memoryTypeBits, props));

	return logicalDevice.allocateMemory(allocInfo);
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::RecordBufferTransferBarrier(const BufferOwnershipTransferInfo& barrierInfo)
{
	vk::BufferMemoryBarrier barrier{};
	barrier.setBuffer(barrierInfo.Buffer.Handle);
	barrier.setSrcQueueFamilyIndex(barrierInfo.SrcFamilyIndex);
	barrier.setDstQueueFamilyIndex(barrierInfo.DstFamilyIndex);
	barrier.setOffset(0);
	barrier.setSize(VK_WHOLE_SIZE);
	barrier.setSrcAccessMask(barrierInfo.SrcAccess);
	barrier.setDstAccessMask(barrierInfo.DstAccess);

	barrierInfo.CmdBuffer.pipelineBarrier(
		barrierInfo.SrcStage,
		barrierInfo.DstStage,
		{}, {}, barrier, {});
}

vk::AccessFlags VK_NAMESPACE::VK_CORE::VK_UTILS::GetAllBufferAccessFlags(vk::QueueFlagBits flag)
{
	// TODO: Implement empty flags

	switch (flag)
	{
		case vk::QueueFlagBits::eGraphics:
			return vk::AccessFlagBits::eIndexRead | vk::AccessFlagBits::eVertexAttributeRead |
				vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
				vk::AccessFlagBits::eUniformRead;
		case vk::QueueFlagBits::eCompute:
			return vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite |
				vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
				vk::AccessFlagBits::eUniformRead;
		case vk::QueueFlagBits::eTransfer:
			return vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite |
				vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
		case vk::QueueFlagBits::eSparseBinding:
			return vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
			// Not really using these queue abilities as for now
		case vk::QueueFlagBits::eProtected:
			return vk::AccessFlagBits();
		case vk::QueueFlagBits::eVideoDecodeKHR:
			return vk::AccessFlagBits();
		case vk::QueueFlagBits::eVideoEncodeKHR:
			return vk::AccessFlagBits();
		case vk::QueueFlagBits::eOpticalFlowNV:
			return vk::AccessFlagBits();
		default:
			return vk::AccessFlagBits();
	}
}

vk::PipelineStageFlags VK_NAMESPACE::VK_CORE::VK_UTILS::GetAllBufferStageFlags(vk::QueueFlagBits flag)
{
	switch (flag)
	{
		case vk::QueueFlagBits::eGraphics:
			return vk::PipelineStageFlagBits::eTopOfPipe | vk::PipelineStageFlagBits::eVertexInput
				| vk::PipelineStageFlagBits::eBottomOfPipe;
		case vk::QueueFlagBits::eCompute:
			return vk::PipelineStageFlagBits::eComputeShader;
		case vk::QueueFlagBits::eTransfer:
			return vk::PipelineStageFlagBits::eTransfer;
		case vk::QueueFlagBits::eSparseBinding:
			return vk::PipelineStageFlagBits::eTransfer;
		// Not really using these queue abilities as for now
		case vk::QueueFlagBits::eProtected:
			return vk::PipelineStageFlagBits();
		case vk::QueueFlagBits::eVideoDecodeKHR:
			return vk::PipelineStageFlagBits();
		case vk::QueueFlagBits::eVideoEncodeKHR:
			return vk::PipelineStageFlagBits();
		case vk::QueueFlagBits::eOpticalFlowNV:
			return vk::PipelineStageFlagBits();
		default:
			return vk::PipelineStageFlagBits();
	}
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::FillBufferReleaseMasks(
	BufferOwnershipTransferInfo& Info, vk::QueueFlags SrcFlags)
{
	auto SrcCapBits = BreakIntoIndividualFlagBits(SrcFlags);

	vk::AccessFlags SrcAccessMask;

	vk::PipelineStageFlags SrcStageMask;

	for (auto SrcCapBit : SrcCapBits)
	{
		SrcAccessMask |= GetAllBufferAccessFlags(SrcCapBit);
		SrcStageMask |= GetAllBufferStageFlags(SrcCapBit);
	}

	Info.SrcAccess = SrcAccessMask;
	Info.SrcStage = SrcStageMask;
	Info.DstAccess = vk::AccessFlagBits();
	Info.DstStage = vk::PipelineStageFlagBits::eBottomOfPipe;
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::FillBufferAcquireMasks(
	BufferOwnershipTransferInfo& Info, vk::QueueFlags DstFlags)
{
	auto DstCapBits = BreakIntoIndividualFlagBits(DstFlags);

	vk::AccessFlags DstAccessMask;
	vk::PipelineStageFlags DstStageMask;

	for (auto DstCapBit : DstCapBits)
	{
		DstAccessMask |= GetAllBufferAccessFlags(DstCapBit);
		DstStageMask |= GetAllBufferStageFlags(DstCapBit);
	}

	Info.DstAccess = DstAccessMask;
	Info.DstStage = DstStageMask;
	Info.SrcAccess = vk::AccessFlagBits();
	Info.SrcStage = vk::PipelineStageFlagBits::eTopOfPipe;
}

VK_NAMESPACE::VK_CORE::Image VK_NAMESPACE::VK_CORE::VK_UTILS::CreateImage(ImageConfig& config)
{
	vk::ImageCreateInfo createInfo{};
	createInfo.setExtent(config.Extent);
	createInfo.setFormat(config.Format);
	createInfo.setImageType(config.Type);
	createInfo.setTiling(config.Tiling);
	createInfo.setUsage(config.Usage);
	createInfo.setArrayLayers(1);
	createInfo.setMipLevels(1);
	createInfo.setSamples(vk::SampleCountFlagBits::e1);
	createInfo.setInitialLayout(config.CurrLayout);
	createInfo.setSharingMode(vk::SharingMode::eExclusive);

	vk::Image image = config.LogicalDevice.createImage(createInfo);
	vk::MemoryRequirements memreq = config.LogicalDevice.getImageMemoryRequirements(image);

	vk::DeviceMemory Memory = AllocateMemory(memreq, config.MemProps,
		config.LogicalDevice, config.PhysicalDevice);

	config.LogicalDevice.bindImageMemory(image, Memory, 0);

	ImageViewCreateInfo viewInfo{};
	viewInfo.Format = config.Format;
	viewInfo.ComponentMaps =
	{
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity
	};

	viewInfo.Subresource.aspectMask = sMemoryHelper.GetImageAspectFlagsForFormat(config.Format);
	viewInfo.Subresource.setBaseArrayLayer(0);
	viewInfo.Subresource.setBaseMipLevel(0);
	viewInfo.Subresource.setLevelCount(1);
	viewInfo.Subresource.setLayerCount(1);

	viewInfo.Type = config.ViewType;

	auto ViewHandle = CreateImageView(config.LogicalDevice, image, viewInfo);

	return { image, Memory, memreq, config, ViewHandle, viewInfo };
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::RecordImageLayoutTransition(const ImageLayoutTransitionInfo& transitionInfo)
{
	vk::ImageMemoryBarrier barrier;

	barrier.setImage(transitionInfo.Handles.Handle);
	barrier.setOldLayout(transitionInfo.OldLayout);
	barrier.setNewLayout(transitionInfo.NewLayout);
	barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
	barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
	barrier.setSrcAccessMask(transitionInfo.SrcAccessFlags);
	barrier.setDstAccessMask(transitionInfo.DstAccessFlags);
	barrier.setSubresourceRange(transitionInfo.Subresource);

	transitionInfo.CmdBuffer.pipelineBarrier(
		transitionInfo.SrcStageFlags,
		transitionInfo.DstStageFlags,
		vk::DependencyFlagBits(),
		nullptr, nullptr,
		barrier
	);
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::RecordImageLayoutTransition(
	const ImageLayoutTransitionInfo& transitionInfo, uint32_t srcQueueFamily, uint32_t dstQueueFamily)
{
	vk::ImageMemoryBarrier barrier;

	barrier.setImage(transitionInfo.Handles.Handle);
	barrier.setOldLayout(transitionInfo.OldLayout);
	barrier.setNewLayout(transitionInfo.NewLayout);
	barrier.setSrcQueueFamilyIndex(srcQueueFamily);
	barrier.setDstQueueFamilyIndex(dstQueueFamily);
	barrier.setSrcAccessMask(transitionInfo.SrcAccessFlags);
	barrier.setDstAccessMask(transitionInfo.DstAccessFlags);
	barrier.setSubresourceRange(transitionInfo.Subresource);

	transitionInfo.CmdBuffer.pipelineBarrier(
		transitionInfo.SrcStageFlags,
		transitionInfo.DstStageFlags,
		vk::DependencyFlagBits(),
		nullptr, nullptr,
		barrier
	);
}

vk::ImageView VK_NAMESPACE::VK_CORE::VK_UTILS::CreateImageView(
	vk::Device device, vk::Image image, const ImageViewCreateInfo& info)
{
	vk::ImageViewCreateInfo createInfo{};
	createInfo.setImage(image);
	createInfo.setViewType(info.Type);
	createInfo.setFormat(info.Format);
	createInfo.setComponents(info.ComponentMaps);
	createInfo.setSubresourceRange(info.Subresource);

	return device.createImageView(createInfo);
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::FillImageTransitionLayoutMasks(
	ImageLayoutTransitionInfo& Info, vk::QueueFlags flags, vk::ImageLayout initLayout, 
	vk::ImageLayout finalLayout, vk::PipelineStageFlags SrcFlags, vk::PipelineStageFlags DstFlags)
{
	auto queueFamiltFlags = sMemoryHelper.GetAccessFlagsForQueueFlags(flags);

	Info.SrcAccessFlags |= (sMemoryHelper.GetAccessFlagsForLayout(initLayout) &
		sMemoryHelper.GetAccessFlagsForStage(SrcFlags)) & queueFamiltFlags;
	
	Info.DstAccessFlags |= (sMemoryHelper.GetAccessFlagsForLayout(finalLayout) &
		sMemoryHelper.GetAccessFlagsForStage(DstFlags)) & queueFamiltFlags;
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::FillImageReleaseMasks(ImageLayoutTransitionInfo& info, vk::QueueFlags flags, 
	vk::ImageLayout layout, vk::PipelineStageFlags SrcFlags)
{
	info.NewLayout = vk::ImageLayout::eTransferDstOptimal;
	FillImageTransitionLayoutMasks(info, flags, layout, info.NewLayout, 
		SrcFlags, vk::PipelineStageFlagBits::eTransfer);
}

void VK_NAMESPACE::VK_CORE::VK_UTILS::FillImageAcquireMasks(
	ImageLayoutTransitionInfo& info, vk::QueueFlags flags, vk::ImageLayout layout, vk::PipelineStageFlags DstFlags)
{
	info.OldLayout = vk::ImageLayout::eTransferDstOptimal;
	FillImageTransitionLayoutMasks(info, flags, info.OldLayout, layout, 
		vk::PipelineStageFlagBits::eTransfer, DstFlags);
}

