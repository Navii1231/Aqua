#pragma once
#include "../Renderable/BasicRenderables.h"

AQUA_BEGIN

class BezierCurve
{
public:
	BezierCurve(const Curve& curve)
		: mCurve(curve) {
	}

	~BezierCurve() = default;

	inline std::vector<glm::vec4> Solve(float stepSize = 0.01f);
	inline glm::vec4 GetPoint(float par) const { return EvaluateCurve(0, static_cast<uint32_t>(mCurve.Points.size() - 1), par); }

private:
	Curve mCurve;

private:
	inline glm::vec4 EvaluateCurve(uint32_t i, uint32_t j, float t) const;
};

std::vector<glm::vec4> BezierCurve::Solve(float stepSize /*= 0.01f*/)
{
	std::vector<glm::vec4> points;
	points.reserve(static_cast<size_t>(1.0f / stepSize));

	size_t idx = 0;

	for (float t = 0.0f; t < 1.0f; t += stepSize, idx++)
	{
		points.emplace_back(GetPoint(t));
	}

	return points;
}

glm::vec4 BezierCurve::EvaluateCurve(uint32_t i, uint32_t j, float t) const
{
	if (j == 0)
		return mCurve.Points[i];

	return EvaluateCurve(i, j - 1, t) * (1.0f - t) + EvaluateCurve(i + 1, j - 1, t) * t;
}

AQUA_END
