#pragma once
#include "Executor.h"

#include "../Execution/GraphBuilder.h"

#include "SortRecorder.h"
#include "RayGenerationPipeline.h"
#include "WavefrontWorkflow.h"
#include "../Material/MaterialBuilder.h"

#include "../Geometry3D/GeometryConfig.h"

AQUA_BEGIN
PH_BEGIN

// Thread safe...
class WavefrontEstimator
{
public:
	AQUA_API WavefrontEstimator(const WavefrontEstimatorCreateInfo& createInfo);

	WavefrontEstimator(const WavefrontEstimator&) = delete;
	WavefrontEstimator& operator=(const WavefrontEstimator&) = delete;

	AQUA_API TraceSession CreateTraceSession();
	AQUA_API Executor CreateExecutor(const ExecutorCreateInfo& createInfo);

	AQUA_API std::expected<::AQUA_NAMESPACE::MaterialInstance, vkLib::CompileError>
		CreateMaterialInstance(const RTMaterialCreateInfo& createInfo);

	std::string GetShaderDirectory() { return mCreateInfo.ShaderDirectory; }

private:
	// Resources...
	vkLib::PipelineBuilder mPipelineBuilder;
	vkLib::ResourcePool mResourcePool;

	std::shared_ptr<RaySortRecorder> mSortRecorder;
	
	std::string mShaderFrontEnd;
	std::string mShaderBackEnd;

	std::unordered_map<std::string, std::string> mImportToShaders;

	// Wavefront properties...
	WavefrontEstimatorCreateInfo mCreateInfo;

	MaterialBuilder mMaterialSystem;

private:
	// Helpers...
	ExecutionPipelines CreatePipelines();

	void CreateTraceBuffers(SessionInfo& session);
	void CreateExecutorBuffers(ExecutionInfo& mExecutionInfo, const ExecutorCreateInfo& executorInfo);
	void CreateExecutorImages(ExecutionInfo& executionInfo, const ExecutorCreateInfo& executorInfo);

	void AddText(std::string& text, const std::string& filepath);
	void RetrieveFrontAndBackEndShaders();

	vkLib::PShader GetRayGenerationShader();
	vkLib::PShader GetIntersectionShader();
	vkLib::PShader GetRaySortEpilogueShader(RaySortEvent sortEvent);
	vkLib::PShader GetRayRefCounterShader();
	vkLib::PShader GetPrefixSumShader();
	vkLib::PShader GetLuminanceMeanShader();
	vkLib::PShader GetPostProcessImageShader();
};

PH_END
AQUA_END
