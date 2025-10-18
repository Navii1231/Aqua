#pragma once
#include "RayTracingStructures.h"
#include "../Utils/CompilerErrorChecker.h"

AQUA_BEGIN
PH_BEGIN

class LocalRadixSortPipeline : public vkLib::ComputePipeline
{
public:
	LocalRadixSortPipeline() = default;

	LocalRadixSortPipeline(uint32_t workGroupSize) { CompileShader(workGroupSize); }

	void UploadBuffer(const vkLib::Buffer<uint32_t>& buffer)
	{
		mBuffer = buffer;
	}

	virtual void UpdateDescriptors()
	{
		vkLib::DescriptorWriter& writer = this->GetDescriptorWriter();

		vkLib::StorageBufferWriteInfo prefixInfo{};
		prefixInfo.Buffer = mBuffer.GetNativeHandles().Handle;

		writer.Update({ 0, 0, 0 }, prefixInfo);
	}

	vkLib::Buffer<uint32_t> GetPrefixBuffers() const { return mBuffer; }

private:
	vkLib::Buffer<uint32_t> mBuffer;

	uint32_t mWorkgroupSize = 256;

private:
	// Helper method...

	void CompileShader(uint32_t workGroupSize)
	{
		mWorkgroupSize = workGroupSize;

		vkLib::PShader shader;

		/*** TODO: Shader source must be hard - coded in the class field! ***/

		std::string ShaderPath = "D:/Dev/VulkanEngine/vkEngineTester/Shaders/Utils/LocalRadixSort.glsl";

		std::string ShaderBackEnd = *vkLib::ReadFile(ShaderPath);

		/********************************************************************/

		// Combine the front-end implementation to the backend shader
		//std::string FullShader = "#version 440\n\n";
		//FullShader += mShaderFrontEnd + ShaderBackEnd;

		uint32_t Stride = 2;

		// TODO: Assert that the mWorkGroupSize must a multiple of two!

		shader.AddMacro("WORKGROUP_SIZE", std::to_string(mWorkgroupSize));
		shader.AddMacro("TREE_DEPTH", std::to_string((uint32_t) (glm::log2((float) mWorkgroupSize) + 0.5f)));
		shader.AddMacro("STRIDE", std::to_string(Stride));

		shader.SetShader("eCompute", ShaderBackEnd);

		// Compile the shader
		auto Errors = shader.CompileShaders();

		CompileErrorChecker checker("../vkEngineTester/Logging/ShaderFails/Shader.glsl");
		auto ErrorInfos = checker.GetErrors(Errors);

		this->SetShader(shader);
	}
};

PH_END
AQUA_END
