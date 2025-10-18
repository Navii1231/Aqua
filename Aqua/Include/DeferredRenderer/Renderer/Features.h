#pragma once
#include "RendererConfig.h"

AQUA_BEGIN

struct FeatureInfo
{
	std::string Name;
	RenderingStage Stage = RenderingStage::eFrontEnd;
	RenderingFeature Type = RenderingFeature::eShadow;
	vkLib::GenericBuffer UniBuffer;
};

struct SSAOFeature
{
	alignas(4) uint32_t SampleCount = 8;
	alignas(4) float Radius = 0.1f;
	// we'll deal with it when implementing the SSAO
	alignas(4) std::uniform_real_distribution<float> Distribution;
};

struct ShadowCascadeFeature
{
	alignas(8) glm::uvec2 BaseResolution = { 2048, 2048 };
	alignas(4) float CascadeDepth = 500.0f;
	alignas(4) uint32_t CascadeDivisions = 8;
};

struct BloomEffectFeature
{
	alignas(16) glm::vec3 Color = { 10.0f, 10.0f, 10.0f };
	alignas(4) float Radius = 0.1f;
	alignas(4) float Threshold = 100.0f;
};

using FeatureInfoMap = std::unordered_map<RenderingFeature, FeatureInfo>;

AQUA_END
