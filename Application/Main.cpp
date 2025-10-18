#include "Application.h"
#include "DeferredRenderer/Renderer/Renderer.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"
#include "Utils/EditorCamera.h"

#include "Geometry3D/MeshLoader.h"

class Sandbox : public Application
{
public:
	Sandbox(const ApplicationCreateInfo& createInfo)
		: Application(createInfo), mRenderer(*mContext, GetAssetDirectory() / "Shaders" / "Deferred"), mMaterialSystem(*mContext, GetAssetDirectory() / "Shaders" / "Deferred") { }

	bool OnStart() override
	{
		Aqua::Line line{};
		line.Begin.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		line.Begin.Position = { 0.0f, 0.0f, 0.0f, 1.0f };

		line.End.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		line.End.Position = { 100.0f, 100.0f, 0.0f, 1.0f };

		Aqua::MeshLoader loader(aiProcess_Triangulate | aiProcess_CalcTangentSpace);
		Aqua::Geometry3D cube = loader.LoadModel((GetAssetDirectory() / "Models" / "Cube.obj").string());

		auto envRef = Aqua::MakeRef<Aqua::Environment>();

		Aqua::DirectionalLightInfo lightInfo{};
		lightInfo.SrcInfo.Color = { 10.0f, 10.0f, 10.0f, 10.0f };
		lightInfo.SrcInfo.Direction = { 1.0f, -1.0f, 1.0f, 1.0f };

		lightInfo.CubeSize = glm::vec3(25.0f, 25.0f, 25.0f);
		lightInfo.Position = glm::vec3(0.0f, -0.0f, 0.0f);

		envRef->SubmitLightSrc(lightInfo);

		mRenderer.SetEnvironment(envRef);

		mRenderer.SetShadowConfig({});
		mRenderer.EnableFeatures(Aqua::RenderingFeature::eShadow);
		mRenderer.PrepareFeatures();
		mMaterialSystem.SetPBRRenderProperties(mRenderer.GetShadingbuffer().GetParentContext());

		auto instance = *mMaterialSystem[TEMPLATE_PBR];

		Aqua::SetMaterialPar(instance, "base_color", glm::vec3(0.6f));
		Aqua::SetMaterialPar(instance, "roughness", 0.3f);
		Aqua::SetMaterialPar(instance, "metallic", 0.1f);
		Aqua::SetMaterialPar(instance, "refract_idx", 7.5f);

		//mRenderer.SubmitLines("lines", line, 2.0f);
		mRenderer.SubmitRenderable("cube", glm::mat4(1.0f), cube[0], instance);
		mRenderer.SubmitRenderable("another_cube", glm::mat4(1.0f), cube[1], instance);

		mRenderer.PrepareMaterialNetwork();

		mRenderer.ActivateAll();

		mRenderer.InvalidateBuffers();
		mRenderer.WaitIdle();

		mRenderer.UpdateDescriptors();

		Aqua::ImGuiLib::SetDisplayImage("renderer", mRenderer.GetShadingbuffer().GetColorAttachments().front());

		return true;
	}

	bool OnUpdate(std::chrono::nanoseconds elaspedTime) override
	{
		mRenderer.SetCamera(mCamera.GetProjectionMatrix(), mCamera.GetViewMatrix());

		static glm::vec3 vel = glm::vec3(1.0f, 0.0f, 0.0f);
		static glm::vec3 pos = glm::vec3(0.0f);

		pos += vel * (float)elaspedTime.count() / (float) 1e9;

		glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

		mRenderer.ModifyRenderable("cube", model);
		mRenderer.InvalidateBuffers();
		mRenderer.WaitIdle();
		mRenderer.UpdateDescriptors();

		mRenderer.IssueDrawCall();
		mRenderer.WaitIdle();

		return true;
	}

	bool OnUIUpdate(std::chrono::nanoseconds elaspedTime) override
	{
		Aqua::ImGuiLib::BeginFrame();

		ImGui::Begin("My Window");
		auto windowSize = ImGui::GetWindowSize();

		//mCamera.SetCameraSpec2D({ 0.0f, 0.0f, 0.0f }, { windowSize.x, -windowSize.y, 1.0f });
		//mCamera.SetPosition({ 0.0f, 0.0f, 0.0f });

		Aqua::EditorCamera3DSpecs cameraSpecs;
		cameraSpecs.Fov = glm::radians(90.0f);
		cameraSpecs.NearClip = 0.1f;
		cameraSpecs.FarClip = 100.0f;
		cameraSpecs.AspectRatio = windowSize.x / windowSize.y;

		mCamera.SetCameraSpec3D(cameraSpecs);

		MoveCamera(mCamera, elaspedTime);

		ImGui::Image(Aqua::ImGuiLib::GetTexRsc("renderer").ImGuiImageID, windowSize);

		ImGui::End();

		Aqua::ImGuiLib::EndFrame();
		return true;
	}

private:
	Aqua::Renderer mRenderer;
	Aqua::MaterialSystem mMaterialSystem;

	Aqua::EditorCamera mCamera;
};

int main()
{
	ApplicationCreateInfo createInfo{};
	createInfo.AppName = "Aqua";
	createInfo.EngineName = "Aqua";
	createInfo.FramesPerSeconds = std::numeric_limits<float>::max();
	createInfo.WindowInfo.fullScr = true;
	createInfo.WindowInfo.height = 900;
	createInfo.WindowInfo.width = 1600;
	createInfo.WindowInfo.vSync = false;
	createInfo.WindowInfo.name = "Learning it the hard way";

	createInfo.WorkerCount = -1;

	createInfo.EnableValidationLayers = true;

	Aqua::SharedRef<Application> app = Aqua::MakeRef<Sandbox>(createInfo);

	app->Run();
}
