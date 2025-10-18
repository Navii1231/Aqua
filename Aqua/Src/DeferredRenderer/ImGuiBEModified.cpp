#include "Core/Aqpch.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
static PFN_vkCmdBeginRenderingKHR   ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR;
static PFN_vkCmdEndRenderingKHR     ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR;
#endif

// mirroring the vulkan data structures and some backend functionality
// 
// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
struct ImGui_ImplVulkan_FrameRenderBuffers
{
	VkDeviceMemory      VertexBufferMemory;
	VkDeviceMemory      IndexBufferMemory;
	VkDeviceSize        VertexBufferSize;
	VkDeviceSize        IndexBufferSize;
	VkBuffer            VertexBuffer;
	VkBuffer            IndexBuffer;
};

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct ImGui_ImplVulkan_WindowRenderBuffers
{
	uint32_t            Index;
	uint32_t            Count;
	ImVector<ImGui_ImplVulkan_FrameRenderBuffers> FrameRenderBuffers;
};

struct ImGui_ImplVulkan_Texture
{
	VkDeviceMemory              Memory;
	VkImage                     Image;
	VkImageView                 ImageView;
	VkDescriptorSet             DescriptorSet;

	ImGui_ImplVulkan_Texture() { memset((void*)this, 0, sizeof(*this)); }
};

// For multi-viewport support:
// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplVulkan_ViewportData
{
	ImGui_ImplVulkanH_Window                Window;                 // Used by secondary viewports only
	ImGui_ImplVulkan_WindowRenderBuffers    RenderBuffers;          // Used by all viewports
	bool                                    WindowOwned;
	bool                                    SwapChainNeedRebuild;   // Flag when viewport swapchain resized in the middle of processing a frame
	bool                                    SwapChainSuboptimal;    // Flag when VK_SUBOPTIMAL_KHR was returned.

	ImGui_ImplVulkan_ViewportData() { WindowOwned = SwapChainNeedRebuild = SwapChainSuboptimal = false; memset((void*)&RenderBuffers, 0, sizeof(RenderBuffers)); }
	~ImGui_ImplVulkan_ViewportData() {}
};

// Vulkan data
struct ImGui_ImplVulkan_Data
{
	ImGui_ImplVulkan_InitInfo   VulkanInitInfo;
	VkDeviceSize                BufferMemoryAlignment;
	VkDeviceSize                NonCoherentAtomSize;
	VkPipelineCreateFlags       PipelineCreateFlags;
	VkDescriptorSetLayout       DescriptorSetLayout;
	VkPipelineLayout            PipelineLayout;
	VkPipeline                  Pipeline;               // pipeline for main render pass (created by app)
	VkPipeline                  PipelineForViewports;   // pipeline for secondary viewports (created by backend)
	VkShaderModule              ShaderModuleVert;
	VkShaderModule              ShaderModuleFrag;
	VkDescriptorPool            DescriptorPool;

	// Texture management
	VkSampler                   TexSampler;
	VkCommandPool               TexCommandPool;
	VkCommandBuffer             TexCommandBuffer;

	// Render buffers for main window
	ImGui_ImplVulkan_WindowRenderBuffers MainWindowRenderBuffers;

	ImGui_ImplVulkan_Data()
	{
		memset((void*)this, 0, sizeof(*this));
		BufferMemoryAlignment = 256;
		NonCoherentAtomSize = 64;
	}
};

ImGui_ImplVulkan_Data* ImGui_ImplVulkan_GetBackendData()
{
	return (ImGui_ImplVulkan_Data*)ImGui::GetIO().BackendRendererUserData;
}

void check_vk_result(VkResult err)
{
	ImGui_ImplVulkan_Data* bd = ImGui_ImplVulkan_GetBackendData();

	if (!bd)
		return;

	ImGui_ImplVulkan_InitInfo* v = &bd->VulkanInitInfo;
	if (v->CheckVkResultFn)
		v->CheckVkResultFn(err);
}

AQUA_BEGIN

void TransitionImageRscLayouts(vk::CommandBuffer CommandBuffer, vk::ImageLayout layout)
{
	const auto& imageRscs = ImGuiLib::GetImageRscs();

	for (const auto& [name, rsc] : imageRscs)
	{
		auto currLayout = rsc.ImageView->GetConfig().CurrLayout;

		if (currLayout == layout)
			continue;

		vk::ImageMemoryBarrier memBar{};
		memBar.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		memBar.setDstAccessMask({});
		memBar.setOldLayout(currLayout);
		memBar.setNewLayout(layout);
		memBar.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		memBar.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		memBar.setImage(rsc.ImageView->GetHandle());
		memBar.setSubresourceRange(rsc.ImageView->GetSubresourceRanges().front());

		CommandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eTopOfPipe,
			{}, {}, {}, memBar);
	}
}

void RevertImageRscLayouts(vk::CommandBuffer CommandBuffer, vk::ImageLayout oldLayout)
{
	const auto& imageRscs = ImGuiLib::GetImageRscs();

	for (const auto& [name, rsc] : imageRscs)
	{
		auto currLayout = rsc.ImageView->GetConfig().CurrLayout;

		if (currLayout == oldLayout)
			continue;

		vk::ImageMemoryBarrier memBar{};
		memBar.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		memBar.setDstAccessMask({});
		memBar.setOldLayout(oldLayout);
		memBar.setNewLayout(currLayout);
		memBar.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		memBar.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
		memBar.setImage(rsc.ImageView->GetHandle());
		memBar.setSubresourceRange(rsc.ImageView->GetSubresourceRanges().front());

		CommandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eTopOfPipe,
			{}, {}, {}, memBar);
	}
}

void ImGui_ImplVulkan_RenderWindowModified(ImGuiViewport* viewport, void*)
{
	ImGui_ImplVulkan_Data* bd = ImGui_ImplVulkan_GetBackendData();
	ImGui_ImplVulkan_ViewportData* vd = (ImGui_ImplVulkan_ViewportData*)viewport->RendererUserData;
	ImGui_ImplVulkanH_Window* wd = &vd->Window;
	ImGui_ImplVulkan_InitInfo* v = &bd->VulkanInitInfo;
	VkResult err;

	if (vd->SwapChainNeedRebuild || vd->SwapChainSuboptimal)
	{
		ImGui_ImplVulkanH_CreateOrResizeWindow(v->Instance, v->PhysicalDevice, v->Device, wd, v->QueueFamily, v->Allocator, (int)viewport->Size.x, (int)viewport->Size.y, v->MinImageCount);
		vd->SwapChainNeedRebuild = vd->SwapChainSuboptimal = false;
	}

	ImGui_ImplVulkanH_Frame* fd = nullptr;
	ImGui_ImplVulkanH_FrameSemaphores* fsd = &wd->FrameSemaphores[wd->SemaphoreIndex];
	{
		{
			err = ::vkAcquireNextImageKHR(v->Device, wd->Swapchain, UINT64_MAX, fsd->ImageAcquiredSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);
			if (err == VK_ERROR_OUT_OF_DATE_KHR)
			{
				vd->SwapChainNeedRebuild = true; // Since we are not going to swap this frame anyway, it's ok that recreation happens on next frame.
				return;
			}
			if (err == VK_SUBOPTIMAL_KHR)
				vd->SwapChainSuboptimal = true;
			else
				check_vk_result(err);
			fd = &wd->Frames[wd->FrameIndex];
		}
		for (;;)
		{
			err = vkWaitForFences(v->Device, 1, &fd->Fence, VK_TRUE, 100);
			if (err == VK_SUCCESS) break;
			if (err == VK_TIMEOUT) continue;
			check_vk_result(err);
		}
		{
			err = vkResetCommandPool(v->Device, fd->CommandPool, 0);
			check_vk_result(err);
			VkCommandBufferBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
			check_vk_result(err);
		}
		{
			ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
			memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
		}

		// our modification
		// ...inserting the pipeline barriers to transition the layouts
		TransitionImageRscLayouts(fd->CommandBuffer, vk::ImageLayout::eGeneral);

	#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
		if (v->UseDynamicRendering)
		{
			// Transition swapchain image to a layout suitable for drawing.
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.image = fd->Backbuffer;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(fd->CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkRenderingAttachmentInfo attachmentInfo = {};
			attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			attachmentInfo.imageView = fd->BackbufferView;
			attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
			attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentInfo.clearValue = wd->ClearValue;

			VkRenderingInfo renderingInfo = {};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
			renderingInfo.renderArea.extent.width = wd->Width;
			renderingInfo.renderArea.extent.height = wd->Height;
			renderingInfo.layerCount = 1;
			renderingInfo.viewMask = 0;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &attachmentInfo;

			ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR(fd->CommandBuffer, &renderingInfo);
		} else
		#endif
		{
			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = wd->RenderPass;
			info.framebuffer = fd->Framebuffer;
			info.renderArea.extent.width = wd->Width;
			info.renderArea.extent.height = wd->Height;
			info.clearValueCount = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? 0 : 1;
			info.pClearValues = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? nullptr : &wd->ClearValue;
			vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		}
	}

	ImGui_ImplVulkan_RenderDrawData(viewport->DrawData, fd->CommandBuffer, bd->PipelineForViewports);

	{
	#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
		if (v->UseDynamicRendering)
		{
			ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR(fd->CommandBuffer);

			// Transition image to a layout suitable for presentation
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.image = fd->Backbuffer;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(fd->CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		} else
		#endif
		{
			vkCmdEndRenderPass(fd->CommandBuffer);
		}

		// our modification
		// reverting the transitions
		RevertImageRscLayouts(fd->CommandBuffer, vk::ImageLayout::eGeneral);

		// continue...

		{
			// this could be replaced...
			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &fsd->ImageAcquiredSemaphore;
			info.pWaitDstStageMask = &wait_stage;
			info.commandBufferCount = 1;
			info.pCommandBuffers = &fd->CommandBuffer;
			info.signalSemaphoreCount = 1;
			info.pSignalSemaphores = &fsd->RenderCompleteSemaphore;

			err = vkEndCommandBuffer(fd->CommandBuffer);
			check_vk_result(err);
			//err = vkResetFences(v->Device, 1, &fd->Fence);
			//check_vk_result(err);

			// #NOTE: BE Modified
			err = (VkResult)v->mWorker.NextQueue()->Submit(vk::SubmitInfo(info), fd->Fence);

			//err = vkQueueSubmit(v->Queue, 1, &info, fd->Fence);
			check_vk_result(err);
		}
	}
}

AQUA_END
