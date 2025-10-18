#pragma once
#include "../Config.h"
#include "../../Device/ContextConfig.h"
#include "../../Device/PhysicalDevice.h"

VK_BEGIN

enum AttachmentTypeFlagBits
{
	eColor        = 1,
	eDepth        = 2,
	eStencil      = 4,
};

using AttachmentTypeFlags = vk::Flags<AttachmentTypeFlagBits>;

VK_CORE_BEGIN

struct ImageAttachmentConfig
{
	vk::AttachmentDescription AttachmentDesc;
	vk::ImageLayout Layout;
	vk::ImageUsageFlags Usage;
};

VK_UTILS_BEGIN

// Render Passes and Framebuffers...
VKLIB_API vk::RenderPass CreateRenderPass(vk::Device device,
	const std::vector<ImageAttachmentConfig>& attachments,
	bool depthIncluded,
	vk::PipelineBindPoint BindPoint);

// Shader Modules
VKLIB_API vk::ShaderModule CreateShaderModule(vk::Device device, const ShaderSPIR_V& shaders);

VKLIB_API std::vector<vk::PipelineShaderStageCreateInfo> CreatePipelineShaderStages(
	vk::Device device, const std::vector<ShaderSPIR_V>& ShaderStages);

VK_UTILS_END
VK_CORE_END
VK_END
