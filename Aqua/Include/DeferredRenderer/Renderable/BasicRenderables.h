#pragma once
#include "FactoryConfig.h"

AQUA_BEGIN

#define ENTRY_POINT_VERTEX      "PointVertices"
#define ENTRY_LINE_VERTEX       "LineVertices"

struct Point
{
	glm::vec4 Position{};
	glm::vec4 Color{ 0.0f, 1.0f, 0.0f, 1.0f };
};

struct Line
{
	Point Begin;
	Point End;
};

using Plane = std::array<Point, 4>;

struct Curve
{
	std::vector<glm::vec4> Points;
	glm::vec4 Color{ 0.0f, 1.0f, 0.0f, 1.0f };
};

struct ModelInfo
{
	MeshData CPUModelData;
};

struct HyperSurfInfo
{
	std::vector<Line> Lines;
	std::vector<Curve> Curves;

	std::vector<Point> Points;

	bool RepresentingLines = true;
};

AQUA_END
