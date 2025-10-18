#pragma once
#include "../Core/Config.h"
#include "../Core/Utils/MemoryUtils.h"
#include "../Core/Ref.h"

VK_BEGIN

class ResourcePool;

VK_CORE_BEGIN

struct BufferResource
{
	Core::Ref<Core::Buffer> BufferHandles;
	Core::Ref<vk::Device> Device;

	std::shared_ptr<ResourcePool> RscPool;
};

struct ImageResource
{
	Core::Image ImageHandles;
	Core::Ref<vk::Sampler> Sampler;
	Core::Ref<vk::Device> Device;

	std::shared_ptr<ResourcePool> RscPool;
};

VK_CORE_END

struct ImageCreateInfo
{
	vk::ImageType Type = vk::ImageType::e2D;
	vk::Format Format = vk::Format::eR8G8B8A8Unorm;
	vk::ImageTiling Tiling = vk::ImageTiling::eOptimal;

	vk::Extent3D Extent = { 0, 0, 0 };
	vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eColorAttachment;
	vk::MemoryPropertyFlags MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
};

struct ImageBlitInfo
{
	vk::Filter Filter = vk::Filter::eLinear;

	glm::uvec2 SrcBeginRegion = { 0, 0 };
	glm::uvec2 SrcEndRegion = { std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() };

	glm::uvec2 DstBeginRegion = { 0, 0 };
	glm::uvec2 DstEndRegion = { std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() };
};

struct MemoryBarrierInfo
{
	vk::AccessFlags SrcAccessMasks = {};
	vk::AccessFlags DstAccessMasks = {};

	vk::PipelineStageFlags SrcPipeleinStages = {};
	vk::PipelineStageFlags DstPipelineStages = {};

	vk::DependencyFlags DependencyFlags = {};
};

struct ImageAttachmentInfo
{
	vk::Format Format = vk::Format::eR8G8B8A8Unorm;
	vk::SampleCountFlagBits Samples = vk::SampleCountFlagBits::e1;
	vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eClear;
	vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eStore;
	vk::AttachmentLoadOp StencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	vk::AttachmentStoreOp StencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	vk::ImageLayout Layout = vk::ImageLayout::eShaderReadOnlyOptimal;
	vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eTransferSrc;
};

struct RenderContextCreateInfo
{
	// NOTE: the depth and stencil attachments come at the end
	std::vector<ImageAttachmentInfo> Attachments;

	bool UsingDepthAttachment = false;
	bool UsingStencilAttachment = false;
};

struct RenderContextInfo
{
	vk::RenderPass RenderPass;
	RenderContextCreateInfo CreateInfo;
};

struct ImageViewData
{
	vk::ImageView View;
	ImageViewCreateInfo Info;

	glm::uvec2 Size;
};

template <uint32_t FloatPrecision>
struct BasicSamplerInfo
{
	vk::SamplerAddressMode AddressModeU = vk::SamplerAddressMode::eClampToEdge;
	vk::SamplerAddressMode AddressModeV = vk::SamplerAddressMode::eClampToEdge;
	vk::SamplerAddressMode AddressModeW = vk::SamplerAddressMode::eClampToEdge;

	vk::Filter MagFilter = vk::Filter::eLinear;
	vk::Filter MinFilter = vk::Filter::eLinear;

	vk::SamplerMipmapMode MipMapMode = vk::SamplerMipmapMode::eLinear;
	bool AnisotropyEnable = true;
	bool CompareEnable = false;

	float MaxAnisotropy = 16.0f;
	vk::CompareOp CompareOp = vk::CompareOp::eAlways;

	float MinLod = 0.0f;
	float MaxLod = VK_LOD_CLAMP_NONE;
	vk::BorderColor BorderColor = vk::BorderColor::eIntOpaqueBlack;
	bool NormalisedCoordinates = true;

	constexpr static uint32_t sFloatPrecision = FloatPrecision;

	bool operator ==(const BasicSamplerInfo& Other) const
	{
		return MagFilter == Other.MagFilter &&
			MinFilter == Other.MinFilter &&
			AddressModeU == Other.AddressModeU &&
			AddressModeV == Other.AddressModeV &&
			AddressModeW == Other.AddressModeW &&
			MipMapMode == Other.MipMapMode &&
			AnisotropyEnable == Other.AnisotropyEnable &&
			CompareEnable == Other.CompareEnable &&
			CompareOp == Other.CompareOp &&
			BorderColor == Other.BorderColor &&
			NormalisedCoordinates == Other.NormalisedCoordinates &&
			static_cast<uint32_t>(MaxAnisotropy * FloatPrecision) ==
			static_cast<uint32_t>(Other.MaxAnisotropy * FloatPrecision) &&

			static_cast<uint32_t>(MinLod * FloatPrecision) ==
			static_cast<uint32_t>(Other.MinLod * FloatPrecision) &&

			static_cast<uint32_t>(MaxLod * FloatPrecision) ==
			static_cast<uint32_t>(Other.MaxLod * FloatPrecision);
	}
};

template <typename T>
inline void HashCombine(size_t& seed, const T& value)
{
	seed ^= std::hash<T>()(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

VK_END

namespace std
{
	template <uint32_t FloatPrecision>
	struct hash<VK_NAMESPACE::BasicSamplerInfo<FloatPrecision>>
	{
		size_t operator()(const VK_NAMESPACE::BasicSamplerInfo<FloatPrecision>& key) const
		{
			size_t seed = 0;

			VK_NAMESPACE::HashCombine(seed, key.AddressModeU);
			VK_NAMESPACE::HashCombine(seed, key.AddressModeV);
			VK_NAMESPACE::HashCombine(seed, key.AddressModeW);
			VK_NAMESPACE::HashCombine(seed, key.MagFilter);
			VK_NAMESPACE::HashCombine(seed, key.MinFilter);
			VK_NAMESPACE::HashCombine(seed, key.MipMapMode);
			VK_NAMESPACE::HashCombine(seed, key.AnisotropyEnable);
			VK_NAMESPACE::HashCombine(seed, key.BorderColor);
			VK_NAMESPACE::HashCombine(seed, key.NormalisedCoordinates);
			VK_NAMESPACE::HashCombine(seed, key.CompareEnable);
			VK_NAMESPACE::HashCombine(seed, key.CompareOp);
			VK_NAMESPACE::HashCombine(seed, static_cast<uint32_t>(key.MaxAnisotropy * FloatPrecision));
			VK_NAMESPACE::HashCombine(seed, static_cast<uint32_t>(key.MinLod * FloatPrecision));
			VK_NAMESPACE::HashCombine(seed, static_cast<uint32_t>(key.MaxLod * FloatPrecision));

			return seed;
		}
	};
}

VK_BEGIN

template <typename SamplerKey>
using BasicSamplerCachePayload = std::unordered_map<SamplerKey, Core::Ref<vk::Sampler>>;

template <typename SamplerKey>
using BasicSamplerCache = std::shared_ptr<BasicSamplerCachePayload<SamplerKey>>;

using SamplerInfo = BasicSamplerInfo<1000>;
using SamplerCache = BasicSamplerCache<SamplerInfo>;

VK_END

