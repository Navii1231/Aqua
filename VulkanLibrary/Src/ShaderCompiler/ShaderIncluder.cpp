#include "Core/vkpch.h"
#include "ShaderCompiler/ShaderIncluder.h"

VK_NAMESPACE::IncludeResult* VK_NAMESPACE::ShaderIncluder::IncludeSystem(
	const std::string& headerName, const std::string& includerName, size_t recursionDepth)
{
	std::filesystem::path Header(headerName);

	for (const auto& systemPath : mSystemPaths)
	{
		auto AbsolutePath = std::filesystem::canonical(systemPath / Header);

		if (std::filesystem::exists(AbsolutePath))
		{
			auto Contents = ReadFile(AbsolutePath.string());

			if (!Contents)
				return  nullptr;

			return new IncludeResult(headerName, *Contents);
		}
	}

	return nullptr;
}

VK_NAMESPACE::IncludeResult* VK_NAMESPACE::ShaderIncluder::IncludeLocal(
	const std::string& headerName, const std::string& includerName, size_t inclusionDepth)
{
	auto SystemResult = IncludeSystem(headerName, includerName, inclusionDepth);

	if (SystemResult)
		return SystemResult;

	std::filesystem::path IncludePath(includerName);
	std::filesystem::path Header(headerName);

	auto Absolute = ResolveLocalPath(Header, IncludePath);

	if (std::filesystem::exists(Absolute))
	{
		auto Contents = ReadFile(Absolute.string());

		if (!Contents)
			return nullptr;

		return new IncludeResult(headerName, *Contents);
	}

	return nullptr;
}

void VK_NAMESPACE::ShaderIncluder::ReleaseInclude(IncludeResult* result)
{
	if (result)
	{
		delete result;
		mRecursionStack.pop_back();
	}
}

std::filesystem::path VK_NAMESPACE::ShaderIncluder::ResolveLocalPath(
	const std::filesystem::path& Header, const std::filesystem::path& IncludePath)
{
	std::filesystem::path RelativePath;

	mRecursionStack.push_back(IncludePath);

	for (const auto& RecursivePath : mRecursionStack)
		RelativePath = RelativePath / RecursivePath;

	return mShaderDirectory / RelativePath / Header;
}
