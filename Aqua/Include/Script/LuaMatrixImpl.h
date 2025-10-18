#pragma once
#include "LuaMatrix.h"
#include "lua/lua.hpp"

template <typename T, glm::length_t Cols, glm::length_t Rows>
void AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Register(lua_State* state)
{
	lua_register(state, MetaTableName().c_str(), CreateNew);
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::CreateNew(lua_State* state)
{
	int argc = lua_gettop(state);

	MatType* newMat = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(newMat, 1); // placement-new, initialize as identity

	MatType& mat = *newMat;

	// Fill matrix elements linearly from Lua arguments
	int argIndex = 1;

	for (int c = 0; c < Cols && argIndex <= argc; ++c)
	{
		for (int r = 0; r < Rows && argIndex <= argc; ++r, ++argIndex)
		{
			if constexpr (std::is_same_v<T, lua_Integer>)
				mat[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)] = static_cast<T>(lua_tointeger(state, argIndex));
			else if constexpr (std::is_same_v<T, lua_Number>)
				mat[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)] = static_cast<T>(lua_tonumber(state, argIndex));
		}
	}

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);

	return 1;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::ToString(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	std::ostringstream oss;
	oss << "mat" << Cols << "x" << Rows << "(\n";
	for (int r = 0; r < Rows; ++r)
	{
		oss << "  ";
		for (int c = 0; c < Cols; ++c)
		{
			oss << (*mat)[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)];
			if (c < Cols - 1) oss << ", ";
		}
		oss << "\n";
	}

	oss << ")";
	lua_pushstring(state, oss.str().c_str());

	return 1;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Index(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	int r = static_cast<int>(lua_tointeger(state, 2)) - 1;
	int c = static_cast<int>(lua_tointeger(state, 3)) - 1;

	if (c >= 0 && c < Cols && r >= 0 && r < Rows)
	{
		if constexpr (std::is_same_v<T, lua_Integer>)
			lua_pushinteger(state, (*mat)[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)]);
		else if constexpr (std::is_same_v<T, lua_Number>)
			lua_pushnumber(state, (*mat)[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)]);

	} else
	{
		lua_pushnil(state);
	}
	return 1;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::NewIndex(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	int r = static_cast<int>(lua_tointeger(state, 2)) - 1;
	int c = static_cast<int>(lua_tointeger(state, 3)) - 1;

	if (c >= 0 && c < Cols && r >= 0 && r < Rows)
	{
		(*mat)[static_cast<uint32_t>(c)][static_cast<uint32_t>(r)] = static_cast<T>(lua_tonumber(state, 4));
	}
	return 0;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Add(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	MatType* result = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(result, (*lhs) + (*rhs));

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);
	return 1;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Sub(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	MatType* result = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(result, (*lhs) - (*rhs));

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);
	return 1;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Mul(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	if (lua_isnumber(state, 2))
	{
		T scalar = static_cast<T>(lua_tonumber(state, 2));

		MatType* result = (MatType*)lua_newuserdata(state, sizeof(MatType));
		std::construct_at(result, (*lhs) * scalar);

		luaL_getmetatable(state, MyType::MetaTableName().c_str());
		lua_setmetatable(state, -2);

		return 1;
	}

	// if rhs has a metatable
	if (lua_getmetatable(state, 2))
	{
		//lua_pop(L, 1); // remove rhs metatable copy from stack
		// Try matrix � matrix for all possible column sizes K
		{
			std::string rhsMeta = MyType::GetBaseMetaTableName() + std::to_string(Cols) + std::to_string(2);
			std::string resultMeta = MyType::GetBaseMetaTableName() + std::to_string(2) + std::to_string(Rows);

			void* rhsUD = luaL_testudata(state, 2, rhsMeta.c_str());

			if (rhsUD)
			{
				glm::mat<2, Cols, T>* rhs = static_cast<glm::mat<2, Cols, T>*>(rhsUD);

				// Perform multiplication
				// pushes new userdata to stack
				auto* result = (glm::mat<2, Rows, T>*)lua_newuserdata(state, sizeof(glm::mat<2, Rows, T>));
				std::construct_at(result, 1);
				*result = (*lhs) * (*rhs); // assuming operator* defined

				luaL_getmetatable(state, resultMeta.c_str());
				lua_setmetatable(state, -2);

				return LUA_OK;
			}
		}

		{
			std::string rhsMeta = MyType::GetBaseMetaTableName() + std::to_string(Cols) + std::to_string(3);
			std::string resultMeta = MyType::GetBaseMetaTableName() + std::to_string(3) + std::to_string(Rows);

			void* rhsUD = luaL_testudata(state, 3, rhsMeta.c_str());

			if (rhsUD)
			{
				glm::mat<3, Cols, T>* rhs = static_cast<glm::mat<3, Cols, T>*>(rhsUD);

				// Perform multiplication
				// pushes new userdata to stack
				auto* result = (glm::mat<3, Rows, T>*)lua_newuserdata(state, sizeof(glm::mat<3, Rows, T>));
				std::construct_at(result, 1);
				*result = (*lhs) * (*rhs); // assuming operator* defined

				luaL_getmetatable(state, resultMeta.c_str());
				lua_setmetatable(state, -2);

				return LUA_OK;
			}
		}

		{
			std::string rhsMeta = MyType::GetBaseMetaTableName() + std::to_string(Cols) + std::to_string(4);
			std::string resultMeta = MyType::GetBaseMetaTableName() + std::to_string(4) + std::to_string(Rows);

			void* rhsUD = luaL_testudata(state, 4, rhsMeta.c_str());

			if (rhsUD)
			{
				glm::mat<4, Cols, T>* rhs = static_cast<typename glm::mat<4, Cols, T>*>(rhsUD);

				// Perform multiplication
				// pushes new userdata to stack
				auto* result = (glm::mat<4, Rows, T>*)lua_newuserdata(state, sizeof(glm::mat<4, Rows, T>));
				std::construct_at(result, 1);
				*result = (*lhs) * (*rhs); // assuming operator* defined

				luaL_getmetatable(state, resultMeta.c_str());
				lua_setmetatable(state, -2);

				return LUA_OK;
			}
		}

		{
			std::string rhsMeta = LuaMatrix<T, Cols, 1>::MetaTableName();
			std::string resultMeta = LuaMatrix<T, Rows, 1>::MetaTableName();

			void* rhsUD = luaL_testudata(state, 1, rhsMeta.c_str());

			if (rhsUD)
			{
				glm::vec<Cols, T>* rhs = static_cast<glm::vec<Cols, T>*>(rhsUD);

				// Perform multiplication
				// pushes new userdata to stack
				auto* result = (glm::vec<Rows, T>*)lua_newuserdata(state, sizeof(glm::vec<Rows, T>));
				std::construct_at(result, 1);
				*result = (*lhs) * (*rhs); // assuming operator* defined

				luaL_getmetatable(state, resultMeta.c_str());
				lua_setmetatable(state, -2);

				return LUA_OK;
			}
		}
	}

	return luaL_error(state, "Invalid operand type for matrix multiplication");
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Div(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	if (lua_isnumber(state, 2))
	{
		T scalar = static_cast<T>(lua_tonumber(state, 2));
		MatType* result = (MatType*)lua_newuserdata(state, sizeof(MatType));
		std::construct_at(result, (*lhs) / scalar);

		luaL_getmetatable(state, MyType::MetaTableName().c_str());
		lua_setmetatable(state, -2);
		return 1;
	}

	return luaL_error(state, "Matrix division only supports scalar");
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::GC(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	mat->~MatType(); // explicit destructor
	return 0;
}

template <typename T, glm::length_t Cols, glm::length_t Rows>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, Rows>::Equal(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	bool isEqual = (*lhs == *rhs);
	lua_pushboolean(state, isEqual);

	return 1;
}

// vector specialization

template <typename T, glm::length_t Cols>
void AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Register(lua_State* state)
{
	lua_register(state, MetaTableName().c_str(), CreateNew);
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::CreateNew(lua_State* state)
{
	int argc = lua_gettop(state);

	MatType* newVec = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(newVec, 0); // initialize as zero

	MatType& vec = *newVec;

	// Fill matrix elements linearly from Lua arguments
	int argIndex = 1;

	for (int c = 0; c < Cols && argIndex <= argc; ++c)
	{
		if constexpr (std::is_same_v<T, lua_Integer>)
			vec[static_cast<uint32_t>(c)] = static_cast<T>(lua_tointeger(state, argIndex++));
		else if constexpr (std::is_same_v<T, lua_Number>)
			vec[static_cast<uint32_t>(c)] = static_cast<T>(lua_tonumber(state, argIndex++));
	}

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);

	return 1;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::ToString(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	std::ostringstream oss;
	oss << "Vec" << Cols << "\n";
	for (int c = 0; c < Cols; ++c)
	{
		oss << (*mat)[static_cast<uint32_t>(c)] << "\n";
	}

	oss << "\n";
	lua_pushstring(state, oss.str().c_str());

	return 1;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Index(lua_State* state)
{
	// Access vec[col] by passing two indices
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	int c = static_cast<int>(lua_tointeger(state, 2)) - 1;

	if (c >= 0 && c < Cols)
	{
		if constexpr (std::is_same_v<T, lua_Integer>)
			lua_pushinteger(state, (*mat)[static_cast<uint32_t>(c)]);
		else if constexpr (std::is_same_v<T, lua_Number>)
			lua_pushnumber(state, (*mat)[static_cast<uint32_t>(c)]);

	} else
	{
		lua_pushnil(state);
	}
	return 1;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::NewIndex(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	int c = static_cast<int>(lua_tointeger(state, 2)) - 1;

	if (c >= 0 && c < Cols)
	{
		(*mat)[static_cast<uint32_t>(c)] = static_cast<T>(lua_tonumber(state, 3));
	}
	return 0;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Add(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	auto userdata = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(userdata, (*lhs) + (*rhs));

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);
	return 1;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Sub(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	auto userdata = (MatType*)lua_newuserdata(state, sizeof(MatType));
	std::construct_at(userdata, (*lhs) - (*rhs));

	luaL_getmetatable(state, MyType::MetaTableName().c_str());
	lua_setmetatable(state, -2);
	return 1;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Mul(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	if (lua_isnumber(state, 2))
	{
		// Scalar multiply
		T scalar = static_cast<T>(lua_tonumber(state, 2));

		auto* userdata = (MatType*)lua_newuserdata(state, sizeof(MatType));
		std::construct_at(userdata, (*lhs) * scalar);

		luaL_getmetatable(state, MyType::MetaTableName().c_str()); // or a unique name
		lua_setmetatable(state, -2);

		return 1;
	}

	// if rhs has a metatable
	if (lua_getmetatable(state, 2))
	{
		//lua_pop(L, 1); // remove rhs metatable copy from stack
		// Try matrix � matrix for all possible column sizes K

		{
			std::string rhsMeta = MyType::GetBaseMetaTableName() + std::to_string(Cols);
			std::string resultMeta = MyType::GetBaseMetaTableName() + std::to_string(Cols);

			void* rhsUD = luaL_testudata(state, 2, rhsMeta.c_str());

			if (rhsUD)
			{
				glm::vec<Cols, T>* rhs = static_cast<typename glm::vec<Cols, T>*>(rhsUD);

				// Perform multiplication
				// pushes new userdata to stack
				auto* result = (glm::vec<Cols, T>*)lua_newuserdata(state, sizeof(glm::vec<Cols, T>));
				std::construct_at(result, 0);
				*result = (*lhs) * (*rhs); // assuming operator* defined

				luaL_getmetatable(state, resultMeta.c_str());
				lua_setmetatable(state, -2);

				return LUA_OK;
			}
		}

		// vector x matrix product will be done later
	}

	return luaL_error(state, "Invalid operand type for matrix multiplication");
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Div(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));

	if (lua_isnumber(state, 2))
	{
		T scalar = static_cast<T>(lua_tonumber(state, 2));
		auto userdata = (MatType*)lua_newuserdata(state, sizeof(MatType));
		std::construct_at(userdata, (*lhs) / scalar);

		luaL_getmetatable(state, MyType::MetaTableName().c_str());
		lua_setmetatable(state, -2);
		return 1;
	}

	return luaL_error(state, "Matrix division only supports scalar");
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::GC(lua_State* state)
{
	MatType* mat = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	mat->~MatType();
	return 0;
}

template <typename T, glm::length_t Cols>
int AQUA_NAMESPACE::LuaMatrix<T, Cols, 1>::Equal(lua_State* state)
{
	MatType* lhs = static_cast<MatType*>(luaL_checkudata(state, 1, MyType::MetaTableName().c_str()));
	MatType* rhs = static_cast<MatType*>(luaL_checkudata(state, 2, MyType::MetaTableName().c_str()));

	bool isEqual = (*lhs == *rhs);
	lua_pushboolean(state, isEqual);

	return 1;
}

