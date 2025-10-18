#pragma once
#include "RendererConfig.h"
#include "MaterialConfig.h"

#include "../../Material/MaterialInstance.h"
#include "../Renderable/RenderTargetFactory.h"

AQUA_BEGIN

struct MaterialSystemConfig;

// We need material caching...
// we need to reserve primitive materials for objects line, points and bezier curves
class MaterialSystem
{
public:
	AQUA_API MaterialSystem();
	AQUA_API MaterialSystem(vkLib::Context ctx, const std::filesystem::path& shaderDir);

	~MaterialSystem() = default;

	AQUA_API void SetShaderDirectory(const std::filesystem::path& shaders);

	AQUA_API void SetHyperSurfRenderProperties(vkLib::RenderTargetContext rCtx);
	AQUA_API void SetPBRRenderProperties(vkLib::RenderTargetContext rCtx);
	AQUA_API void SetDiffuseRenderProperties(vkLib::RenderTargetContext rCtx);
	AQUA_API void SetGlossyRenderProperties(vkLib::RenderTargetContext rCtx);
	AQUA_API void SetWireframeRenderProperties(vkLib::RenderTargetContext rCtx);

	AQUA_API void SetCtx(vkLib::Context ctx);

	// builds and caches a material one
	AQUA_API MaterialInstance BuildInstance(const std::string& name, const DeferGFXMaterialCreateInfo& createInfo) const;
	// builds and caches a material one
	AQUA_API MaterialInstance BuildInstance(const std::string& name, const DeferCmpMaterialCreateInfo& createInfo) const;

	// finds the material in the cache
	AQUA_API std::expected<MaterialInstance, bool> operator[](const std::string& name) const;

private:
	SharedRef<MaterialSystemConfig> mConfig;

private:
	void InitInstance(MaterialTemplate& temp, MaterialInstance& instance, MAT_NAMESPACE::Platform rendererPlatform) const;

	SharedRef<MaterialTemplate> FindTemplate(const std::string& name) const;

	SharedRef<MaterialTemplate> CreateTemplate(const std::string& name, 
		const DeferGFXMaterialCreateInfo& createInfo, const MaterialInstance& clone) const;

	SharedRef<MaterialTemplate> CreateTemplate(const std::string& name, 
		const DeferCmpMaterialCreateInfo& createInfo, const MaterialInstance& clone) const;

	void BuildHyperSurfPipelines();
	void BuildWireframePipeline();
	void BuildPBRPipeline();
	void BuildDiffusePipeline();
	void BuildGlossyPipeline();

	void CompileHyperSurfShader();
	void CompileWireframeShader();
	void InsertImports();
	void SetupDeferShaderStrings();
};

AQUA_END
