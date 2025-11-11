#include "Core/Aqpch.h"
#include "Material/MaterialPostprocessor.h"

std::expected<std::string, AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError>
	AQUA_NAMESPACE::MaterialPostprocessor::ResolveCustomParameters(MAT_NAMESPACE::ShaderParameterSet& parameters) const
{
	if (mShaderTextView.empty())
		return std::unexpected(ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eEmptyString, {}, { 0, 0, 0 }, "Empty string"));

	Lexer lexer;
	lexer.SetString(mShaderTextView);
	lexer.SetWhiteSpacesAndDelimiters(" \t\r\n", "@.(){}[]=+-*/%|^&!~?:;,");

	size_t prevIdx = lexer.GetPosition();
	lexer++;

	std::string output;
	output.reserve(mShaderTextView.size());

	while(true)
	{
		if (lexer.HasConsumed())
		{
			output += mShaderTextView.substr(prevIdx, lexer.GetCursors().PosOff - prevIdx);
			break;
		}

		size_t currPos = lexer.GetCursors().PosOff;

		// Try to look for [basic_type].[name] pattern
		if (GetBasicTypeIdx(*lexer) != -1)
		{
			Token typeName = lexer++;

			if (*lexer == ".") // our variable
			{
				lexer++;
				Token varName = lexer++;

				if (!ValidVariableName(varName))
					return std::unexpected(ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidCustomParameter,
						varName, lexer.GetCursors(), "Invalid custom parameter name: " + std::string(varName)));

				if (parameters.find(std::string(varName)) == parameters.end())
				{
					auto& par = parameters[std::string(varName)];
					par.Type = std::string(typeName);
					par.Name = std::string(varName);
					par.TypeSize = 0; // will be set by the material builder
					par.Offset = 0; // will be configured by the material builder
				}

				output += mShaderTextView.substr(prevIdx, currPos - prevIdx - typeName.size());
				output += mCustomPrefix + parameters[std::string(varName)].Name;

				prevIdx = lexer.GetPosition();

				continue;
			}
		}

		// Otherwise just emit the token as is
		output += mShaderTextView.substr(prevIdx, lexer.GetCursors().PosOff - prevIdx);

		prevIdx = lexer.GetCursors().PosOff;
		lexer++;
	}

	return output;
}

std::expected<std::string, AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError>
	AQUA_NAMESPACE::MaterialPostprocessor::ResolveImportDirectives() const
{
	// An import system to load pre-existing shaders...
	// Using a lexer for that as we will use custom key word for inclusion
	// The custom keyword and grammar for inclusion is:
	// import [ShaderName]

	std::string output;
	output.reserve(mShaderTextView.size());

	MAT_NAMESPACE::MaterialGraphPostprocessError error;
	error.State = MAT_NAMESPACE::MaterialPostprocessState::eSuccess;

	Lexer lexer;
	lexer.SetString(mShaderTextView);
	lexer.SetWhiteSpacesAndDelimiters(" \t\r", "\n");

	lexer++;

	size_t prevIdx = 0;

	while (true)
	{
		if (lexer.HasConsumed())
		{
			output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);
			break;
		}

		Token curr = *lexer;
		auto cursor = lexer.GetCursors();

		if (*lexer == "import")
		{
			lexer++;
			Token shaderNameToken = lexer++;
			Token endLineToken = lexer++;

			if (!ImportSanityCheck(error, lexer, prevIdx, curr, shaderNameToken, endLineToken, cursor))
				return std::unexpected(error);

			size_t Size = curr.size() + shaderNameToken.size() + endLineToken.size();

			std::string lexemeString = std::string(shaderNameToken);

			auto iter = mImportsModules.find(lexemeString);

			if (iter == mImportsModules.end())
			{
				ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eShaderNotFound, curr, cursor,
					"Could not import the shader: " + lexemeString);

				return std::unexpected(error);
			}

			// adding the module text here
			output += iter->second;
			output += "\n";

			prevIdx = lexer.GetPosition();
			lexer++;

			continue;
		}

		// Otherwise just emit the token as is
		output += mShaderTextView.substr(prevIdx, lexer.GetPosition() - prevIdx);

		prevIdx = lexer.GetPosition();
		lexer++;
	}

	return output;
}

AQUA_NAMESPACE::MAT_NAMESPACE::MaterialGraphPostprocessError AQUA_NAMESPACE::MaterialPostprocessor::ConstructError(MAT_NAMESPACE::MaterialPostprocessState state, const Token& token, const Cursors& cursors, const std::string& errorString) const
{
	MAT_NAMESPACE::MaterialGraphPostprocessError error;
	error.State = state;
	error.Info = "line (" + std::to_string(cursors.LineOff) + ", " +
		std::to_string(cursors.CharOff) + ") -- " + errorString;

	return error;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::ImportSanityCheck(MAT_NAMESPACE::MaterialGraphPostprocessError& error, Lexer& lexer, const size_t prevIdx, const Token& importKeyword, const Token& shaderImport, const Token& endLine, const Cursors& cursors) const
{
	auto str = lexer.GetString();

	if (shaderImport.empty())
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, importKeyword, cursors, "No shader has been provided");

		return false;
	}

	if (prevIdx == 0)
	{
		// do nothing, this is a valid case
	}
	else if (str[prevIdx] != '\n')
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, importKeyword, cursors, "The import statement must begin with a new line");

		return false;
	}

	if (endLine != "\n" && endLine[0] != '\0')
	{
		error = ConstructError(MAT_NAMESPACE::MaterialPostprocessState::eInvalidImportDirective, endLine, cursors, "Unexpected token \'" + std::string(endLine) + "\'");

		return false;
	}

	return true;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::IsBasicShaderType(std::string_view type) const
{
	auto found = std::find(mBasicShaderTypes.begin(), mBasicShaderTypes.end(), std::string(type));

	if (found == mBasicShaderTypes.end())
		return false;

	return true;
}

bool AQUA_NAMESPACE::MaterialPostprocessor::ValidVariableName(std::string_view varName) const
{
	if (varName.empty())
		return false;

	if (!std::isalpha(varName.front()) && varName.front() != '_')
		return false;

	for (auto c : varName)
	{
		if (!std::isalpha(c) && !std::isdigit(c) && c != '_')
			return false;
	}

	return true;
}

uint32_t AQUA_NAMESPACE::MaterialPostprocessor::GetBasicTypeIdx(const Token& token) const
{
	auto found = std::find_if(mBasicShaderTypes.begin(), mBasicShaderTypes.end(), [&token](const std::string& type)
		{
			if (token == type)
				return true;

			return false;
		});

	if (found == mBasicShaderTypes.end())
		return -1;

	return static_cast<uint32_t>(found - mBasicShaderTypes.begin());
}
