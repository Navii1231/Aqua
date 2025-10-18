#pragma once
#include "../Core/AqCore.h"

#include <vector>
#include <string>
#include <string_view>

AQUA_BEGIN

template <typename Char>
struct BasicToken
{
	std::basic_string_view<Char> Lexeme;
	int LineNo = 0;
	int CharOff = 0;

	size_t PosInStr = static_cast<size_t>(0);
};

template <typename Char>
class BasicLexer
{
public:
	using MyStringView = std::basic_string_view<Char>;
	using MyString = std::basic_string<Char>;
	using MyToken = BasicToken<Char>;

public:
	BasicLexer() = default;

	BasicLexer(const BasicLexer&) = delete;
	BasicLexer& operator =(const BasicLexer&) = delete;

	inline MyToken Advance();
	inline MyToken Peek(size_t off = 1);

	inline void SetMarker();
	inline void RetrieveMarker();

	inline void Reset();

	// NOTE: The string must not be destroyed while the class ::AQUA_NAMESPACE::Lexer is using it
	inline void SetString(const MyStringView& source);
	inline void SetWhiteSpacesAndDelimiters(const MyString& spaces, const MyString& delimiters);

	const MyToken& GetCurrent() const { return mCurrent; }
	bool HasConsumed() const { return mPosInStr >= mSource.size(); }

	size_t GetPosition() const { return mPosInStr; }

	inline std::vector<MyToken> Lex();
	inline std::vector<MyToken> LexString(const MyStringView& view);

private:
	MyStringView mSource;
	MyToken mCurrent;
	MyToken mMarkerToken;

	size_t mPosInStr = 0;

	size_t mLineNumber = 0;
	size_t mCharOffset = 0;

	MyString mDelimiters;
	MyString mWhiteSpaces;

private:

	inline void SkipWhiteSpaces();
	inline MyToken ReadUntilDelimiterHits();
	inline bool IsInString(char val, const MyString& str);

	inline char Increment();
	inline void UpdateState(char Current);
};

using Token = BasicToken<char>;
using Lexer = BasicLexer<char>;

AQUA_END

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::SetString(const MyStringView& source)
{
	mSource = source;
}

template <typename Char>
typename AQUA_NAMESPACE::BasicLexer<Char>::MyToken AQUA_NAMESPACE::BasicLexer<Char>::Advance()
{
	if (HasConsumed())
		return {};

	SkipWhiteSpaces();
	mCurrent = ReadUntilDelimiterHits();

	return mCurrent;
}

template <typename Char>
typename AQUA_NAMESPACE::BasicLexer<Char>::MyToken AQUA_NAMESPACE::BasicLexer<Char>::Peek(size_t off)
{
	if (off == 0)
		return GetCurrent();

	MyToken curr = mCurrent;

	SetMarker();

	while (off != 0)
	{
		if (HasConsumed())
			break;

		SkipWhiteSpaces();
		curr = ReadUntilDelimiterHits();
		off--;
	}

	RetrieveMarker();

	return curr;
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::SetMarker()
{
	mMarkerToken = mCurrent;
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::RetrieveMarker()
{
	mCurrent = mMarkerToken;
	mCharOffset = mCurrent.CharOff;
	mLineNumber = mCurrent.LineNo;
	mPosInStr = mCurrent.PosInStr;
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::Reset()
{
	mPosInStr = 0;
	mLineNumber = 0;
	mCharOffset = 0;
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::SetWhiteSpacesAndDelimiters(const MyString& spaces, const MyString& delimiters)
{
	mWhiteSpaces = spaces;
	mDelimiters = delimiters;

	mDelimiters.append(mWhiteSpaces);
}

template <typename Char>
std::vector<typename AQUA_NAMESPACE::BasicLexer<Char>::MyToken> AQUA_NAMESPACE::BasicLexer<Char>::Lex()
{
	std::vector<MyToken> tokens;

	Reset();

	while (!HasConsumed())
	{
		tokens.push_back(Advance());
	}

	return tokens;
}

template <typename Char>
std::vector<typename AQUA_NAMESPACE::BasicLexer<Char>::MyToken> AQUA_NAMESPACE::BasicLexer<Char>::LexString(const MyStringView& view)
{
	// Create a new token to store new string
	Lexer lexer{};
	lexer.SetWhiteSpacesAndDelimiters(mWhiteSpaces, mDelimiters);
	lexer.SetString(view);

	return lexer.Lex();
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::SkipWhiteSpaces()
{
	_STL_ASSERT(!mSource.empty(), "The Lexer string was empty!");

	const char* Src = mSource.begin()._Unwrapped();
	char Current = Src[mPosInStr];

	while (IsInString(Current, mWhiteSpaces) && Current != '\0')
	{
		Current = Src[++mPosInStr];
		UpdateState(Current);
	}
}

template <typename Char>
typename AQUA_NAMESPACE::BasicLexer<Char>::MyToken AQUA_NAMESPACE::BasicLexer<Char>::ReadUntilDelimiterHits()
{
	_STL_ASSERT(!mSource.empty(), "The Lexer string was empty!");

	if (HasConsumed())
		return {};

	MyToken next;
	next.LineNo = static_cast<int>(mLineNumber);
	next.CharOff = static_cast<int>(mCharOffset);
	next.PosInStr = mPosInStr;

	auto Src = mSource.data();

	char Current = Src[mPosInStr++];

	// Checking if we are directly hitting a delimiter
	if (IsInString(Current, mDelimiters) || Current == '\0')
	{
		next.Lexeme = mSource.substr(next.PosInStr, 1);
		return next;
	}

	Current = Src[mPosInStr];

	// Scan till we don't run into a delimiter, but don't include it
	while (!IsInString(Current, mDelimiters) && Current != '\0')
		Current = Increment();

	next.Lexeme = mSource.substr(next.PosInStr, mPosInStr - next.PosInStr);

	return next;
}

template <typename Char>
bool AQUA_NAMESPACE::BasicLexer<Char>::IsInString(char val, const MyString& str)
{
	auto found = std::find(str.begin(), str.end(), val);
	return found != str.end();
}

template <typename Char>
char AQUA_NAMESPACE::BasicLexer<Char>::Increment()
{
	auto Src = mSource.data();
	char Current = Src[++mPosInStr];

	if (Current == '\n')
	{
		mLineNumber++;
		mCharOffset = 0;
		return Current;
	}

	mCharOffset++;

	return Current;
}

template <typename Char>
void AQUA_NAMESPACE::BasicLexer<Char>::UpdateState(char Current)
{
	if (Current == '\n')
	{
		mLineNumber++;
		mCharOffset = 0;
		return;
	}

	mCharOffset++;
}
