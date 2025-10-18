#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/RenderableBuilder.h"
#include "DeferredRenderer/Renderable/BasicRenderables.h"
#include "DeferredRenderer/Pipelines/PipelineConfig.h"

AQUA_BEGIN

RenderableBuilder<ModelInfo>::CopyFnMap sVertexCopyFn =
{
	{ ENTRY_POSITION, [](vkLib::GenericBuffer& buffer, const ModelInfo& renderableInfo, RenderableInfo& info)
	{
		info.VertexCount = renderableInfo.CPUModelData.GetVertexCount();
		info.IndexCount = renderableInfo.CPUModelData.GetIndexCount();
		info.PrimitiveType = renderableInfo.CPUModelData.mPrimitive;
		info.FaceCount = renderableInfo.CPUModelData.GetFaceIndexCount();

		buffer << renderableInfo.CPUModelData.aPositions;
	} },

	{ ENTRY_NORMAL, [](vkLib::GenericBuffer& buffer, const ModelInfo& renderableInfo, RenderableInfo& info)
	{
		buffer << renderableInfo.CPUModelData.aNormals;
	} },

	{ ENTRY_TANGENT_SPACE, [](vkLib::GenericBuffer& buffer, const ModelInfo& renderableInfo, RenderableInfo& info)
	{
		buffer.Resize(renderableInfo.CPUModelData.aPositions.size() * 2 * sizeof(glm::vec3));

		auto* mem = buffer.MapMemory<glm::vec3>(2 * renderableInfo.CPUModelData.aPositions.size());

		for (size_t i = 0; i < renderableInfo.CPUModelData.aPositions.size(); i++)
		{
			mem[0] = renderableInfo.CPUModelData.aTangents[i];
			mem[1] = renderableInfo.CPUModelData.aBitangents[i];

			mem += 2;
		}

		buffer.UnmapMemory();
	} },

	{ ENTRY_TEXCOORDS, [](vkLib::GenericBuffer& buffer, const ModelInfo& renderableInfo, RenderableInfo& info)
	{
		buffer << renderableInfo.CPUModelData.aTexCoords;
	} },

	// This is not really necessary...
	{ ENTRY_METADATA, [](vkLib::GenericBuffer& buffer, const ModelInfo& renderableInfo, RenderableInfo& info)
	{
		buffer.Resize(renderableInfo.CPUModelData.aPositions.size() * sizeof(glm::vec3));

		auto* mem = buffer.MapMemory<glm::vec3>(renderableInfo.CPUModelData.aPositions.size());

		for (size_t i = 0; i < renderableInfo.CPUModelData.aPositions.size(); i++)
		{
			*mem = { 0.0f, 0.0f, 0.0f };
			mem++;
		}

		buffer.UnmapMemory();
	} },
};

RenderableBuilder<ModelInfo>::CopyFn sIndexCopyFn = 
[](vkLib::GenericBuffer& idxBuf, const ModelInfo& info, RenderableInfo& renderableInfo)
	{
		idxBuf.Resize(3 * info.CPUModelData.aFaces.size() * sizeof(uint32_t));

		auto* mem = idxBuf.MapMemory<uint32_t>(3 * info.CPUModelData.aFaces.size());

		for (size_t i = 0; i < info.CPUModelData.aFaces.size(); i++)
		{
			mem[0] = info.CPUModelData.aFaces[i].Indices[0];
			mem[1] = info.CPUModelData.aFaces[i].Indices[1];
			mem[2] = info.CPUModelData.aFaces[i].Indices[2];

			mem += 3;
		}

		idxBuf.UnmapMemory();
	};

RenderableBuilder<HyperSurfInfo>::CopyFnMap sHyperSurfVertCopyFn =
{
	{ ENTRY_LINE_VERTEX,
	[](vkLib::GenericBuffer& buffer, const HyperSurfInfo& info, RenderableInfo& renderableInfo)
	{
		if (info.RepresentingLines)
		{
			// index mapping will be different for curves and lines
			size_t totalPointsInTheCurve = 0;

			for (const auto& curve : info.Curves)
				totalPointsInTheCurve += curve.Points.size();

			renderableInfo.VertexCount = 2 * info.Lines.size() + totalPointsInTheCurve;
			renderableInfo.PrimitiveType = FacePrimitive::eLine;

			buffer.Resize(renderableInfo.VertexCount * sizeof(Point));

			if (renderableInfo.VertexCount == 0)
				return;

			auto* mem = buffer.MapMemory<Point>(renderableInfo.VertexCount);

			for (size_t i = 0; i < info.Lines.size(); i++)
			{
				mem[0] = info.Lines[i].Begin;
				mem[1] = info.Lines[i].End;
				mem += 2;
			}

			for (const auto& curve : info.Curves)
			{
				for (size_t i = 0; i < curve.Points.size(); i++)
				{
					*mem = { curve.Points[i], curve.Color };
					mem++;
				}
			}

			buffer.UnmapMemory();
		}
	} },

	{ ENTRY_POINT_VERTEX, 
	[](vkLib::GenericBuffer& buffer, const HyperSurfInfo& info, RenderableInfo& renderableInfo)
	{
		if (!info.RepresentingLines)
		{
			renderableInfo.VertexCount = info.Points.size();
			renderableInfo.PrimitiveType = FacePrimitive::ePoint;

			buffer.Resize(renderableInfo.VertexCount * sizeof(Point));

			if (renderableInfo.VertexCount == 0)
				return;

			auto* mem = buffer.MapMemory<Point>(renderableInfo.VertexCount);

			for (size_t i = 0; i < renderableInfo.VertexCount; i++)
			{
				*mem = info.Points[i];
				mem++;
			}

			buffer.UnmapMemory();
		}
	} }
};

RenderableBuilder<HyperSurfInfo>::CopyFn sHyperSurfIdxCopyFn =
[](vkLib::GenericBuffer& buffer, const HyperSurfInfo& info, RenderableInfo& renderableInfo)
	{
		if (info.RepresentingLines)
		{
			// index mapping will be different for curves and lines
			size_t totalPointsInTheCurve = 0;

			for (const auto& curve : info.Curves)
				totalPointsInTheCurve += curve.Points.size();

			size_t curveIdxCount = totalPointsInTheCurve < 2 ? 0 : 2 * (totalPointsInTheCurve - 1);

			renderableInfo.IndexCount = 2 * info.Lines.size() + curveIdxCount;
			renderableInfo.FaceCount = renderableInfo.IndexCount;

			buffer.Resize(renderableInfo.IndexCount * sizeof(uint32_t));

			if (renderableInfo.IndexCount == 0)
				return;

			auto* mem = buffer.MapMemory<uint32_t>(renderableInfo.IndexCount);

			for (uint32_t i = 0; i < static_cast<uint32_t>(info.Lines.size()); i++)
			{
				mem[0] = 2 * i;
				mem[1] = 2 * i + 1;

				mem += 2;
			}

			uint32_t count = 0;

			for (const auto& curve : info.Curves)
			{
				for (uint32_t i = 1; i < static_cast<uint32_t>(curve.Points.size()); i++)
				{
					mem[0] = i - 1 + count;
					mem[1] = i + count;
					mem += 2;
				}

				count += static_cast<uint32_t>(curve.Points.size());
			}

			buffer.UnmapMemory();
		}
		else
		{
			// no index; drawing by vertices instead
			renderableInfo.IndexCount = 0;
			renderableInfo.FaceCount = 0;
		}
	};

AQUA_END
