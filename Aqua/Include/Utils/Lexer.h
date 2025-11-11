#pragma once
#include "../Core/AqCore.h"

#include <vector>
#include <string>
#include <string_view>

AQUA_BEGIN

struct Cursors
{
	size_t PosOff = 0;
	size_t LineOff = 0;
	size_t CharOff = 0;
};

// one way to solve is to inherit the string view class
// but that would break the existing codebase
// okay, let's bring the revolution
template <typename Char>
struct BasicToken : public std::basic_string_view<Char>
{
	using MyStr = std::basic_string<Char>;
	using MyStrView = std::basic_string_view<Char>;

	BasicToken() = default;
	~BasicToken() = default;

	template <typename _Expr>
	BasicToken(const _Expr& strExpr)
		: MyStrView(strExpr) {}

	template <typename _Expr>
	BasicToken& operator=(const _Expr& strExpr)
	{
		*this = BasicToken(strExpr);
		return *this;
	}

	operator MyStr() const { return MyStr(*this); }
	operator MyStrView() const { return MyStrView(*this); }
};

template <typename Char, typename TokenType = BasicToken<Char>>
class BasicLexer
{
public:
	using MyStringView = std::basic_string_view<Char>;
	using MyString = std::basic_string<Char>;
	using MyToken = TokenType;

public:
	BasicLexer() = default;

	BasicLexer(const BasicLexer&) = default;
	BasicLexer& operator =(const BasicLexer&) = default;

	inline MyToken Advance(int64_t _off = 1);
	inline MyToken Peek(size_t off = 1) const;

	inline void SetMarker();
	inline void RetrieveMarker();

	inline void SetCursors(size_t posOff, size_t lineOff = -1, size_t charOff = -1);
	inline void CutOff(size_t posOff);
	inline void Reset();

	inline BasicLexer Inherit(int64_t offset, size_t count = -1) const;

	inline std::string_view GetString() const { return mSource; }

	// NOTE: The string must not be destroyed while the class ::AQUA_NAMESPACE::Lexer is using it
	inline void SetString(const MyStringView& source);
	inline void SetWhiteSpacesAndDelimiters(const MyString& spaces, const MyString& delimiters);

	const MyToken& GetCurrent() const { return mCurrent; }
	bool HasConsumed() const { return mCursors.PosOff >= mSource.size(); }

	size_t GetPosition() const { return mCursors.PosOff - mCurrent.size(); }
	Cursors GetCursors() const { return mCursors; }

	MyToken operator*() const { return mCurrent; }
	const MyToken* operator->() const { return &mCurrent; }
	Char operator[](size_t off) const { return mSource[off]; }

	MyToken operator++(int)
	{ auto curr = GetCurrent(); Advance(1); return curr; }
	MyToken operator--(int)
	{ auto curr = GetCurrent(); Advance(-1); return curr; }

	BasicLexer& operator++() { Advance(1); return *this; }
	BasicLexer& operator--() { Advance(-1); return *this; }

	BasicLexer& operator+=(size_t _off) { Advance(_off); return *this; }
	BasicLexer& operator-=(size_t _off) { Advance(-_off); return *this; }

	MyToken operator+(size_t _off) const { return Peek(_off); }
	MyToken operator-(size_t _off) const { return Peek(-_off); }

	inline std::vector<MyToken> Lex();
	inline std::vector<MyToken> LexString(const MyStringView& view);

private:
	MyStringView mSource;
	MyToken mCurrent;
	MyToken mMarkerToken;

	Cursors mCursors, mMarkedEncoders;

	MyString mDelimiters;
	MyString mWhiteSpaces;

private:
	inline void SkipWhiteSpaces(Cursors& encoder) const;
	inline MyToken ReadUntilDelimiterHits(Cursors& encoder) const;
	inline bool IsInString(char val, const MyString& str) const;

	inline char Increment(Cursors& encoder) const;
	inline void UpdateEncoder(char Current, Cursors& encoder) const;
};

using Token = BasicToken<char>;
using Lexer = BasicLexer<char>;

AQUA_END

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::SetString(const MyStringView& source)
{
	mSource = source;
}

template <typename Char, typename TokenType>
typename AQUA_NAMESPACE::BasicLexer<Char, TokenType>::MyToken AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Advance(int64_t _off /*= 1*/)
{
	if (HasConsumed())
		return {};

	// relying on compiler optimizations
	// the compiler will strip away the while loop and 
	// if condition for default '_off' value
	if (_off == 0)
		return GetCurrent();

	while (_off != 0)
	{
		SkipWhiteSpaces(mCursors);
		mCurrent = ReadUntilDelimiterHits(mCursors);
		_off--;
	}

	return mCurrent;
}

template <typename Char, typename TokenType>
typename AQUA_NAMESPACE::BasicLexer<Char, TokenType>::MyToken AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Peek(size_t off) const
{
	if (off == 0)
		return GetCurrent();

	MyToken curr = mCurrent;
	Cursors encoder = mCursors;

	while (off != 0)
	{
		if (HasConsumed())
			break;

		SkipWhiteSpaces(encoder);
		curr = ReadUntilDelimiterHits(encoder);
		off--;
	}

	return curr;
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::SetMarker()
{
	mMarkerToken = mCurrent;
	mMarkedEncoders = mCursors;
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::RetrieveMarker()
{
	mCurrent = mMarkerToken;
	mCursors = mMarkedEncoders;
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::SetCursors(size_t posOff, size_t lineOff /*= -1*/, size_t charOff /*= -1*/)
{
	// for line and char offs equals to -1, we shall read all the character again up until pos off

	if (posOff > mSource.size())
		return;

	mCursors.PosOff = posOff;
	mCursors.CharOff = charOff;
	mCursors.LineOff = lineOff;

	if (lineOff == -1) // recalculate the char offs
	{
		mCursors.LineOff = 0;
		mCursors.CharOff = 0;

		for (size_t i = 0; i < posOff; i++)
		{
			mCursors.LineOff += mSource[i] == '\n';
			mCursors.CharOff = mSource[i] == '\n' ? 0 : mCursors.CharOff + 1;
		}
	}
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::CutOff(size_t posOff)
{
	if (posOff >= mSource.size())
		mSource = {};

	mSource = mSource.substr(posOff); // cutting off the initialize string up to posOff...
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Reset()
{
	mCursors = {};
}

template <typename Char, typename TokenType>
AQUA_NAMESPACE::BasicLexer<Char, TokenType> AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Inherit(int64_t offset, size_t count) const
{
	BasicLexer inherited;
	inherited.SetString(mSource.substr(static_cast<int64_t>(mCursors.PosOff) + offset, count));
	inherited.SetWhiteSpacesAndDelimiters(mWhiteSpaces, mDelimiters);

	return inherited;
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::SetWhiteSpacesAndDelimiters(const MyString& spaces, const MyString& delimiters)
{
	mWhiteSpaces = spaces;
	mDelimiters = delimiters;

	mDelimiters.append(mWhiteSpaces);
}

template <typename Char, typename TokenType>
std::vector<typename AQUA_NAMESPACE::BasicLexer<Char, TokenType>::MyToken> AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Lex()
{
	std::vector<MyToken> tokens;

	Reset();

	while (!HasConsumed())
	{
		tokens.push_back(Advance());
	}

	return tokens;
}

template <typename Char, typename TokenType>
std::vector<typename AQUA_NAMESPACE::BasicLexer<Char, TokenType>::MyToken> AQUA_NAMESPACE::BasicLexer<Char, TokenType>::LexString(const MyStringView& view)
{
	// Create a new token to store new string
	Lexer lexer{};
	lexer.SetWhiteSpacesAndDelimiters(mWhiteSpaces, mDelimiters);
	lexer.SetString(view);

	return lexer.Lex();
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::SkipWhiteSpaces(Cursors& encoder) const
{
	_STL_ASSERT(!mSource.empty(), "The Lexer string was empty!");

	const char* Src = mSource.begin()._Unwrapped();
	char Current = Src[encoder.PosOff];

	while (IsInString(Current, mWhiteSpaces) && Current != '\0')
	{
		Current = Src[++encoder.PosOff];
		UpdateEncoder(Current, encoder);
	}
}

template <typename Char, typename TokenType>
typename AQUA_NAMESPACE::BasicLexer<Char, TokenType>::MyToken AQUA_NAMESPACE::BasicLexer<Char, TokenType>::ReadUntilDelimiterHits(Cursors& encoder) const
{
	_STL_ASSERT(!mSource.empty(), "The Lexer string was empty!");

	if (HasConsumed())
		return {};

	size_t prevPos = encoder.PosOff;
	auto Src = mSource.data();
	char Current = Src[encoder.PosOff++];

	// Checking if we are directly hitting a delimiter
	if (IsInString(Current, mDelimiters) || Current == '\0')
	{
		return mSource.substr(prevPos, 1);
	}

	Current = Src[encoder.PosOff];

	// Scan till we don't run into a delimiter, but don't include it
	while (!IsInString(Current, mDelimiters) && Current != '\0')
		Current = Increment(encoder);

	return mSource.substr(prevPos, encoder.PosOff - prevPos);
}

template <typename Char, typename TokenType>
bool AQUA_NAMESPACE::BasicLexer<Char, TokenType>::IsInString(char val, const MyString& str) const
{
	// TODO: could be optimized with binary search...
	auto found = std::find(str.begin(), str.end(), val);
	return found != str.end();
}

template <typename Char, typename TokenType>
char AQUA_NAMESPACE::BasicLexer<Char, TokenType>::Increment(Cursors& encoder) const
{
	auto Src = mSource.data();
	char Current = Src[++encoder.PosOff];

	if (Current == '\n')
	{
		encoder.LineOff++;
		encoder.CharOff = 0;
		return Current;
	}

	encoder.CharOff++;

	return Current;
}

template <typename Char, typename TokenType>
void AQUA_NAMESPACE::BasicLexer<Char, TokenType>::UpdateEncoder(char Current, Cursors& encoder) const
{
	if (Current == '\n')
	{
		encoder.LineOff++;
		encoder.CharOff = 0;
		return;
	}

	encoder.CharOff++;
}
