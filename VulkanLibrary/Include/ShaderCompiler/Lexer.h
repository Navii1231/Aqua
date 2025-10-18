#pragma once
#include "ShaderConfig.h"

VK_BEGIN

struct Token
{
	std::string Value;
	int LineNumber = 0;
	int CharOffset = 0;

	size_t Position = -1;
};

class Lexer
{
public:
	VKLIB_API Lexer(const std::string& Source);

	VKLIB_API Token& NextToken();
	VKLIB_API const std::vector<Token>& CreateTokenStream();
	VKLIB_API void Reset();

	VKLIB_API void SetCursor(size_t position);

	VKLIB_API void SetWhiteSpacesAndDelimiters(const std::string& spaces, const std::string& delimiters);

private:
	std::string mSource;
	std::vector<Token> mTokens;

	size_t mPlace = 0;
	size_t mLineNumber = 0;
	size_t mCharOffset = 0;

	std::string mDelimiters;
	std::string mWhiteSpaces;

private:
	void Lex();
	void SkipWhiteSpaces();
	Token ReadUntilDelimiterHits();
	bool IsInString(char val, const std::string& str);

	const char& Increment();
	void UpdateState(char Current);
};

VK_END
