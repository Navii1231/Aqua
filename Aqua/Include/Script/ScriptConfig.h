#pragma once
#include "../Core/AqCore.h"
#include "../../Dependencies/Include/lua/lua.hpp"
#include "Core/SharedRef.h"

AQUA_BEGIN

// #define LUA_TNONE		(-1)
// 
// #define LUA_TNIL		0
// #define LUA_TBOOLEAN		1
// #define LUA_TLIGHTUSERDATA	2
// #define LUA_TNUMBER		3
// #define LUA_TSTRING		4
// #define LUA_TTABLE		5
// #define LUA_TFUNCTION		6
// #define LUA_TUSERDATA		7
// #define LUA_TTHREAD		8
// 
// #define LUA_NUMTYPES		9


enum class LuaError
{
	eOk                      = 0,
	eYield                   = 1,
	eErrRun                  = 2,
	eErrSyntax               = 3,
	eErrMem                  = 4,
	eErrErr                  = 5,
};

enum class LuaType
{
	eNone              = -1,
	eNil               = 0,
	eBoolean           = 1,
	eLightUserData     = 2,
	eNumber            = 3,
	eString            = 4,
	eTable             = 5,
	eFunction          = 6,
	eUserData          = 7,
	eThread            = 8,
	eNumTypes          = 9,
};

/*
LUA_API lua_Number(lua_tonumberx) (lua_State* L, int idx, int* isnum);
LUA_API lua_Integer(lua_tointegerx) (lua_State* L, int idx, int* isnum);
LUA_API int             (lua_toboolean)(lua_State* L, int idx);
LUA_API const char* (lua_tolstring)(lua_State* L, int idx, size_t* len);
LUA_API lua_Unsigned(lua_rawlen) (lua_State* L, int idx);
LUA_API lua_CFunction(lua_tocfunction) (lua_State* L, int idx);
LUA_API void* (lua_touserdata)(lua_State* L, int idx);
LUA_API lua_State* (lua_tothread)(lua_State* L, int idx);
LUA_API const void* (lua_topointer)(lua_State* L, int idx);
*/

struct LuaSymbol;

struct LuaTable
{
	std::vector<std::pair<LuaSymbol, LuaSymbol>> KeyValuePairs;
};

struct LuaSymbol
{
	LuaType Typename = LuaType::eNil;

	bool Nil = false;

	void* pUserData;
	std::vector<uint8_t> PushedData;

	union
	{
		double Number;
		int64_t Integer;
		bool Boolean;
		lua_CFunction CFunction;
		lua_State* Thread;
		void* Pointer;
	};

	std::string String;
	LuaTable Table; // for nested tables, we keep track of the children fields

	LuaSymbol() = default;

	LuaSymbol(const std::string& val) : Typename(LuaType::eString), String(val) {}
	LuaSymbol(double number) : Typename(LuaType::eNumber), Number(number) {}
	LuaSymbol(int64_t integer) : Typename(LuaType::eNumber), Integer(integer) {}
	LuaSymbol(bool boolean) : Typename(LuaType::eBoolean), Boolean(boolean) {}
	LuaSymbol(lua_CFunction fn) : Typename(LuaType::eFunction), CFunction(fn) {}
	LuaSymbol(lua_State* thread) : Typename(LuaType::eThread), Thread(thread) {}
	LuaSymbol(void* ptr) : Typename(LuaType::eLightUserData), Pointer(ptr) {}
	LuaSymbol(const LuaTable& table) : Typename(LuaType::eTable), Table(table) {}
};

using LuaSymbolList = std::vector<LuaSymbol>;

using InvokerT = std::function<LuaSymbolList(const LuaSymbolList&)>;
using InvokerTRef = SharedRef<InvokerT>;


AQUA_END

