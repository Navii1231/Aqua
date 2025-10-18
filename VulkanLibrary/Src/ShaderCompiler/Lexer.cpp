#include "Core/vkpch.h"
#include "ShaderCompiler/Lexer.h"

VK_BEGIN

Lexer::Lexer(const std::string& Source) : mSource(Source)
{}

Token& Lexer::NextToken()
{
	SkipWhiteSpaces();
	return mTokens.emplace_back(ReadUntilDelimiterHits());
}

const std::vector<Token>& Lexer::CreateTokenStream()
{
	Reset();
	Lex();
	return mTokens;
}

void Lexer::Reset()
{
	mTokens.clear();
	mPlace = 0;
	mLineNumber = 0;
	mCharOffset = 0;
}

void Lexer::SetWhiteSpacesAndDelimiters(const std::string& spaces, const std::string& delimiters)
{
	mWhiteSpaces = spaces;
	mDelimiters = delimiters;

	mDelimiters.append(mWhiteSpaces);
}

void Lexer::Lex()
{
	while (NextToken().Value.front()) {}
}

void Lexer::SkipWhiteSpaces()
{
	const char* Src = mSource.c_str();
	char Current = Src[mPlace];

	while (IsInString(Current, mWhiteSpaces) && Current != 0)
	{
		Current = Src[++mPlace];
		UpdateState(Current);
	}
}

Token Lexer::ReadUntilDelimiterHits()
{
	Token next;
	next.LineNumber = static_cast<int>(mLineNumber);
	next.CharOffset = static_cast<int>(mCharOffset);

	size_t Initial = mPlace;

	const char* Src = mSource.c_str();
	char Current = Increment();

	while (!IsInString(Current, mDelimiters) && Current != 0)
		Current = Src[mPlace++];

	size_t Length = mPlace - Initial;
	next.Position = Initial;

	if (Length == 1)
	{
		next.Value = Current;
		return next;
	}

	mPlace--;
	next.Value.reserve(--Length);
	next.Value = mSource.substr(Initial, Length);

	return next;
}

bool Lexer::IsInString(char val, const std::string& str)
{
	auto found = std::find(str.begin(), str.end(), val);
	return found != str.end();
}

void Lexer::SetCursor(size_t position)
{
	_STL_ASSERT(position <= mSource.size(), "Cursor position out of bounds!");
	mPlace = position;
}

const char& Lexer::Increment()
{
	const char& Current = mSource[mPlace++];

	if (Current == '\n')
	{
		mLineNumber++;
		mCharOffset = 0;
		return Current;
	}

	mCharOffset++;

	return Current;
}

void Lexer::UpdateState(char Current)
{
	if (Current == '\n')
	{
		mLineNumber++;
		mCharOffset = 0;
		return;
	}

	mCharOffset++;
}

VK_END

