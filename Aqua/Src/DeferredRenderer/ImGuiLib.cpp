#include "Core/Aqpch.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"
#include "Window/GLFW_Window.h"

#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

AQUA_NAMESPACE::ImGuiLib* AQUA_NAMESPACE::ImGuiLib::sInstance = nullptr;

AQUA_BEGIN
void ImGui_ImplVulkan_RenderWindowModified(ImGuiViewport* viewport, void*);
AQUA_END

AQUA_NAMESPACE::ImGuiLib::ImGuiLib(vkLib::Context ctx, GLFWwindow* window) : mCtx(ctx)
{
	_STL_VERIFY(!sInstance, "ImGui has already been initialized");

	mCtx = ctx;
	mSwapchain = ctx.GetSwapchain();

	mCommandPools = mCtx.CreateCommandPools();

	mImGuiCommandBuffer = mCommandPools[0].Allocate();
	mImGuiSignal = mCtx.CreateSemaphore();

	auto rscPool = mCtx.CreateResourcePool();
	mDefaultSampler = rscPool.CreateSampler({});

	PrepareRenderFactory();

	AllocateDescRscs();
	SetupImGui(window);

	SetupTheme();

	mRenderingWorker = ctx.FetchWorker(0);
	mPresenter = ctx.FetchWorker(0);

	mImGuiIO = &ImGui::GetIO();
}

AQUA_NAMESPACE::ImGuiLib::~ImGuiLib()
{
	mCtx.WaitIdle();

	mCommandPools[0].Free(mImGuiCommandBuffer);
	sInstance = nullptr;
}

void AQUA_NAMESPACE::ImGuiLib::AllocateDescRscs()
{
	mDescManager = mCtx.FetchDescriptorPoolManager();

	auto builder = mDescManager.GetBuilder();

	vkLib::Core::DescriptorSetAllocatorInfo allocInfo{};
	allocInfo.BatchSize = 1000;
	allocInfo.BindingCount = 100;
	allocInfo.Flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	allocInfo.Types.insert(vk::DescriptorType::eCombinedImageSampler);
	allocInfo.Types.insert(vk::DescriptorType::eStorageImage);
	allocInfo.Types.insert(vk::DescriptorType::eSampledImage);

	mDescPool = vkLib::Core::CreateRef<vk::DescriptorPool>([this, builder](vk::DescriptorPool& pool)
	{
		mCtx.WaitIdle();
		builder.Destroy(pool);
	}, builder.Build(allocInfo));
}

void AQUA_NAMESPACE::ImGuiLib::SetupImGui(GLFWwindow* window)
{
	// swapchain management is explicit

	vk::Device device = *mCtx.GetHandle();
	vkLib::PhysicalDevice physicalDevice = mCtx.GetDeviceInfo().PhysicalDevice;
	vk::Instance instance = *physicalDevice.ParentInstance;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// When view ports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer back ends
	ImGui_ImplGlfw_InitForVulkan(window, true);
	//init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
	mImGuiInitInfo.Instance = instance;
	mImGuiInitInfo.PhysicalDevice = physicalDevice.Handle;
	mImGuiInitInfo.Device = device;
	mImGuiInitInfo.QueueFamily = 0;
	// passing the (0, 0) queue
	mImGuiInitInfo.mWorker = mCtx.FetchWorker(0);
	mImGuiInitInfo.PipelineCache = nullptr;
	mImGuiInitInfo.DescriptorPool = *mDescPool;
	mImGuiInitInfo.RenderPass = mRenderbuffer.GetParentContext().GetNativeHandle();
	mImGuiInitInfo.Subpass = 0;
	mImGuiInitInfo.MinImageCount = 2;
	mImGuiInitInfo.ImageCount = static_cast<uint32_t>(mSwapchain->GetSwapchainData().Backbuffers.size());
	mImGuiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	mImGuiInitInfo.Allocator = nullptr;
	mImGuiInitInfo.CheckVkResultFn = [](VkResult err) { _STL_ASSERT(vk::Result(err) == vk::Result::eSuccess, "Core: ImGui Vulkan Err") };

	ImGui_ImplVulkan_Init(&mImGuiInitInfo);
}

void AQUA_NAMESPACE::ImGuiLib::BeginFrameImpl()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport();
}

void AQUA_NAMESPACE::ImGuiLib::EndFrameImpl()
{
	ImGui::Render();

	ImDrawData* main_draw_data = ImGui::GetDrawData();
	const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
	//mImGuiWindow.ClearValue.color.float32[0] = mClearColor.x * mClearColor.w;
	//mImGuiWindow.ClearValue.color.float32[1] = mClearColor.y * mClearColor.w;
	//mImGuiWindow.ClearValue.color.float32[2] = mClearColor.z * mClearColor.w;
	//mImGuiWindow.ClearValue.color.float32[3] = mClearColor.w;

	vk::ResultValue<uint32_t> resultVal(vk::Result::eSuccess, 0);

	if (!main_is_minimized)
		resultVal = FrameRenderImpl(main_draw_data);

	auto err = resultVal.result;

	if (err == vk::Result::eErrorOutOfDateKHR || err == vk::Result::eSuboptimalKHR)
	{

	}
	if (err == vk::Result::eErrorOutOfDateKHR)
	{
		return;
	}
	if (err != vk::Result::eErrorOutOfDateKHR)
		mImGuiInitInfo.CheckVkResultFn(VkResult(err));

	auto& platformIO = ImGui::GetPlatformIO();

	// this is the culprit
	// the default function, ImGui_ImplVulkan_RenderWindow(...), does not transition
	// the image rsc layout, so I wrote a modified version which handles the transitions properly
	platformIO.Renderer_RenderWindow = ImGui_ImplVulkan_RenderWindowModified;

	// Update and Render additional Platform Windows
	if (sInstance->mImGuiIO->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	// Present Main Platform Window
	if (!main_is_minimized)
		FramePresentImpl(resultVal.value);
}

vk::ResultValue<uint32_t> AQUA_NAMESPACE::ImGuiLib::FrameRenderImpl(ImDrawData* drawData)
{
	mRenderingWorker.WaitIdle();

	mImGuiCommandBuffer.reset();
	mImGuiCommandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	// before we begin the pass, we'll transition the image layouts
	for (const auto& [name, rsc] : mDisplayImages)
	{
		rsc.ImageView->BeginCommands(mImGuiCommandBuffer);
		rsc.ImageView->RecordTransitionLayout(vk::ImageLayout::eGeneral);
	}

	// begin the render pass
	vk::RenderPassBeginInfo info = {};
	info.renderPass = mRenderbuffer.GetParentContext().GetNativeHandle();
	info.framebuffer = mRenderbuffer.GetNativeHandle();
	info.renderArea.extent.width = mRenderbuffer.GetResolution().x;
	info.renderArea.extent.height = mRenderbuffer.GetResolution().y;
	info.clearValueCount = 1;
	info.setClearValues(mClearColor);

	mImGuiCommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(drawData, mImGuiCommandBuffer);

	// Submit command buffer
	mImGuiCommandBuffer.endRenderPass();

	// before we dispatch the commands, making sure we're reverting the image states back to prev ones
	for (const auto& [name, rsc] : mDisplayImages)
	{
		rsc.ImageView->EndCommands();
	}

	mImGuiCommandBuffer.end();

	auto submission = mRenderingWorker.Enqueue(*mImGuiSignal, mImGuiCommandBuffer);

	// copying and presenting the image
	auto resultValue = mSwapchain->AcquireNextFrame();
	auto renderPass = mSwapchain->GetRenderCtx().GetNativeHandle();

	vk::Result err = resultValue.result;

	if (err != vk::Result::eSuccess)
		return resultValue;

	auto frame = mSwapchain->operator[](resultValue.value);
	auto semaphoreFrame = mSwapchain->GetSemaphoreFrame();

	vk::PipelineStageFlags waitStages[] = 
	{ vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe };

	vk::Semaphore waitSignals[] =
	{ *semaphoreFrame.ImageAcquiredSemaphore, *mImGuiSignal };


	frame.CommandBuffer.reset();
	frame.CommandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	frame.Backbuffer.BeginCommands(frame.CommandBuffer);
	frame.Backbuffer.RecordBlit(*mRenderbuffer.GetColorAttachments().front(), { vk::Filter::eLinear });
	frame.Backbuffer.EndCommands();

	frame.CommandBuffer.end();

	vk::SubmitInfo submitInfo = {};
	submitInfo.setWaitSemaphores(waitSignals);
	submitInfo.setWaitDstStageMask(waitStages);
	submitInfo.setCommandBuffers(frame.CommandBuffer);
	submitInfo.setSignalSemaphores(*semaphoreFrame.RenderCompleteSemaphore);

	mPresenter[0]->Submit(submitInfo, *frame.Fence);
	mCtx.WaitForFences(*frame.Fence); // waiting right away to reset the image acquire semaphore

	return resultValue;
}

vk::Result AQUA_NAMESPACE::ImGuiLib::FramePresentImpl(uint32_t frameIdx)
{
	auto semaphoreFrame = mSwapchain->GetSemaphoreFrame();

	mSwapchain->NextFrame();

	vk::PresentInfoKHR presentInfo{};
	presentInfo.setImageIndices(frameIdx);
	presentInfo.setSwapchains(*mSwapchain->GetHandle());
	presentInfo.setWaitSemaphores(*semaphoreFrame.RenderCompleteSemaphore);

	return mRenderingWorker[0]->PresentKHR(presentInfo);
}

void AQUA_NAMESPACE::ImGuiLib::SetDisplayImageImpl(const std::string& name, 
	vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler)
{
	auto activeSampler = sampler ? sampler : mDefaultSampler;

	if (mDisplayImages.find(name) != mDisplayImages.end())
	{
		ImGui_ImplVulkan_RemoveTexture(mDisplayImages[name].ImGuiImageID);
		mDisplayImages.erase(name);
	}

	if (!view)
		return;

	mDisplayImages[name].ImGuiImageID = ImGui_ImplVulkan_AddTexture(*activeSampler,
		view.GetNativeHandle(), 
		(VkImageLayout)vk::ImageLayout::eGeneral);

	mDisplayImages[name].ImageView = view;
	mDisplayImages[name].Sampler = activeSampler;
}

void AQUA_NAMESPACE::ImGuiLib::UpdateDisplaySizeImpl()
{
	glm::ivec2 displaySize = { mSwapchain->GetInfo().Width, mSwapchain->GetInfo().Height };

	mImGuiIO->DisplaySize = ImVec2((float)mSwapchain->GetInfo().Width, (float)mSwapchain->GetInfo().Height);
	mRenderFactory.SetTargetSize(displaySize);

	if(displaySize.x != 0 && displaySize.y != 0)
		mRenderbuffer = *mRenderFactory.CreateFramebuffer();
}

void AQUA_NAMESPACE::ImGuiLib::PrepareRenderFactory()
{
	mRenderFactory.SetContextBuilder(mCtx.FetchRenderContextBuilder(vk::PipelineBindPoint::eGraphics));

	mRenderFactory.AddColorAttribute("ImGuiColor", "RGBA8Un");
	mRenderFactory.SetAllColorProperties(vk::AttachmentLoadOp::eClear);

	_STL_VERIFY(mRenderFactory.Validate(), "Couldn't create ImGUi render buffer");

	mRenderFactory.SetTargetSize({ mSwapchain->GetInfo().Width, mSwapchain->GetInfo().Height });
	mRenderbuffer = *mRenderFactory.CreateFramebuffer();
}

void AQUA_NAMESPACE::ImGuiLib::SetupTheme()
{
	ImGuiIO& io = ImGui::GetIO();

	std::string RegularFontFile = mAssetDirectory + std::string("Fonts\\comic.ttf");
	std::string BoldFontFile = mAssetDirectory + std::string("Fonts\\comicbd.ttf");

	io.Fonts->AddFontFromFileTTF(BoldFontFile.c_str(), 18.0f);
	io.FontDefault = io.Fonts->AddFontFromFileTTF(RegularFontFile.c_str(), 18.0f);

	SetupDarkColorTheme();
}

void AQUA_NAMESPACE::ImGuiLib::SetupDarkColorTheme()
{
	auto& colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.105f, 0.11f, 1.0f);

	// Headers
	colors[ImGuiCol_Header] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

	// Buttons
	colors[ImGuiCol_Button] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.105f, 0.11f, 1.0f);

	// Frame BG
	colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.105f, 0.11f, 1.0f);

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.3805f, 0.381f, 1.0f);
	colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.2805f, 0.281f, 1.0f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

	// Title BG
	colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.95f, 0.1505f, 0.951f, 1.0f);
}

