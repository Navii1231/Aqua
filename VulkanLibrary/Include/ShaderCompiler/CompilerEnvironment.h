#pragma once
#include "ShaderIncluder.h"
#include "ShaderConfig.h"

VK_BEGIN

class CompilerEnvironment
{
public:
	explicit CompilerEnvironment(const CompilerConfig& config)
		: mConfig(config) {}

	VKLIB_API void AddPath(const std::filesystem::path& path);
	VKLIB_API void RemovePath(const std::filesystem::path& path);

	void AddMacro(const std::string& macroName, const std::string& macroDefine)
	{ mMacrosDefines[macroName] = macroDefine; }

	void RemoveMacro(const std::string& macroName)
	{ mMacrosDefines.erase(macroName); }

	void SetPreprocessorDirectives(const PreprocessorDirectives& directives)
	{ mMacrosDefines = directives; }

	VKLIB_API std::shared_ptr<ShaderIncluder> CreateShaderIncluder(const std::string& shaderPath) const;

	CompilerConfig GetConfig() const { return mConfig; }
	void SetConfig(const CompilerConfig& config) { mConfig = config; }

	const std::unordered_map<std::string, std::string> GetMacroDefines() const
	{ return mMacrosDefines; }

private:
	std::set<std::filesystem::path> mSystemPaths;
	CompilerConfig mConfig;

	PreprocessorDirectives mMacrosDefines;
};

VK_END
