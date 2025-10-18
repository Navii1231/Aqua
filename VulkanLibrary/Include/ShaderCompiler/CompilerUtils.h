#pragma once
#include "../Core/Config.h"

VK_BEGIN

using FileStream = std::string;

enum FileError
{
	eFileNotFound       = 1,
};

inline std::expected<FileStream, FileError> ReadFile(const std::string& filepath, std::ios::openmode mode = std::ios::in)
{
	std::ifstream file(filepath, std::ios::in | mode);

	if (!file)
		return std::unexpected(FileError::eFileNotFound);

	std::stringstream stream;

	stream << file.rdbuf();
	file.close();

	return std::move(stream.str());
}

inline std::expected<bool, FileError> WriteFile(const std::string& filepath, const FileStream& stream)
{
	std::ofstream file(filepath, std::ios::out);

	if (!file)
		return std::unexpected(FileError::eFileNotFound);

	file.write((const char*)stream.data(), stream.size());
	file.close();

	return true;
}

VK_END
