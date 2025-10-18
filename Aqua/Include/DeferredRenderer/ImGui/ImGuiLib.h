#pragma once
#include "../../Core/AqCore.h"
#include "../../Core/SharedRef.h"

#include "../Renderable/RenderTargetFactory.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"

struct ::GLFWwindow;

AQUA_BEGIN

struct ImGuiImageResource
{
	vkLib::ImageView ImageView;
	vkLib::Core::Ref<vk::Sampler> Sampler;
	VkDescriptorSet ImGuiImageID;
};

// only a single instance of this class is allowed
class ImGuiLib
{
public:
	static void BeginFrame()
	{ sInstance->BeginFrameImpl(); }

	static void EndFrame()
	{ sInstance->EndFrameImpl(); }

	static void UpdateDisplaySize()
	{ sInstance->UpdateDisplaySizeImpl(); }

	static void SetDisplayImage(const std::string& name, vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler = {})
	{ sInstance->SetDisplayImageImpl(name, view, sampler); }

	static const ImGuiImageResource& GetTexRsc(const std::string& name)
	{ return sInstance->GetTexRscImpl(name); }

	static const std::unordered_map<std::string, ImGuiImageResource>& GetImageRscs()
	{ return sInstance->mDisplayImages; }

	static ImGuiLib* GetInstance() { return sInstance; }

private:
	vkLib::Context mCtx;
	vkLib::DescriptorPoolManager mDescManager;
	vkLib::Core::DescriptorSetAllocator mDescAlloc;

	vkLib::Core::Ref<vk::DescriptorPool> mDescPool;

	vkLib::CommandPools mCommandPools;

	ImGui_ImplVulkan_InitInfo mImGuiInitInfo = {};

	vkLib::Core::Worker mRenderingWorker;
	vkLib::Core::Worker mPresenter;

	std::shared_ptr<vkLib::Swapchain> mSwapchain;

	RenderTargetFactory mRenderFactory;
	vkLib::Framebuffer mRenderbuffer;

	vkLib::Core::Ref<vk::Semaphore> mImGuiSignal;
	vk::CommandBuffer mImGuiCommandBuffer;

	vk::ClearValue mClearColor;

	ImGuiIO* mImGuiIO;

	// TODO: should be set by the client
	std::string mAssetDirectory = "E:\\Dev\\Aqua\\Aqua\\Assets\\";

	// resources
	std::unordered_map<std::string, ImGuiImageResource> mDisplayImages;
	vkLib::Core::Ref<vk::Sampler> mDefaultSampler;

private:
	AQUA_API void AllocateDescRscs();
	AQUA_API void SetupImGui(GLFWwindow* window);

private:
	AQUA_API void BeginFrameImpl();
	AQUA_API void EndFrameImpl();

	AQUA_API vk::ResultValue<uint32_t> FrameRenderImpl(ImDrawData* drawData);
	AQUA_API vk::Result FramePresentImpl(uint32_t frameIdx);

	AQUA_API void SetDisplayImageImpl(const std::string& name,
		vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler);

	const ImGuiImageResource& GetTexRscImpl(const std::string& name)
	{ return mDisplayImages.at(name); }

	AQUA_API void UpdateDisplaySizeImpl();
	AQUA_API void PrepareRenderFactory();
	AQUA_API void SetupTheme();
	AQUA_API void SetupDarkColorTheme();

	AQUA_API ImGuiLib(vkLib::Context ctx, GLFWwindow* window);
	AQUA_API ~ImGuiLib();

	ImGuiLib(const ImGuiLib&) = delete;
	ImGuiLib& operator=(const ImGuiLib&) = delete;

private:
	AQUA_API static ImGuiLib* sInstance;

	friend void ImGuiInit(vkLib::Context, GLFWwindow* window);
	friend void ImGuiShutdown();
};

static void ImGuiInit(vkLib::Context ctx, GLFWwindow* window)
{
	ImGuiLib::sInstance = new ImGuiLib(ctx, window);
}

static void ImGuiShutdown()
{
	delete ImGuiLib::sInstance;
	ImGuiLib::sInstance = nullptr;
}

AQUA_END
