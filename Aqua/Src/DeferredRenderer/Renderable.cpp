#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/Renderable.h"

void AQUA_NAMESPACE::Renderable::UpdateMetaData(const std::string& bindingName, uint32_t beginIdx /*= 0*/, uint32_t endIdx /*= std::numeric_limits<uint32_t>::max()*/)
{
	endIdx = glm::clamp(endIdx, beginIdx, (uint32_t)(Info.VertexCount / sizeof(uint32_t)));
	UpdateMetaData(mVertexBuffers[bindingName], VertexData, beginIdx, endIdx);
}

void AQUA_NAMESPACE::Renderable::UpdateMetaData(vkLib::GenericBuffer buffer, const VertexMetaData& data, uint32_t beginIdx, uint32_t endIdx)
{
	uint32_t count = endIdx - beginIdx;

	buffer.Resize(count * data.Stride);

	if (count == 0)
		return;

	auto memory = buffer.MapMemory<uint8_t>(count * data.Stride, beginIdx * data.Stride);
	auto endMemory = memory + count * data.Stride;

	for (; memory < endMemory; memory += data.Stride)
	{
		glm::vec3& metaData = *(glm::vec3*)(memory + data.Offset);

		metaData.x = static_cast<float>(data.ModelIdx);
		metaData.y = static_cast<float>(data.MaterialIdx);
		metaData.z = static_cast<float>(data.ParameterOffset);
	}

	buffer.UnmapMemory();
}

