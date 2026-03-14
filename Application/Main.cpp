#include "Application.h"
#include "DeferredRenderer/Renderer/Renderer.h"
#include "DeferredRenderer/ImGui/ImGuiLib.h"
#include "Utils/EditorCamera.h"

#include "Execution/ComputeDraft.h"

#include "Geometry3D/MeshLoader.h"

#include "Utils/ThreadPool.h"
#include "Layer.h"

#include "Machine Learning/DecisionTree.h"
#include "Machine Learning/RegressTree.h"
#include "Machine Learning/LogisticRegress.h"
#include "Machine Learning/AutoDiff.h"
#include "Parser/ExprParser.h"
#include "Parser/IRNeuralDraft.h"

class Sandbox : public Layer
{
public:
	Sandbox(vkLib::Context ctx, const std::filesystem::path path)
		: mContext(ctx), mPath(path), mRenderer(ctx, path / "Shaders" / "Deferred"), mMaterialSystem(ctx, path / "Shaders" / "Deferred") {
	}

	bool OnStart() override
	{
		TestNeuralDraft();

		ExprGraph graph{};
		Aqua::Exec::UniqueIDGen idGen{};
		AutoDiffInventory inventory{};

		const auto& idxMap = inventory.GetIdxMap();

		auto parseFn = [&inventory](const std::string& str)
			{
				ExprParser parser{};
				parser.SetString(str);
				parser.SetOpMap(inventory.GetOpMap());

				return *parser.Parse();
			};

		std::string sigmoid = "1.0 / (1.0 + exp(-x))";

		ExprParser parser;

		parser.SetString(sigmoid);
		parser.SetOpMap(inventory.GetOpMap());

		auto expNode = parser.Parse();
		auto expGraph = *expNode;

		graph = CloneByValue(idGen, expGraph);

		std::unordered_map<std::string, Aqua::Exec::BasicGraph<ExprNodeRef>> basicFns{};
		basicFns["sin"] = parseFn("cos(x)");
		basicFns["cos"] = parseFn("sin(x)");
		basicFns["tan"] = parseFn("sec(x) * sec(x)");
		basicFns["asin"] = parseFn("1.0 / ((1.0 - x ^ 2.0) ^ (1.0 / 2.0))");
		basicFns["acos"] = parseFn("-1.0 / ((1.0 - x ^ 2.0) ^ (1.0 / 2.0))");
		basicFns["atan"] = parseFn("1.0 / (1.0 + x ^ 2.0)");
		basicFns["sinh"] = parseFn("cosh(x)");
		basicFns["cosh"] = parseFn("sinh(x)");
		basicFns["tanh"] = parseFn("sech(x) * sech(x)");
		basicFns["ln"] = parseFn("1.0 / x");
		basicFns["exp"] = parseFn("exp(x)");

		ExprParser::MyAutoDiff autoDiff{};

		autoDiff.SetIDGen([&idGen]() { return idGen(); });
		autoDiff.SetExpr(graph);
		autoDiff.SetInputVariables(graph.InputNodes);

		autoDiff.SetDerivFn([&graph, &idxMap, &basicFns, &idGen](Aqua::Exec::NodeID nodeID, Aqua::Exec::NodeID childID)->Aqua::Exec::BasicGraph<ExprNodeRef>
			{
				if (graph[nodeID].mInfo == NodeType::eOp)
				{
					const auto& opInfo = std::get<OpInfo>(graph[nodeID].mVar);
					const auto& [info, fn] = idxMap.at(opInfo.Op);

					// TODO: make sure the functions inputs are placed properly
					return fn([&idGen]() { return idGen(); }, graph, nodeID, childID);
				}

				// we are left with functions or variables
				// for this we need a map to get the corresponding 
				// derivative of a function. The only difficult part
				// is getting the operators right.

				auto cloned = CloneByValue(idGen, basicFns[std::get<OpInfo>(graph[nodeID].mVar).Op]);

				auto stitch = cloned.InputNodes.front();
				cloned.InputNodes.clear();

				auto [rdm, state] = CloneExByRef(cloned, graph, GetChildren(*graph.Nodes[nodeID])[childID]);

				cloned.Nodes[stitch] = cloned.Nodes[rdm];
				cloned.Nodes[stitch]->SetNodeID(stitch);

				cloned.Nodes.erase(rdm);

				return cloned;
			});

		auto derivGraph = autoDiff.Apply();

#if 0
		// weight, height and selection
		std::tuple<std::vector<float>, std::vector<float>, std::vector<Index>> milSel{};

		std::get<0>(milSel).emplace_back(55.0f);
		std::get<0>(milSel).emplace_back(65.0f);
		std::get<0>(milSel).emplace_back(75.0f);
		std::get<0>(milSel).emplace_back(85.0f);
		std::get<0>(milSel).emplace_back(75.0f);
		std::get<0>(milSel).emplace_back(60.0f);
		std::get<0>(milSel).emplace_back(65.0f);
		std::get<0>(milSel).emplace_back(70.0f);
		std::get<0>(milSel).emplace_back(75.0f);

		std::get<1>(milSel).emplace_back(165.0f);
		std::get<1>(milSel).emplace_back(155.0f);
		std::get<1>(milSel).emplace_back(165.0f);
		std::get<1>(milSel).emplace_back(170.0f);
		std::get<1>(milSel).emplace_back(180.0f);
		std::get<1>(milSel).emplace_back(190.0f);
		std::get<1>(milSel).emplace_back(175.0f);
		std::get<1>(milSel).emplace_back(165.0f);
		std::get<1>(milSel).emplace_back(185.0f);

		LogisticRegress regress{};
		regress.SetData({ 0, 1, 2, 3, 4, 5, 6, 7, 8 });
		regress[0].NumFn = [&milSel](Index idx)
			{
				return std::get<0>(milSel)[idx];
			};
		
		regress[1].NumFn = [&milSel](Index idx)
			{
				return std::get<1>(milSel)[idx];
			};

		regress.SetPredicate([&milSel](Index idx)
			{
				return std::get<0>(milSel)[idx] > 70.0f && std::get<1>(milSel)[idx] > 170.0f;
			});

		regress.Init(0.0f, 1.0f / 100.0f);

		auto probs = regress.Forward(0);

		regress.ExecuteEpoch(0.00001f);
		regress.ExecuteEpoch(0.00001f);
		regress.ExecuteEpoch(0.00001f);
		regress.ExecuteEpoch(0.00001f);
		regress.ExecuteEpoch(0.00001f);
#endif

#if 0
		IndexTable table{};

		table(0, 0) = true;
		table(1, 0) = false;
		table(2, 0) = true;
		table(3, 0) = true;
		table(4, 0) = true;

		table(0, 1) = false;
		table(1, 1) = true;
		table(2, 1) = true;
		table(3, 1) = true;
		table(4, 1) = false;

		table(0, 2) = false;
		table(1, 2) = false;
		table(2, 2) = false;
		table(3, 2) = false;
		table(4, 2) = false;

		table(0, 3) = true;
		table(1, 3) = false;
		table(2, 3) = false;
		table(3, 3) = false;
		table(4, 3) = false;

		table(0, 4) = true;
		table(1, 4) = true;
		table(2, 4) = true;
		table(3, 4) = false;
		table(4, 4) = true;

		table(0, 5) = false;
		table(1, 5) = true;
		table(2, 5) = false;
		table(3, 5) = true;
		table(4, 5) = false;

		table(0, 6) = true;
		table(1, 6) = false;
		table(2, 6) = false;
		table(3, 6) = false;
		table(4, 6) = false;

		table(0, 7) = true;
		table(1, 7) = true;
		table(2, 7) = true;
		table(3, 7) = false;
		table(4, 7) = true;

		table(0, 8) = false;
		table(1, 8) = true;
		table(2, 8) = true;
		table(3, 8) = true;
		table(4, 8) = false;

		table(0, 9) = true;
		table(1, 9) = false;
		table(2, 9) = false;
		table(3, 9) = true;
		table(4, 9) = false;

		table(0, 10) = true;
		table(1, 10) = false;
		table(2, 10) = true;
		table(3, 10) = false;
		table(4, 10) = true;

		table(0, 11) = false;
		table(1, 11) = true;
		table(2, 11) = false;
		table(3, 11) = true;
		table(4, 11) = false;

		// training

		auto GetFeature = [&table](Index featIdx, Index id)
			{
				return table(featIdx, id);
			};

		Bucket bucket{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

		DecisionTree draft{};

		draft.SetDataSet(bucket);
		draft.SetPredicate(std::bind(GetFeature, 4, std::placeholders::_1));

		// mall, market, mood, time
		for (Index i = 0; i < 4; i++)
		{
			draft[i].FeatFn = std::bind(GetFeature, i, std::placeholders::_1);
		}

		auto root = draft.Construct();
#endif

		auto buffer = mContext.CreateResourcePool().CreateGenericBuffer();

		buffer.Resize(100);

		std::vector<float> localBuf({ 1, 2, 3, 6, 6 });

		buffer << localBuf;

		Aqua::Line line{};
		line.Begin.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		line.Begin.Position = { 0.0f, 0.0f, 0.0f, 1.0f };

		line.End.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		line.End.Position = { 100.0f, 100.0f, 0.0f, 1.0f };

		Aqua::MeshLoader loader(aiProcess_Triangulate | aiProcess_CalcTangentSpace);
		Aqua::Geometry3D cube = loader.LoadModel((mPath / "Models" / "Cube.obj").string());

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

#if 0

		std::string initialization = R"(
		
		SET WorkGroupSize = { 256, 1, 1 }
		SET InvocationCount = { 1, 1, 1 }

		SET KernelConsts
		{
			u32 pSplits;
		};

		shared_buffer<fp32> sActiveBuffer(0, 0);

		function Evaluate()->void
		{
			u32 globalID = gl_GlobalInvocationID.x;

			if (globalID >= pSplits)
				return;

			sActiveBuffer[globalID] = 0.5 * fp32(globalID);
		}

		)";

		std::string step = R"(
		
		SET WorkGroupSize = { 256, 1, 1 }
		SET InvocationCount = { 1, 1, 1 }
		
		SET KernelConsts
		{
			u32 pSplits;
		};

		shared_buffer<fp32> sInactiveBuffer(0, 0);
		shared_buffer<fp32> sActiveBuffer(0, 1);

		function Laplacian(fp32 x0, fp32 x_1, fp32 x1)->fp32
		{
			return (x_1 + x1) / 2 - x0;
		}

		function GetValue(u32 idx)->fp32
		{
			return sInactiveBuffer[idx];
		}

		function SetValue(u32 idx, fp32 val)->void
		{
			sActiveBuffer[idx] = val;
		}
		
		function Evaluate()->void
		{
			u32 globalID = gl_GlobalInvocationID.x;
			
			if(globalID >= pSplits)
				return;

			fp32 x0 = GetValue(globalID);
			fp32 x_1 = GetValue(globalID - 1);
			fp32 x1 = GetValue(globalID + 1);

			SetValue(globalID, Laplacian(x0, x_1, x1));
		}

		)";

		std::string swapBuffers = R"(

		SET WorkGroupSize = {256, 1, 1}
		SET InvocationCount = {1, 1, 1}

		SET KernelConsts
		{
			u32 pSplits;
		};

		shared_buffer<fp32> sInactiveBuffer(0, 0);
		shared_buffer<fp32> sActiveBuffer(0, 1);

		function Evaluate()->void
		{
			u32 globalID = gl_GlobalInvocationID.x;

			if (globalID >= pSplits)
				return;

			sInactiveBuffer[globalID] = sActiveBuffer[globalID];
		}

		)";

		// testing new execution system
		Aqua::Exec::ComputeDraft draft(mContext);

		draft[0] = initialization;
		auto initializationProcess = *draft.Construct({ 0 });

		draft.Clear();

		draft[0] = step;
		draft[1] = swapBuffers;

		draft.Connect(0, 1, vk::PipelineStageFlagBits::eTopOfPipe);

		auto simulation = *draft.Construct({ 1 });

		auto rscPool = mContext.CreateResourcePool();
		auto execUnits = mContext.CreateCommandPools()[0].CreateExecUnits(2);

		auto firstBuf = rscPool.CreateBuffer<float>(vk::BufferUsageFlagBits::eStorageBuffer, 32);
		auto secondBuf = rscPool.CreateBuffer<float>(vk::BufferUsageFlagBits::eStorageBuffer, 32);

		Aqua::Exec::SetComputeRsc(initializationProcess[0], 0, 0, firstBuf);
		Aqua::Exec::SetKernelConst(initializationProcess[0], 0, 32);

		Aqua::Exec::SetComputeRsc(simulation[0], 0, 0, firstBuf);
		Aqua::Exec::SetComputeRsc(simulation[0], 0, 1, secondBuf);
		Aqua::Exec::SetKernelConst(simulation[0], 0, 32);

		Aqua::Exec::SetComputeRsc(simulation[1], 0, 0, firstBuf);
		Aqua::Exec::SetComputeRsc(simulation[1], 0, 1, secondBuf);
		Aqua::Exec::SetKernelConst(simulation[1], 0, 32);

		initializationProcess.Update();
		simulation.Update();

		Aqua::Exec::Execute(initializationProcess.SortEntries(), execUnits);
		Aqua::Exec::WaitFor(execUnits);

		Aqua::Exec::Execute(simulation.SortEntries(), execUnits);
		Aqua::Exec::WaitFor(execUnits);

		std::vector<float> buf{};

		firstBuf >> buf;
		secondBuf >> buf;

		std::for_each(buf.begin(), buf.end(), [](float fp)
			{
				std::cout << fp << ", ";
			});

#endif

		return true;
	}

	bool OnUpdate(std::chrono::nanoseconds elaspedTime) override
	{
		mRenderer.SetCamera(mCamera.GetProjectionMatrix(), mCamera.GetViewMatrix());

		static glm::vec3 vel = glm::vec3(1.0f, 0.0f, 0.0f);
		static glm::vec3 pos = glm::vec3(0.0f);

		pos += vel * (float)elaspedTime.count() / (float)1e9;

		glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

		mRenderer.ModifyRenderable("cube", model);
		mRenderer.InvalidateBuffers();
		mRenderer.WaitIdle();
		mRenderer.UpdateDescriptors();

		mRenderer.Draw();
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

	vkLib::Context mContext;
	std::filesystem::path mPath;
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

	Aqua::SharedRef<Application> app = Aqua::MakeRef<Application>(createInfo);
	app->SetDefaultEventCallbacks();

	app->AddLayer(Aqua::MakeRef<Sandbox>(*app->GetContext(), app->GetAssetDirectory()));

	app->Run();
}
