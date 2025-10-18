#include "Core/vkpch.h"
#include "ShaderCompiler/CompilerEnvironment.h"

void VK_NAMESPACE::CompilerEnvironment::AddPath(const std::filesystem::path& path)
{
	mSystemPaths.insert(path);
}

void VK_NAMESPACE::CompilerEnvironment::RemovePath(const std::filesystem::path& path)
{
	mSystemPaths.erase(path);
}

std::shared_ptr<VK_NAMESPACE::ShaderIncluder> VK_NAMESPACE::CompilerEnvironment::
	CreateShaderIncluder(const std::string& shaderPath) const
{
	std::filesystem::path ShaderDirectory(shaderPath);
	ShaderDirectory = std::filesystem::canonical(ShaderDirectory.parent_path());

	std::vector<std::filesystem::path> paths(mSystemPaths.begin(), mSystemPaths.end());
	return std::make_shared<ShaderIncluder>(ShaderDirectory, paths);
}

