#pragma once
#include "MemoryConfig.h"
#include "GenericBuffer.h"

VK_BEGIN

class ImageView;

// No mip-mapping support yet
class Image : public RecordableResource
{
public:
	Image() = default;

	VKLIB_API ImageView CreateImageView(const ImageViewCreateInfo& info) const;

	template <typename T>
	void CopyBufferData(const Buffer<T>& buffer);

	VKLIB_API void Blit(const Image& src, ImageBlitInfo blitInfo);

	VKLIB_API void TransitionLayout(vk::ImageLayout newLayout,
		vk::PipelineStageFlags usageStage = vk::PipelineStageFlagBits::eTopOfPipe) const;

	/* -------------------- Scoped operations ---------------------- */

	VKLIB_API virtual void BeginCommands(vk::CommandBuffer commandBuffer) const override;

	VKLIB_API void RecordBlit(const Image& src, ImageBlitInfo blitInfo, bool restoreOriginalLayout = true) const;
	VKLIB_API void RecordTransitionLayout(vk::ImageLayout newLayout, vk::PipelineStageFlags usageStage = vk::PipelineStageFlagBits::eTopOfPipe) const;

	VKLIB_API virtual void EndCommands() const override;

	/* ------------------------------------------------------------- */

	VKLIB_API void TransferOwnership(vk::QueueFlagBits Cap) const;
	VKLIB_API void TransferOwnership(uint32_t queueFamilyIndex) const;

	vk::Image GetHandle() const { return mChunk->ImageHandles.Handle; }
	const Core::ImageConfig& GetConfig() const { return mChunk->ImageHandles.Config; }
	Core::Ref<vk::Sampler> GetSampler() const { return mChunk->Sampler; }
	void SetSampler(Core::Ref<vk::Sampler> sampler) { mChunk->Sampler = sampler; }

	// Only base mipmap level is supported right now!
	VKLIB_API std::vector<vk::ImageSubresourceRange> GetSubresourceRanges() const;
	VKLIB_API std::vector<vk::ImageSubresourceLayers> GetSubresourceLayers() const;

	VKLIB_API ImageView GetIdentityImageView() const;
	VKLIB_API GenericBuffer GetImageBuffer() const;

	glm::uvec2 GetSize() const { return { mChunk->ImageHandles.Config.Extent.width, 
		mChunk->ImageHandles.Config.Extent.height }; }

	explicit operator bool() const { return static_cast<bool>(mChunk); }

private:
	Core::Ref<Core::ImageResource> mChunk;

	explicit Image(const Core::Ref<Core::ImageResource>& chunk)
		: mChunk(chunk) {}

	friend class ResourcePool;
	friend class RenderTargetContext;
	friend class Framebuffer;

	friend class Swapchain;

	friend VKLIB_API void RecordBlitImages(vk::CommandBuffer commandBuffer, Image& Dst,
		vk::ImageLayout dstLayout, const Image& Src,
		vk::ImageLayout srcLayout, ImageBlitInfo blitInfo);

	friend VKLIB_API Image Clone(Context, const Image&);
	friend VKLIB_API ImageView Clone(Context, const ImageView&);
private:
	// Helper methods...

	VKLIB_API void RecordTransitionLayoutInternal(vk::ImageLayout NewLayout, vk::PipelineStageFlags usageStage, 
		vk::ImageLayout oldLayout, vk::PipelineStageFlags oldStages,
		vk::CommandBuffer CmdBuffer, vk::QueueFlags QueueCaps) const;

	VKLIB_API void RecordBlitInternal(const ImageBlitInfo& imageBlitInfo, vk::ImageLayout dstLayout, 
		const Image& src, vk::ImageLayout srcLayout, vk::CommandBuffer CmdBuffer) const;

	VKLIB_API void CopyFromImage(Core::Image& DstImage, const Core::Image& SrcImage,
		const vk::ArrayProxy<vk::ImageCopy>& CopyRegions) const;

	VKLIB_API void CopyFromBuffer(Core::Image& DstImage, const Core::Buffer& SrcBuffer,
		const vk::ArrayProxy<vk::BufferImageCopy>& CopyRegions) const;

	VKLIB_API void CopyToBuffer(Core::Image& srcImage, const Core::Buffer& dstBuf, const vk::ArrayProxy<vk::BufferImageCopy>& copyRegions) const;

	VKLIB_API void ReleaseImage(uint32_t dstQueueFamily) const;
	VKLIB_API void AcquireImage(uint32_t dstQueueFamily) const;

	VKLIB_API ImageBlitInfo CheckBounds(const ImageBlitInfo& imageBlitInfo, const glm::uvec2& srcSize) const;

	// Make hollow instance...
	VKLIB_API void MakeHollow();
};

template <typename T>
void VK_NAMESPACE::Image::CopyBufferData(const Buffer<T>& buffer)
{
	vk::BufferImageCopy CopyRegion{};
	CopyRegion.setBufferOffset(0);
	CopyRegion.setBufferImageHeight(0);
	CopyRegion.setBufferRowLength(0);

	CopyRegion.setImageOffset({ 0, 0, 0 });
	CopyRegion.setImageExtent(mChunk->ImageHandles.Config.Extent);
	CopyRegion.setImageSubresource(GetSubresourceLayers().front());

	CopyFromBuffer(mChunk->ImageHandles, buffer.GetNativeHandles(), CopyRegion);
}

VKLIB_API void RecordBlitImages(vk::CommandBuffer commandBuffer, Image& Dst,
	vk::ImageLayout dstLayout, const Image& Src, 
	vk::ImageLayout srcLayout, ImageBlitInfo blitInfo);

VK_END
