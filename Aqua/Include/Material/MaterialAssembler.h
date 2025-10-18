#pragma once
#include "MaterialInstance.h"

AQUA_BEGIN
MAT_BEGIN

// Adding some kind of an import system later
class MaterialAssembler
{
public:
	MaterialAssembler() = default;
	~MaterialAssembler() = default;

	void SetPipelineBuilder(vkLib::PipelineBuilder builder) { mPipelineBuilder = builder; }

	AQUA_API std::expected<MAT_NAMESPACE::Material, vkLib::CompileError> ConstructRayTracingMaterial(const std::string& shader, const vkLib::PreprocessorDirectives& directives = {});
	AQUA_API std::expected<MAT_NAMESPACE::Material, vkLib::CompileError> ConstructDeferGFXMaterial(const std::string& shader, vkLib::GraphicsPipelineConfig config, const vkLib::PreprocessorDirectives& directives = {});
	AQUA_API std::expected<MAT_NAMESPACE::Material, vkLib::CompileError> ConstructDeferCmpMaterial(const std::string& shader, const vkLib::PreprocessorDirectives& directives = {});

	static std::string GetDeferGFXVertexShader() { return sDeferGFXVertexShader; }

private:
	vkLib::PipelineBuilder mPipelineBuilder;

	static std::string sDeferGFXVertexShader;
};

MAT_END
AQUA_END
