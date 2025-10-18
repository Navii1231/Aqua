#pragma once
#include "ExecutorConfig.h"

#include "../Execution/GraphBuilder.h"

AQUA_BEGIN
PH_BEGIN

// Incorporating the execution model next
class Executor
{
public:
	Executor() = default;

	// Copy the command buffers
	Executor(const Executor& Other) = delete;
	Executor& operator=(const Executor& Other) = delete;

	Executor(Executor&&) = default;
	Executor& operator =(Executor&&) = default;

	AQUA_API ~Executor();

	AQUA_API TraceResult Trace();

	// support for debugging
	AQUA_API TraceResult Step();

	AQUA_API void SetDebugMode(bool enable);
	AQUA_API void InvalidateMaterialResources();
	AQUA_API void Reset();
	AQUA_API void ConstructExecutionGraphs(uint32_t depth);
	AQUA_API void SetTraceSession(const TraceSession& traceSession);

	AQUA_API void SetSkybox(vkLib::ImageView view, vkLib::Core::Ref<vk::Sampler> sampler);

	template<typename Iter>
	void SetMaterialPipelines(Iter Begin, Iter End);

	void SetSortingFlag(bool allowSort)
	{ mExecutorInfo->CreateInfo.AllowSorting = allowSort; }

	AQUA_API void SetCameraView(const glm::mat4& cameraView);

	// Getters...
	uint32_t GetBounceIdx() const { return mExecutionBlock.mBounceIdx - 1; }
	TraceSession GetTraceSession() const { return mExecutorInfo->TracingSession; }

	glm::ivec2 GetTargetResolution() const { return mExecutorInfo->Target.ImageResolution; }
	vkLib::Image GetPresentable() const { return mExecutorInfo->Target.Presentable; }
	vkLib::Buffer<WavefrontSceneInfo> GetSceneInfo() const { return mExecutorInfo->Scene; }

	// For debugging...
	RayBuffer GetRayBuffer() const { return mExecutorInfo->Rays; }
	CollisionInfoBuffer GetCollisionBuffer() const { return mExecutorInfo->CollisionInfos; }
	RayRefBuffer GetRayRefBuffer() const { return mExecutorInfo->RayRefs; }
	RayInfoBuffer GetRayInfoBuffer() const { return mExecutorInfo->RayInfos; }
	vkLib::Buffer<uint32_t> GetMaterialRefCounts() const { return mExecutorInfo->RefCounts; }
	vkLib::Image GetVariance() const { return mExecutorInfo->Target.PixelVariance; }
	vkLib::Image GetMean() const { return mExecutorInfo->Target.PixelMean; }

private:
	std::shared_ptr<ExecutionInfo> mExecutorInfo;

	PostProcessFlags mPostProcess;

	EXEC_NAMESPACE::Graph mRayGenGraph;
	EXEC_NAMESPACE::Graph mPostProcessGraph;

	std::vector<EXEC_NAMESPACE::Graph> mTraceGraphs;

	EXEC_NAMESPACE::GraphList mRayGenExecList;
	EXEC_NAMESPACE::GraphList mPostProcessExecList;
	std::vector<EXEC_NAMESPACE::GraphList> mTraceExecList;

	// skybox
	bool mSkyboxExists = false;
	vkLib::ImageView mSkyboxView;
	vkLib::Core::Ref<vk::Sampler> mSkyboxSampler;

	// this is read only data for all execution graphs!!
	ExecutionBlock mExecutionBlock;
	uint32_t mMaxBounce = 0;

	bool mDebugMode = false;

	TraceSessionState mTraceState = TraceSessionState::eReady;

	std::vector<vk::CommandBuffer> mCmdBufs;

	vkLib::Context mCtx;

private:
	Executor(const ExecutorCreateInfo& createInfo);

	void ConnectRayGenToTrace(const std::vector<std::string>& traceInput);
	void ConnectTraces(uint32_t leading, uint32_t dependent, const std::vector<std::string>& traceInputs);
	void ConnectTraceToPostProcess(const std::vector<std::string>& postInputs);

	void JoinGraphs();
	void SeparateGraphs();

	void ConstructTraceExec(EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& outputs);
	void ConstructRayGenExec(EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& outputs);
	void ConstructPostProcessExec(EXEC_NAMESPACE::GraphBuilder& builder, std::vector<std::string>& inputs);

	uint32_t GetRandomNumber();

	TraceResult StepImpl();

	void RecordRayGenerator(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);

	// todo: unused for now
	void RecordRaySortFinisher(vk::CommandBuffer commandBuffer);
	void RecordPrefixSummer(vk::CommandBuffer commandBuffer);
	void RecordRayCounter(vk::CommandBuffer commandBuffer);
	void RecordRaySortPreparer(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);

	void RecordIntersectionTester(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);
	void RecordLuminanceMean(vk::CommandBuffer commandBuffer, uint32_t pActiveBuffer);
	void RecordPostProcess(vk::CommandBuffer commandBuffer);

	void ExecuteGraphList(const EXEC_NAMESPACE::GraphList& execList);

	void UpdateSceneInfo();

	void InvalidateMaterialData();

private:
	void AssignMaterialsResources(MaterialInstance& instance, const SessionInfo& TracingSession);
	void RecordMaterialPipeline(vk::CommandBuffer cmd, uint32_t pMaterialRef, uint32_t pBounceIdx, uint32_t pActiveBuffer);

	void UpdateMaterialDescriptors();

	friend class WavefrontEstimator;
};

template<typename Iter>
inline void Executor::SetMaterialPipelines(Iter Begin, Iter End)
{
	mExecutorInfo->MaterialResources.clear();
	
	for (; Begin != End; Begin++)
	{
		mExecutorInfo->MaterialResources.emplace_back(*Begin);

		// For optimizations...
		if (mExecutorInfo->MaterialResources.size() == mExecutorInfo->MaterialResources.capacity())
			mExecutorInfo->MaterialResources.reserve(2 * mExecutorInfo->MaterialResources.size());
	}

	mExecutorInfo->RefCounts.Resize(glm::max(static_cast<uint32_t>(mExecutorInfo->MaterialResources.size()
		+ 2), static_cast<uint32_t>(32)));

	InvalidateMaterialData();
}

PH_END
AQUA_END
