#pragma once
#include "../Core/Config.h"
#include "../Core/Ref.h"
#include "PipelineConfig.h"
#include "../Memory/Framebuffer.h"
#include "BasicPipeline.h"
#include "PShader.h"

#include "../Descriptors/DescriptorsConfig.h"

#include "../Memory/Buffer.h"

#include "../Core/Logger.h"

VK_BEGIN

using GraphicsDynamicStateFn = std::function<void(vk::CommandBuffer, const GraphicsPipelineConfig&)>;

struct GraphicsPipelineHandles : public PipelineHandles, public PipelineInfo
{
	RenderTargetContext TargetContext;
	GraphicsPipelineState State = GraphicsPipelineState::eInitialized;

	std::vector<GraphicsDynamicStateFn> DynamicChange;
};

// Not thread safe
template <typename BasePipeline>
class BasicGraphicsPipeline : public BasePipeline
{
public:
	using MyBasePipeline = BasePipeline;
	using MyIndex = uint32_t; // For now, we will set IndexType to uint32_t only

public:
	BasicGraphicsPipeline() = default;
	BasicGraphicsPipeline(const ShaderFiles& shaders, const PreprocessorDirectives& directives = {})
		: MyBasePipeline(shaders, directives) {}

	BasicGraphicsPipeline(const PShader& shader)
		: MyBasePipeline(shader) {}

	virtual ~BasicGraphicsPipeline() = default;

	// Configurable stuff at runtime when allowed
	void SetCullMode(vk::CullModeFlags cullMode) const;
	void SetDepthCompareOp(vk::CompareOp op) const;
	void SetDepthTestEnable(bool enable) const;
	void SetDepthWriteEnable(bool enable) const;
	void SetFrontFace(vk::FrontFace frontFace) const;
	void SetLineWidth(float lineWidth) const;
	void SetPrimitiveTopology(vk::PrimitiveTopology topology) const;
	void SetScissor(const vk::Rect2D& rect) const;
	void SetViewport(const vk::Viewport& viewport) const;

	// Methods inside the begin/end scope
	virtual void Begin(vk::CommandBuffer commandBuffer) const;

	void Activate() const;

	void DrawVertices(uint32_t vertexOffset, uint32_t firstInstance,
		uint32_t instanceCount, uint32_t vertexCount) const;

	void DrawVerticesIndirect(uint32_t drawOffset, uint32_t stride = sizeof(vk::DrawIndexedIndirectCommand),
		uint32_t drawCount = std::numeric_limits<uint32_t>::max()) const;

	void DrawIndexed(uint32_t indexOffset, uint32_t vertexOffset, uint32_t firstInstance,
		uint32_t instanceCount, uint32_t indexCount = std::numeric_limits<uint32_t>::max()) const;

	void DrawIndexedIndirect(uint32_t drawOffset, uint32_t stride = sizeof(vk::DrawIndirectCommand),
		uint32_t drawCount = std::numeric_limits<uint32_t>::max()) const;

	// Ends the scope
	virtual void End() const;

	virtual vk::PipelineBindPoint GetPipelineBindPoint() const { return vk::PipelineBindPoint::eGraphics; }
	virtual PipelineLayoutData GetPipelineLayoutData() const override { return mHandles->LayoutData; }

	// Some of the configuration is editable at runtime
	const GraphicsPipelineConfig& GetConfig() const { return mConfig; }

	// Required by the GraphicsPipeline
	Framebuffer GetFramebuffer() const { return mFramebuffer; }

	Core::Ref<GraphicsPipelineHandles> GetPipelineHandles() const { return mHandles; }
	vkLib::RenderTargetContext GetRenderTargetContext() const { return mHandles->TargetContext; }

	void SetClearColorValues(uint32_t idx, const glm::vec4& color);
	void SetClearDepthStencilValues(float depth, uint32_t stencil);

	// User defined behavior...
	void SetConfig(const vkLib::GraphicsPipelineConfig& config) { mConfig = config; }
	void SetFramebuffer(vkLib::Framebuffer target) { mFramebuffer = target; }

	// Buffers state - we can only set all these buffers, but there's no way to retrieve them back. 
	// The user is expected to manage store them on their side if they want to manage them
	template <typename T>
	void SetVertexBuffer(uint32_t binding, vkLib::Buffer<T> buffer) 
	{ mVertexBuffers[binding] = buffer.GetBufferRsc(); }

	template <typename T>
	void SetIndexBuffer(vkLib::Buffer<T> buffer) { mIndexBuffer = buffer.GetBufferRsc(); }

	template <typename T>
	void SetIndexIndirectBuffer(vkLib::Buffer<T> buffer)
	{ mIndexIndirectBuffer = buffer.GetBufferRsc(); }

	template<typename T>
	void SetVertexIndirectBuffer(vkLib::Buffer<T> buffer)
	{ mVertexIndirectBuffer = buffer.GetBufferRsc(); }

private:
	Core::Ref<GraphicsPipelineHandles> mHandles;
	vkLib::Framebuffer mFramebuffer;

	mutable vkLib::GraphicsPipelineConfig mConfig;

	// binding to vertex buffer
	std::vector<vkLib::Core::BufferResource> mVertexBuffers;
	vkLib::Core::BufferResource mIndexBuffer;

	vkLib::Core::BufferResource mIndexIndirectBuffer;
	vkLib::Core::BufferResource mVertexIndirectBuffer;

	std::vector<vk::ClearValue> mClearValues;

	friend class PipelineBuilder;

private:
	virtual void BindVertexBuffers(vk::CommandBuffer commandBuffer) const;
	virtual void BindIndexBuffer(vk::CommandBuffer commandBuffer) const;

	virtual size_t GetIndexCount() const { return mIndexBuffer.BufferHandles->ElemCount * 
		mIndexBuffer.BufferHandles->Config.TypeSize / sizeof(MyIndex); }

	void BeginRenderPass() const;
	void EndRenderPass() const;
};

using GraphicsPipeline = BasicGraphicsPipeline<BasicPipeline>;

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetClearColorValues(uint32_t idx, const glm::vec4& color)
{
	_STL_ASSERT(GetRenderTargetContext().GetColorAttachmentCount() > idx, "Can't clear the color attach");

	mClearValues[idx].color = vk::ClearColorValue(color.x, color.y, color.z, color.w);
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetClearDepthStencilValues(float depth, uint32_t stencil)
{
	_STL_ASSERT(GetRenderTargetContext().UsingDepthOrStencilAttachment(), "Can't clear the depth/stencil attach");

	mClearValues.back().depthStencil = vk::ClearDepthStencilValue(depth, stencil);
}

template <typename BasePipeline>
void BasicGraphicsPipeline<BasePipeline>::Begin(vk::CommandBuffer commandBuffer) const
{
	_STL_ASSERT(mHandles->State != GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has already been called! "
		"You must end the current scope by calling GraphicsPipeline::End before starting a new one");

	this->BeginPipeline(commandBuffer);
	mHandles->State = GraphicsPipelineState::eRecording;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::Activate() const
{
	// activating in the begin function since only a single renderpass can be recorded at once
	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mHandles->Handle);

	for (auto dynamicChange : mHandles->DynamicChange)
	{
		dynamicChange(commandBuffer, mConfig);
	}

	if (!mHandles->SetCache.empty())
	{
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mHandles->LayoutData.Layout, 0, mHandles->SetCache, nullptr);
	}
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::DrawVertices(
	uint32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount, uint32_t vertexCount) const
{
	_STL_ASSERT(mHandles->State == GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has never been called! "
		"You must begin the scope by calling GraphicsPipeline::Begin before calling "
		"GraphicsPipeline::End!");

	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();

	Framebuffer renderTarget = GetFramebuffer();
	BindVertexBuffers(commandBuffer);

	BeginRenderPass();

	commandBuffer.draw(vertexCount, instanceCount, vertexOffset, firstInstance);

	EndRenderPass();
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::DrawVerticesIndirect(
	uint32_t drawOffset, uint32_t stride /*= sizeof(vk::DrawIndexedIndirectCommand)*/,
	uint32_t drawCount /*= std::numeric_limits<uint32_t>::max()*/) const
{
	_STL_ASSERT(mHandles->State == GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has never been called! "
		"You must begin the scope by calling GraphicsPipeline::Begin before calling "
		"GraphicsPipeline::End!");

	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();


	Framebuffer renderTarget = GetFramebuffer();

	BindVertexBuffers(commandBuffer);

	drawCount = drawCount == std::numeric_limits<uint32_t>::max() ?
		(uint32_t)mVertexIndirectBuffer.BufferHandles->ElemCount * 
		mVertexIndirectBuffer.BufferHandles->Config.TypeSize / stride : drawCount;

	BeginRenderPass();

	commandBuffer.drawIndirect(mVertexIndirectBuffer.BufferHandles->Handle,
		drawOffset, drawCount, stride);

	EndRenderPass();
}

template <typename BasePipeline>
void BasicGraphicsPipeline<BasePipeline>::DrawIndexed(uint32_t firstIndex, 
	uint32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount, uint32_t indexCount) const
{
	_STL_ASSERT(mHandles->State == GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has never been called! "
		"You must begin the scope by calling GraphicsPipeline::Begin before calling "
		"GraphicsPipeline::End!");

	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();


	Framebuffer renderTarget = GetFramebuffer();

	BindVertexBuffers(commandBuffer);
	BindIndexBuffer(commandBuffer);

	uint32_t idxCount = indexCount == std::numeric_limits<uint32_t>::max() ?
		(uint32_t) GetIndexCount() : indexCount;

	BeginRenderPass();

	commandBuffer.drawIndexed(idxCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	EndRenderPass();
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::DrawIndexedIndirect(uint32_t drawOffset,
	uint32_t stride, uint32_t drawCount) const
{
	_STL_ASSERT(mHandles->State == GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has never been called! "
		"You must begin the scope by calling GraphicsPipeline::Begin before calling "
		"GraphicsPipeline::End!");

	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();

	Framebuffer renderTarget = GetFramebuffer();

	BindVertexBuffers(commandBuffer);
	BindIndexBuffer(commandBuffer);

	drawCount = drawCount == std::numeric_limits<uint32_t>::max() ?
		(uint32_t)mIndexIndirectBuffer.BufferHandles->ElemCount *
		mIndexIndirectBuffer.BufferHandles->Config.TypeSize / stride : drawCount;

	BeginRenderPass();

	commandBuffer.drawIndexedIndirect(mIndexIndirectBuffer.BufferHandles->Handle, 
		drawOffset, drawCount, stride);

	EndRenderPass();
}

template <typename BasePipeline>
void BasicGraphicsPipeline<BasePipeline>::End() const
{
	_STL_ASSERT(mHandles->State == GraphicsPipelineState::eRecording, "GraphicsPipeline::Begin has never been called! "
		"You must begin the scope by calling GraphicsPipeline::Begin before calling "
		"GraphicsPipeline::End!");

	mHandles->State = GraphicsPipelineState::eExecutable;
	this->EndPipeline();
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::BindVertexBuffers(vk::CommandBuffer commandBuffer) const
{
	if (mVertexBuffers.empty())
		return;

	std::vector<vk::Buffer> buffers(mVertexBuffers.size());
	std::vector<vk::DeviceSize> offsets(mVertexBuffers.size());

	for (size_t i = 0; i < mVertexBuffers.size(); i++)
	{
		buffers[i] = mVertexBuffers[i].BufferHandles->Handle;
		offsets[i] = 0;
	}

	commandBuffer.bindVertexBuffers(0, buffers, offsets);
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::BindIndexBuffer(vk::CommandBuffer commandBuffer) const
{
	commandBuffer.bindIndexBuffer(mIndexBuffer.BufferHandles->Handle, 0, vk::IndexType::eUint32);
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::BeginRenderPass() const
{
	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();

	Framebuffer renderTarget = GetFramebuffer();

	vk::Rect2D renderArea;
	renderArea.offset = vk::Offset2D(0, 0);
	renderArea.extent = vk::Extent2D(renderTarget.GetResolution().x, renderTarget.GetResolution().y);

	renderTarget.BeginCommands(commandBuffer);

	// TODO: Transform each attachment to the default layout of render pass

	vk::RenderPassBeginInfo beginRenderPass{};
	beginRenderPass.setClearValues(mClearValues);
	beginRenderPass.setFramebuffer(renderTarget.GetNativeHandle());
	beginRenderPass.setRenderArea(renderArea);
	beginRenderPass.setRenderPass(GetRenderTargetContext().GetNativeHandle());

	commandBuffer.beginRenderPass(beginRenderPass, vk::SubpassContents::eInline);
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::EndRenderPass() const
{
	vk::CommandBuffer commandBuffer = this->GetCommandBuffer();

	commandBuffer.endRenderPass();
	GetFramebuffer().EndCommands();
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetCullMode(vk::CullModeFlags cullMode) const
{
	mConfig.Rasterizer.CullMode = cullMode;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetDepthCompareOp(vk::CompareOp op) const
{
	mConfig.DepthBufferingState.DepthCompareOp = op;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetDepthTestEnable(bool enable) const
{
	mConfig.DepthBufferingState.DepthTestEnable = enable;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetDepthWriteEnable(bool enable) const
{
	mConfig.DepthBufferingState.DepthWriteEnable = enable;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetFrontFace(vk::FrontFace frontFace) const
{
	mConfig.Rasterizer.FrontFace = frontFace;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetLineWidth(float lineWidth) const
{
	mConfig.Rasterizer.LineWidth = lineWidth;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetPrimitiveTopology(vk::PrimitiveTopology topology) const
{
	mConfig.Topology = topology;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetScissor(const vk::Rect2D& rect) const
{
	mConfig.CanvasScissor = rect;
}

template <typename BasePipeline>
void VK_NAMESPACE::BasicGraphicsPipeline<BasePipeline>::SetViewport(const vk::Viewport& viewport) const
{
	mConfig.CanvasView = viewport;
}

VK_END
