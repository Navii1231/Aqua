#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/RenderTargetFactory.h"

void AQUA_NAMESPACE::RenderTargetFactory::Clear()
{
	Invalidate();

	mColorProperties.clear();
	mColorAttributes.clear();
	ClearAllImageViews();
}

void AQUA_NAMESPACE::RenderTargetFactory::AddColorAttribute(const std::string& name, const std::string& format)
{
	Invalidate();

	mColorProperties[name].SetAttachIdx(static_cast<uint32_t>(mColorAttributes.size()));
	mColorAttributes.emplace_back(name, format);
}

void AQUA_NAMESPACE::RenderTargetFactory::SetDepthAttribute(const std::string& name, const std::string& format)
{
	Invalidate();

	mDepthAttribute = { name, format };
	mDepthProperties.SetAttachIdx(-1);
	mDepthProperties.SetUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);
	mDepthProperties.SetLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void AQUA_NAMESPACE::RenderTargetFactory::SetStencilLoadOp(vk::AttachmentLoadOp op)
{
	Invalidate();
	mDepthProperties.SetStencilLoadOp(op);
}

void AQUA_NAMESPACE::RenderTargetFactory::SetStencilStoreOp(vk::AttachmentStoreOp op)
{
	Invalidate();
	mDepthProperties.SetStencilStoreOp(op);
}

std::expected<bool, AQUA_NAMESPACE::RenderFactoryError> AQUA_NAMESPACE::RenderTargetFactory::Validate()
{
	auto error = CheckValidity();

	if (!error)
		return error;

	GenerateRenderContext();

	// Set empty image views
	mImageViews.resize(mColorAttributes.size() + mContext.UsingDepthOrStencilAttachment());

	return error;
}

void AQUA_NAMESPACE::RenderTargetFactory::SetImageView(const std::string& name, vkLib::ImageView view)
{
	if (mDepthAttribute.Name == name)
	{
		mImageViews.back() = view;
		return;
	}

	_STL_ASSERT(mColorProperties.find(name) != mColorProperties.end(), "Trying to set a non existent image view");

	mImageViews[mColorProperties.at(name).AttachIdx] = view;
}

void AQUA_NAMESPACE::RenderTargetFactory::RemoveImageView(const std::string& name)
{
	if (mDepthAttribute.Name == name)
	{
		mImageViews.back() = vkLib::ImageView();
		return;
	}

	_STL_ASSERT(mColorProperties.find(name) != mColorProperties.end(), "Trying to set a non existent image view");

	mImageViews[mColorProperties.at(name).AttachIdx] = vkLib::ImageView();
}

void AQUA_NAMESPACE::RenderTargetFactory::ClearAllImageViews()
{
	for (auto& view : mImageViews)
		view = vkLib::ImageView();
}

std::expected<vkLib::Framebuffer, vkLib::FramebufferConstructError> AQUA_NAMESPACE::RenderTargetFactory::CreateFramebuffer()
{
	for (const auto& view : mImageViews)
	{
		if(view) 
			return mContext.ConstructFramebuffer(mImageViews);
	}

	return mContext.CreateFramebuffer(mTargetSize.x, mTargetSize.y);
}

void AQUA_NAMESPACE::RenderTargetFactory::GenerateRenderContext()
{
	vkLib::RenderContextCreateInfo createInfo{};
	vkLib::ImageAttachmentInfo attachInfo{};

	createInfo.Attachments.reserve(mColorProperties.size());
	
	for (const auto& attribute : mColorAttributes)
	{
		const auto& properties = mColorProperties[attribute.Name];

		auto [format, whatever1, whatever2, whatever3] = ConvertIntoTagAttributes(attribute.Format);

		attachInfo.Format = format;
		attachInfo.Layout = properties.Layout;
		attachInfo.LoadOp = properties.LoadOp;
		attachInfo.StoreOp = properties.StoreOp;
		attachInfo.Samples = properties.Samples;
		attachInfo.StencilLoadOp = properties.StencilLoadOp;
		attachInfo.StencilStoreOp = properties.StencilStoreOp;
		attachInfo.Usage = properties.Usage;

		createInfo.Attachments.emplace_back(attachInfo);
	}

	if (!mDepthAttribute.Name.empty())
	{
		const auto& properties = mDepthProperties;
		auto [format, whatever1, whatever2, whatever3] = ConvertIntoTagAttributes(mDepthAttribute.Format);

		attachInfo.Format = format;
		attachInfo.LoadOp = properties.LoadOp;
		attachInfo.Layout = properties.Layout;
		attachInfo.StoreOp = properties.StoreOp;
		attachInfo.Samples = properties.Samples;
		attachInfo.StencilLoadOp = properties.StencilLoadOp;
		attachInfo.StencilStoreOp = properties.StencilStoreOp;
		attachInfo.Usage = properties.Usage;

		createInfo.UsingDepthAttachment = true;
		createInfo.UsingStencilAttachment =
			format == vk::Format::eD24UnormS8Uint ||
			format == vk::Format::eD16UnormS8Uint ||
			format == vk::Format::eD32SfloatS8Uint;

		attachInfo.Layout = !createInfo.UsingStencilAttachment ? vk::ImageLayout::eDepthAttachmentOptimal : attachInfo.Layout;

		createInfo.Attachments.emplace_back(attachInfo);
	}

	mContext = mBuilder.Build(createInfo);
}

std::expected<bool, AQUA_NAMESPACE::RenderFactoryError> AQUA_NAMESPACE::RenderTargetFactory::CheckValidity()
{
	std::expected<bool, AQUA_NAMESPACE::RenderFactoryError> error;

	if (mColorProperties.size() == 0 && mDepthAttribute.Name.empty())
		return std::unexpected(RenderFactoryError::eColorOrDepthAttachment);

	if (mColorProperties.size() != mColorAttributes.size())
		return std::unexpected(RenderFactoryError::eInconsistentColorAttachments);

	return true;
}
