#pragma once
#include "../Core/Config.h"
#include "../Core/Ref.h"
#include "../Core/Utils/FramebufferUtils.h"

#include "../Memory/Image.h"
#include "../Descriptors/DescriptorsConfig.h"

#include "../ShaderCompiler/ShaderCompiler.h"
#include "../Memory/RenderTargetContext.h"

VK_BEGIN

class RenderTargetContext;
class PShader;

enum class GraphicsPipelineState
{
	eInitialized       = 0,
	eRecording         = 1,
	eExecutable        = 2,
};

struct RasterizerInfo
{
	vk::CullModeFlags CullMode = vk::CullModeFlagBits::eNone;
	vk::FrontFace FrontFace = vk::FrontFace::eClockwise;
	float LineWidth = 1.0f;
	vk::PolygonMode PolygonMode = vk::PolygonMode::eFill;
};

struct DepthStencilBufferingInfo
{
	bool DepthTestEnable = false;
	bool DepthWriteEnable = false;
	bool DepthBoundsTestEnable = false;
	vk::CompareOp DepthCompareOp = vk::CompareOp::eLess;
	float MinDepthBounds = 0.0f;
	float MaxDepthBounds = 1.0f;
	bool StencilTestEnable = false;
	vk::StencilOpState StencilFrontOp{};
	vk::StencilOpState StencilBackOp{};
};

struct VertexInputDesc
{
	std::vector<vk::VertexInputAttributeDescription> Attributes;
	std::vector<vk::VertexInputBindingDescription> Bindings;
};

struct PipelineLayoutData
{
	vk::PipelineLayout Layout;
	std::vector<Core::Ref<DescriptorResource>> DescResources;
	std::vector<vk::PushConstantRange> PushConstantRanges;
};

struct PipelineInfo
{
	std::vector<ShaderSPIR_V> ShaderStages;
};

struct GraphicsPipelineConfig
{
	// Editable
	vk::PrimitiveTopology Topology = vk::PrimitiveTopology::eTriangleList;
	VertexInputDesc VertexInput;

	RasterizerInfo Rasterizer; // Editable

	// Editable
	DepthStencilBufferingInfo DepthBufferingState;

	// Editable
	vk::Viewport CanvasView;
	vk::Rect2D CanvasScissor; // Editable

	RenderTargetContext TargetContext;

	vk::IndexType IndicesType = vk::IndexType::eUint32;

	uint32_t SubpassIndex = 0;

	std::vector<vk::DynamicState> DynamicStates;
};

struct PipelineHandles
{
	vk::Pipeline Handle;
	PipelineLayoutData LayoutData;
	//std::vector<vk::DescriptorSetLayout> SetLayouts;
	std::vector<vk::DescriptorSet> SetCache;
	std::vector<uint32_t> DynamicOffsets;

	Core::Ref<vk::Device> Device;
};

struct PipelineBuilderData
{
	vk::PipelineCache Cache;
};

VK_END
