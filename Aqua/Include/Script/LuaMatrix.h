#pragma once
#include "ScriptConfig.h"

AQUA_BEGIN

template <typename T, glm::length_t Cols, glm::length_t Rows>
struct LuaMatrix
{
	using MyType = LuaMatrix<T, Cols, Rows>;
	using MatType = glm::mat<Cols, Rows, T>;

	static void Register(lua_State* state);

	static int CreateNew(lua_State* state);
	static int ToString(lua_State* state);
	static int Index(lua_State* state);
	static int NewIndex(lua_State* state);
	static int Add(lua_State* state);
	static int Sub(lua_State* state);
	static int Mul(lua_State* state);
	static int Div(lua_State* state);
	static int GC(lua_State* state);
	static int Equal(lua_State* state);

	static std::string GetBaseMetaTableName()
	{
		std::string BaseName = "Mat";

		if constexpr (std::is_same_v<T, lua_Integer>)
		{
			BaseName = "I" + BaseName;
		}

		return BaseName;
	}

	static std::string MetaTableName()
	{
		return GetBaseMetaTableName() + std::to_string(Rows) + std::to_string(Cols);
	}

	LuaTable mMetaTable;

	LuaMatrix(lua_State* state)
	{
		mMetaTable.KeyValuePairs.emplace_back(std::string("__tostring"), ToString);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__index"), Index);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__newindex"), NewIndex);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__add"), Add);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__sub"), Sub);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__mul"), Mul);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__div"), Div);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__eq"), Equal);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__gc"), GC);

		Register(state);
	}
};

template <typename T, glm::length_t Cols>
struct LuaMatrix<T, Cols, 1>
{
	using MyType = LuaMatrix<T, Cols, 1>;
	using MatType = glm::vec<Cols, T>;

	static void Register(lua_State* state);

	static int CreateNew(lua_State* state);
	static int ToString(lua_State* state);
	static int Index(lua_State* state);
	static int NewIndex(lua_State* state);
	static int Add(lua_State* state);
	static int Sub(lua_State* state);
	static int Mul(lua_State* state);
	static int Div(lua_State* state);
	static int GC(lua_State* state);
	static int Equal(lua_State* state);

	static std::string GetBaseMetaTableName()
	{
		std::string BaseName = "Vec";

		if constexpr (std::is_same_v<T, lua_Integer>)
		{
			BaseName = "I" + BaseName;
		}

		return BaseName;
	}

	static std::string MetaTableName()
	{
		return GetBaseMetaTableName() + std::to_string(Cols);
	}

	LuaTable mMetaTable;

	LuaMatrix(lua_State* state)
	{
		mMetaTable.KeyValuePairs.emplace_back(std::string("__tostring"), ToString);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__index"), Index);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__newindex"), NewIndex);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__add"), Add);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__sub"), Sub);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__mul"), Mul);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__div"), Div);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__eq"), Equal);
		mMetaTable.KeyValuePairs.emplace_back(std::string("__gc"), GC);	

		Register(state);
	}
};

AQUA_END
