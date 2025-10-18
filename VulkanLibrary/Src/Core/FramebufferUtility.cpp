#include "Core/vkpch.h"
#include "Core/Utils/FramebufferUtils.h"

vk::RenderPass VK_NAMESPACE::VK_CORE::VK_UTILS::CreateRenderPass(
	vk::Device device, const std::vector<ImageAttachmentConfig>& attachments,
	bool depthIncluded, vk::PipelineBindPoint BindPoint)
{
	// For now, we're using one subpass only
	vk::SubpassDescription subpassDesc{};
	std::vector<vk::AttachmentReference> Refs;
	vk::AttachmentReference depthStencilRef;

	if (!attachments.empty())
	{
		for (uint32_t i = 0; i < attachments.size() - depthIncluded; i++)
		{
			auto& Recent = Refs.emplace_back();
			Recent.attachment = i;
			Recent.setLayout(attachments[i].Layout);
		}

		subpassDesc.setColorAttachments(Refs);
		subpassDesc.setPipelineBindPoint(BindPoint);

		depthStencilRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
		depthStencilRef.layout = attachments.back().Layout;

		if (depthIncluded)
			subpassDesc.setPDepthStencilAttachment(&depthStencilRef);
	}

	vk::RenderPassCreateInfo createInfo{};

	std::vector<vk::AttachmentDescription> descs;

	for (auto& attachment : attachments)
		descs.emplace_back(attachment.AttachmentDesc);

	createInfo.setAttachments(descs);
	createInfo.setSubpasses(subpassDesc);

	return device.createRenderPass(createInfo);
}

vk::ShaderModule VK_NAMESPACE::VK_CORE::VK_UTILS::CreateShaderModule(
	vk::Device device, const ShaderSPIR_V& shaders)
{
	vk::ShaderModuleCreateInfo createInfo{};
	createInfo.setCode(shaders.ByteCode);

	return device.createShaderModule(createInfo);
}

std::vector<vk::PipelineShaderStageCreateInfo> VK_NAMESPACE::VK_CORE::
VK_UTILS::CreatePipelineShaderStages(
	vk::Device device, const std::vector<VK_NAMESPACE::ShaderSPIR_V>& ShaderStages)
{
	std::vector<vk::PipelineShaderStageCreateInfo> Infos(ShaderStages.size());

	for (size_t i = 0; i < Infos.size(); i++)
	{
		auto& Info = Infos[i];
		auto& Shader = ShaderStages[i];

		Info.setPName("main");
		Info.setStage(Shader.Stage);

		vk::ShaderModuleCreateInfo moduleInfo{};
		moduleInfo.setCode(Shader.ByteCode);

		Info.module = CreateShaderModule(device, Shader);
	}

	return Infos;
}

