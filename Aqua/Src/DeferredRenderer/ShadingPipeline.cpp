#include "Core/Aqpch.h"
#include "DeferredRenderer/Pipelines/ShadingPipeline.h"

AQUA_NAMESPACE::ShadingPipeline::ShadingPipeline(const vkLib::PShader& shader, vkLib::Framebuffer shadingBuffer) 
{
	this->SetShader(shader);
	this->SetFramebuffer(shadingBuffer);

	vkLib::GraphicsPipelineConfig config{};

	SetupBasicConfig(config);

	config.TargetContext = shadingBuffer.GetParentContext();

	this->SetConfig(config);

	auto cAttachs = GetFramebuffer().GetColorAttachments();
	mOutImage = *cAttachs[0];
}

void AQUA_NAMESPACE::ShadingPipeline::SetupBasicConfig(vkLib::GraphicsPipelineConfig& config)
{
	glm::uvec2 scrSize = this->GetFramebuffer().GetResolution();

	config.CanvasScissor.offset = vk::Offset2D(0, 0);
	config.CanvasScissor.extent = vk::Extent2D(scrSize.x, scrSize.y);
	config.CanvasView = vk::Viewport(0.0f, 0.0f, (float)scrSize.x, (float)scrSize.y, 0.0f, 1.0f);

	config.IndicesType = vk::IndexType::eUint32;

	config.SubpassIndex = 0;

	config.DepthBufferingState.DepthBoundsTestEnable = false;
	config.DepthBufferingState.DepthCompareOp = vk::CompareOp::eLess;
	config.DepthBufferingState.DepthTestEnable = false;
	config.DepthBufferingState.DepthWriteEnable = true;
	config.DepthBufferingState.MaxDepthBounds = 1.0f;
	config.DepthBufferingState.MinDepthBounds = 0.0f;
	config.DepthBufferingState.StencilTestEnable = false;

	config.Rasterizer.CullMode = vk::CullModeFlagBits::eNone;
	config.Rasterizer.FrontFace = vk::FrontFace::eCounterClockwise;
	config.Rasterizer.LineWidth = 0.01f;
	config.Rasterizer.PolygonMode = vk::PolygonMode::eFill;

	config.DynamicStates.emplace_back(vk::DynamicState::eScissor);
	config.DynamicStates.emplace_back(vk::DynamicState::eViewport);
	config.DynamicStates.emplace_back(vk::DynamicState::eLineWidth);
	config.DynamicStates.emplace_back(vk::DynamicState::eDepthCompareOp);
}

void AQUA_NAMESPACE::ShadingPipeline::UpdateDescriptors()
{
	vkLib::PShader shader = this->GetShader();

	auto updateImage = [this, &shader](const vkLib::DescriptorLocation& location, vkLib::ImageView imageView)
		{
			vkLib::SampledImageWriteInfo sampledImageInfo{};
			sampledImageInfo.ImageLayout = vk::ImageLayout::eGeneral;
			sampledImageInfo.ImageView = imageView.GetNativeHandle();
			sampledImageInfo.Sampler = *imageView->GetSampler();

			if (!shader.IsEmpty(location.SetIndex, location.Binding))
				this->UpdateDescriptor(location, sampledImageInfo);
		};

	auto updateStorageBuffer = [this, &shader](
		const vkLib::DescriptorLocation& location, vkLib::Core::BufferResource buffer)
		{
			vkLib::StorageBufferWriteInfo storageBuffer{};
			storageBuffer.Buffer = buffer.BufferHandles->Handle;

			if (!shader.IsEmpty(location.SetIndex, location.Binding) && storageBuffer.Buffer)
				this->UpdateDescriptor(location, storageBuffer);
		};

	auto updateUniformBuffer = [this, &shader](
		const vkLib::DescriptorLocation& location, vkLib::Core::BufferResource buffer)
		{
			vkLib::UniformBufferWriteInfo uniformBuffer{};
			uniformBuffer.Buffer = buffer.BufferHandles->Handle;

			if (!shader.IsEmpty(location.SetIndex, location.Binding) && uniformBuffer.Buffer)
				this->UpdateDescriptor(location, uniformBuffer);
		};

	// The Geometry images always placed in the set zero
	uint32_t i = 0;

	for (const auto& [tag, vertRsc] : mGeometry)
	{
		updateImage(vertRsc.Location, vertRsc.ImageView);
	}

	for (const auto& [tag, resource] : mResources)
	{
		if (tag[0] == '@')
			updateImage(resource.Location, resource.ImageView);
		if (tag[0] == '$')
			updateStorageBuffer(resource.Location, resource.Buffer);
		if (tag[0] == '#')
			updateUniformBuffer(resource.Location, resource.Buffer);
	}
}

void AQUA_NAMESPACE::ShadingPipeline::SetGeometry(const FragmentResourceMap& geometry)
{
	mGeometry.clear();

	for (auto [name, image] : geometry)
	{
		mGeometry[name].ImageView = image.GetIdentityImageView();
	}
}

void AQUA_NAMESPACE::ShadingPipeline::SetGeometryDescLocation(const std::string& tag, const glm::uvec3& location)
{
	_STL_ASSERT(mGeometry.find(tag) != mGeometry.end(), "The geometry image doesn't exist");

	if (mGeometry.find(tag) != mGeometry.end())
		mGeometry[tag].Location = { location.x, location.y, location.z };
}
