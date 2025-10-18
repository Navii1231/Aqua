#include "Core/Aqpch.h"
#include "Core/Config.h"
#include "DeferredRenderer/Renderable/CopyIndices.h"

AQUA_BEGIN

std::string CopyIdxPipeline::sDefaultImplementation =
R"(
#version 440

layout(local_size_x = 32) in;

layout(push_constant) uniform ShaderConstants
{
	uint pVertexOffset;
	uint pIndexOffset;
	uint pSize;
};

layout(std430, set = 0, binding = 0) readonly buffer SrcIndices
{
	uint sSrcIndices[];
};

layout(std430, set = 0, binding = 1) buffer DstIndices
{
	uint sDstIndices[];
};

uint indices[2] = uint[](

	0, 1

	);

void main()
{
	uint Position = gl_GlobalInvocationID.x;

	if (Position >= pSize)
		return;

	sDstIndices[Position + pIndexOffset] = sSrcIndices[Position] + pVertexOffset;
	//sDstIndices[Position + pIndexOffset] = indices[Position] + pVertexOffset;
	//sDstIndices[0] = sSrcIndices[0];
	//sDstIndices[1] = sSrcIndices[1];
}
)";

AQUA_END

AQUA_NAMESPACE::CopyIdxPipeline::CopyIdxPipeline()
{
	vkLib::PShader shader{};
	shader.SetShader("eCompute", sDefaultImplementation);

	shader.CompileShaders();

	this->SetShader(shader);
}

void AQUA_NAMESPACE::CopyIdxPipeline::UpdateDescriptors(vkLib::GenericBuffer dst, vkLib::GenericBuffer src)
{
	vkLib::StorageBufferWriteInfo idx{};
	idx.Buffer = src.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 0, 0 }, idx);

	idx.Buffer = dst.GetNativeHandles().Handle;

	this->UpdateDescriptor({ 0, 1, 0 }, idx);
}

void AQUA_NAMESPACE::CopyIdxPipeline::operator()(vk::CommandBuffer cmd, vkLib::GenericBuffer dst,
	vkLib::GenericBuffer src, uint32_t vertexCount)
{
	size_t idxCount = src.GetSize() / sizeof(uint32_t);
	size_t idxOffset = dst.GetSize() / sizeof(uint32_t);

	glm::uvec3 workGrps = glm::uvec3(idxCount / GetWorkGroupSize().x + 1, 1, 1);

	dst.Resize(dst.GetSize() + src.GetSize());

	UpdateDescriptors(dst, src);

	Begin(cmd);

	Activate();

	Aqua::PushConst(*this, "eCompute.ShaderConstants.Index_0", static_cast<uint32_t>(vertexCount));
	Aqua::PushConst(*this, "eCompute.ShaderConstants.Index_1", static_cast<uint32_t>(idxOffset));
	Aqua::PushConst(*this, "eCompute.ShaderConstants.Index_2", static_cast<uint32_t>(idxCount));

	Dispatch(workGrps);

	End();
}
