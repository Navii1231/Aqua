#include "Core/vkpch.h"
#include "Device/Context.h"
#include "Memory/ResourcePool.h"

VK_NAMESPACE::Image VK_NAMESPACE::Clone(Context ctx, const Image& image)
{
	Core::ImageConfig config = image.GetConfig();
	ImageCreateInfo createInfo{};

	createInfo.Extent = config.Extent;
	createInfo.Format = config.Format;
	createInfo.Tiling = config.Tiling;
	createInfo.Type = config.Type;
	createInfo.MemProps = config.MemProps;
	createInfo.Usage = config.Usage;

	Image clone = ctx.CreateResourcePool().CreateImage(createInfo);

	if (ctx.GetHandle() != image.mChunk->Device)
	{
		clone.CopyBufferData(Clone(ctx, image.GetImageBuffer()));
		return clone;
	}

	clone.Blit(image, {});

	return clone;
}

VK_NAMESPACE::ImageView VK_NAMESPACE::Clone(Context ctx, const ImageView& view)
{
	return Clone(ctx, *view).CreateImageView(view.GetImageData().Info);
}

VK_NAMESPACE::Image VK_NAMESPACE::ResourcePool::CreateImage(const ImageCreateInfo& info) const
{
	auto Device = mDevice;

	Core::ImageConfig config{};
	config.Extent = info.Extent;
	config.Format = info.Format;
	config.LogicalDevice = *Device;
	config.PhysicalDevice = mPhysicalDevice.Handle;
	config.ResourceOwner = mWorkingClass->FindOptimalQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
	config.Tiling = info.Tiling;
	config.Type = info.Type;
	config.MemProps = info.MemProps;
	config.Usage = info.Usage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

	Core::Image Handles = Core::Utils::CreateImage(config);

	Core::ImageResource chunk{};
	chunk.Device = Device;
	chunk.ImageHandles = Handles;

	Core::Ref<Core::ImageResource> chunkRef = Core::CreateRef<Core::ImageResource>(
		[](const Core::ImageResource& handles)
	{
		handles.Device->freeMemory(handles.ImageHandles.Memory);
		handles.Device->destroyImage(handles.ImageHandles.Handle);
		handles.Device->destroyImageView(handles.ImageHandles.IdentityView);
	}, chunk);

	Image image(chunkRef);

	image.mCommandPools = mImageCommandPools;
	image.mWorkingClass = GetWorkingClass();
	image.mChunk->RscPool = std::make_shared<ResourcePool>(*this);

	return image;
}

VK_NAMESPACE::SamplerCache VK_NAMESPACE::ResourcePool::CreateSamplerCache() const
{
	SamplerCache cache = std::make_shared<BasicSamplerCachePayload<SamplerInfo>>();
	return cache;
}

VK_NAMESPACE::VK_CORE::Ref<vk::Sampler> VK_NAMESPACE::ResourcePool::CreateSampler(
	const SamplerInfo& samplerInfo, SamplerCache cache /*= {} */) const
{
	if (cache)
	{
		auto found = cache->find(samplerInfo);

		if (found != cache->end())
			return found->second;
	}

	vk::SamplerCreateInfo createInfo{};
	createInfo.setAddressModeU(samplerInfo.AddressModeU);
	createInfo.setAddressModeV(samplerInfo.AddressModeV);
	createInfo.setAddressModeW(samplerInfo.AddressModeW);
	createInfo.setAnisotropyEnable(samplerInfo.AnisotropyEnable);
	createInfo.setBorderColor(samplerInfo.BorderColor);
	createInfo.setMagFilter(samplerInfo.MagFilter);
	createInfo.setMinFilter(samplerInfo.MinFilter);
	createInfo.setMaxAnisotropy(samplerInfo.MaxAnisotropy);
	createInfo.setMaxLod(samplerInfo.MaxLod);
	createInfo.setMinLod(samplerInfo.MinLod);
	createInfo.setMipLodBias(0.0f);
	createInfo.setUnnormalizedCoordinates(!samplerInfo.NormalisedCoordinates);

	auto handle = mDevice->createSampler(createInfo);
	auto Device = mDevice;

	auto Result = Core::CreateRef<vk::Sampler>([Device](vk::Sampler& sampler)
	{ Device->destroySampler(sampler); }, handle);

	if (cache)
		(*cache)[samplerInfo] = Result;

	return Result;
}
