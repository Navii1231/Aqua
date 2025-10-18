#pragma once
#include "MaterialConfig.h"
#include "../Utils/Lexer.h"

AQUA_BEGIN

// TODO: Comments are ignored
class MaterialPostprocessor
{
public:
	MaterialPostprocessor() = default;
	~MaterialPostprocessor() = default;

	void SetShaderTextView(std::string_view text) { mShaderTextView = text; }
	void SetCustomPrefix(const std::string& val) { mCustomPrefix = val; }

	void SetShaderModules(const MAT_NAMESPACE::ShaderImportMap& imports) { mImportsModules = imports; }
	void SetBasicTypeNames(const std::vector<std::string>& basicTypes) { mBasicShaderTypes = basicTypes; }

	AQUA_API std::expected<std::string, MAT_NAMESPACE::MaterialGraphPostprocessError> ResolveCustomParameters(MAT_NAMESPACE::ShaderParameterSet& parameters) const;
	AQUA_API std::expected<std::string, MAT_NAMESPACE::MaterialGraphPostprocessError> ResolveImportDirectives() const;

private:
	std::string_view mShaderTextView;
	std::string mCustomPrefix = "u_custom_par_";

	MAT_NAMESPACE::ShaderImportMap mImportsModules;
	std::vector<std::string> mBasicShaderTypes;

private:
	MAT_NAMESPACE::MaterialGraphPostprocessError ConstructError(MAT_NAMESPACE::MaterialPostprocessState state,
		const Token& token, const std::string& errorString) const;

	bool ImportSanityCheck(MAT_NAMESPACE::MaterialGraphPostprocessError& error, const Token& prevToken,
		const Token& importKeyword,
		const Token& shaderImport,
		const Token& endLine) const;

	bool IsBasicShaderType(std::string_view type) const;
	bool ValidVariableName(std::string_view varName) const;

	uint32_t GetBasicTypeIdx(const Token& token) const;
};

AQUA_END
