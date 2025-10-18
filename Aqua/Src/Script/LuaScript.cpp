#include "Core/Aqpch.h"
#include "Script/LuaScript.h"
#include "Lua/Lua.hpp"

#include "Script/LuaMatrixImpl.h"

AQUA_NAMESPACE::LuaScript::LuaScript()
{
	mState = luaL_newstate();
	luaL_openlibs(mState);

	SetMatrixLib();
}

AQUA_NAMESPACE::LuaScript::LuaScript(lua_State* state)
{
	mState = state;
	mBorrowedState = true;
}

AQUA_NAMESPACE::LuaScript::~LuaScript()
{
	if(!mBorrowedState)
		lua_close(mState);
}

void AQUA_NAMESPACE::LuaScript::SetInstructions(const std::string& instructions)
{
	mInstructions = instructions;
}

void AQUA_NAMESPACE::LuaScript::SetFilepath(const std::string& filepath)
{
	mInstructions = *vkLib::ReadFile(filepath);
}

void AQUA_NAMESPACE::LuaScript::Run() const
{
	int result = luaL_dostring(mState, mInstructions.c_str());
}

std::expected<int64_t, AQUA_NAMESPACE::LuaError> AQUA_NAMESPACE::LuaScript::RetrieveInteger(const std::string& name) const
{
	int val = lua_getglobal(mState, name.c_str());

	// value lies at the top of the stack 
	LuaType type = LuaType(lua_type(mState, -1));

	if (type != LuaType::eNumber)
		return std::unexpected(LuaError(val));

	int64_t result = lua_tointeger(mState, -1);
	lua_pop(mState, 1);

	return result;
}

std::expected<double, AQUA_NAMESPACE::LuaError> AQUA_NAMESPACE::LuaScript::RetrieveFloat(const std::string& name) const
{
	int val = lua_getglobal(mState, name.c_str());

	// value lies at the top of the stack 
	LuaType type = LuaType(lua_type(mState, -1));

	if (type != LuaType::eNumber)
		return std::unexpected(LuaError(val));

	double result = lua_tonumber(mState, -1);
	lua_pop(mState, 1);

	return result;
}

std::expected<std::string, AQUA_NAMESPACE::LuaError> AQUA_NAMESPACE::LuaScript::RetrieveString(const std::string& name) const
{
	int val = lua_getglobal(mState, name.c_str());

	// value lies at the top of the stack 
	LuaType type = LuaType(lua_type(mState, -1));

	if (type != LuaType::eString)
		return std::unexpected(LuaError(val));

	const char* rawString = lua_tostring(mState, -1);
	lua_pop(mState, 1);

	return rawString;
}

std::expected<AQUA_NAMESPACE::LuaSymbol, AQUA_NAMESPACE::LuaError> AQUA_NAMESPACE::LuaScript::RetrieveSymbol(const std::string& name) const
{
	// C++ lacks reflection mechanisms on its user defined types
	// making it difficult to mirror the type architecture and type check dynamically

	// here we retrieve the data from the lua stack
	int err = lua_getglobal(mState, name.c_str());
	return RetrieveSymbol(-1);
}

void AQUA_NAMESPACE::LuaScript::RetrieveTable(LuaTable& symbol, int idx) const
{
	lua_pushnil(mState);

	// if we are using relative indices, we make sure that the index is updated
	idx -= (idx < 0);

	while (lua_next(mState, idx) != 0)
	{
		auto& [keySym, valSym] = symbol.KeyValuePairs.emplace_back();

		valSym = RetrieveSymbol(-1);
		PopStack(1);
		keySym = RetrieveSymbol(-1);

		GetStackCount();
	}

	GetStackCount();
}

AQUA_NAMESPACE::LuaSymbol AQUA_NAMESPACE::LuaScript::RetrieveSymbol(int idx) const
{
	LuaType type = LuaType(lua_type(mState, idx));

	LuaSymbol symbol;
	symbol.Typename = type;

	switch (type)
	{
		case LuaType::eNone:
			break;
		case LuaType::eNil:
			symbol.Nil = true;
			break;
		case LuaType::eBoolean:
			symbol.Boolean = lua_toboolean(mState, idx);
			break;
		case LuaType::eLightUserData:
			symbol.Pointer = lua_touserdata(mState, idx);
			break;
		case LuaType::eNumber:
			symbol.Number = lua_tonumber(mState, idx);
			break;
		case LuaType::eString:
			symbol.String = lua_tostring(mState, idx);
			break;
		case LuaType::eTable:
			// this is where we recursively traverse the fields
			RetrieveTable(symbol.Table, idx);
			break;
		case LuaType::eFunction:
			symbol.CFunction = lua_tocfunction(mState, idx);
			break;
		case LuaType::eUserData:
			symbol.pUserData = (void*)lua_touserdata(mState, idx);
			break;
		case LuaType::eThread:
			symbol.Thread = lua_tothread(mState, idx);
			break;
		case LuaType::eNumTypes:
			// invalid type
			break;
		default:
			break;
	}

	return symbol;
}

AQUA_NAMESPACE::LuaError AQUA_NAMESPACE::LuaScript::SetTable(const std::string& name, const LuaTable& table, const std::string& metaTableName /*= {}*/) const
{
	lua_newtable(mState);
	AddKeyValPair(table);
	lua_setglobal(mState, name.c_str());

	if (metaTableName.empty())
		return LuaError::eOk;

	lua_getglobal(mState, name.c_str());
	luaL_setmetatable(mState, metaTableName.c_str());

	PopStack(1);

	return LuaError::eOk;
}

AQUA_NAMESPACE::LuaError AQUA_NAMESPACE::LuaScript::Register(const std::string& name, lua_CFunction fn)
{
	lua_register(mState, name.c_str(), fn);

	return LuaError::eOk;
}

AQUA_NAMESPACE::LuaError AQUA_NAMESPACE::LuaScript::SetMetaTable(const std::string& name, const LuaTable& table) const
{
	luaL_newmetatable(mState, name.c_str());
	AddKeyValPair(table);
	PopStack(1);

	return LuaError::eOk;
}

void AQUA_NAMESPACE::LuaScript::BeginType(const std::string& name) const
{
	// create the table and wait for methods to come
	lua_newtable(mState); // stack: {..., table}
	mCurrScopedType = name;
}

void AQUA_NAMESPACE::LuaScript::SubmitFunction(const std::string& fnName, InvokerT invoker) const
{
	// Wrap the callable into a std::function invoker
	// yet to find a way to dynamically type pun the arguments into what fn expects

	auto invokerRef = MakeRef<InvokerT>(std::forward<InvokerT>(invoker));
	mInvokers[mCurrScopedType].push(invokerRef);

	// Upvalue 1: function name
	lua_pushstring(mState, fnName.c_str());

	// Upvalue 2: invoker pointer
	lua_pushlightuserdata(mState, GetRefAddr(invokerRef));

	// Upvalue 3: type data (optional, can be nullptr)
	lua_pushlightuserdata(mState, nullptr);

	// Closure with 3 upvalues
	lua_pushcclosure(mState, [](lua_State* state) -> int
	{
		// Grab upvalues
		const char* funcName = lua_tostring(state, lua_upvalueindex(1));
		auto* invoker = static_cast<InvokerT*>(lua_touserdata(state, lua_upvalueindex(2)));
		void* typeData = lua_touserdata(state, lua_upvalueindex(3));

		// Collect args
		LuaScript script(state);
		auto args = script.GetStackElems();

		// Forward to the invoker
		auto retVals = (*invoker)(args);

		// pushing the return lua symbols
		for (const auto& symbol : retVals)
			script.PushSymbol(symbol);

		return 1;
	}, 3);

	// Register closure
	lua_setfield(mState, -2, fnName.c_str());
}

void AQUA_NAMESPACE::LuaScript::EndType() const
{
	lua_setglobal(mState, mCurrScopedType.c_str()); // stack: {...}

	mCurrScopedType = {};
}

void AQUA_NAMESPACE::LuaScript::RemoveType(const std::string& typeName)
{
	lua_pushnil(mState);
	lua_setglobal(mState, typeName.c_str());

	if (mInvokers.find(typeName) != mInvokers.end())
		mInvokers.erase(typeName);
}

std::vector<AQUA_NAMESPACE::LuaSymbol> AQUA_NAMESPACE::LuaScript::GetStackElems() const
{
	int elemCount = GetStackCount();

	std::vector<LuaSymbol> symbols{};
	symbols.reserve(elemCount);

	for (int i = 1; i <= elemCount; i++)
	{
		symbols.emplace_back(RetrieveSymbol(i));
		GetStackCount();
	}

	return symbols;
}

void AQUA_NAMESPACE::LuaScript::ClearStack() const
{
	PopStack(lua_gettop(mState));
}

int AQUA_NAMESPACE::LuaScript::GetStackCount() const
{
	return lua_gettop(mState);
}

std::vector<AQUA_NAMESPACE::LuaSymbol> AQUA_NAMESPACE::LuaScript::CallFn(const std::string& name, const std::vector<LuaSymbol>& args, int expretcount /*= 1*/) const
{
	lua_getglobal(mState, name.c_str());

	// stack manipulation to push arguments
	for (const auto& sym : args)
		PushSymbol(sym);

	lua_pcall(mState, static_cast<int>(args.size()), expretcount, 0);

	std::vector<LuaSymbol> retSyms;
	retSyms.reserve(expretcount);

	for (int i = 0; i < expretcount; i++)
	{
		retSyms.push_back(RetrieveSymbol(-1));
		PopStack(1);
	}

	return retSyms;
}

void AQUA_NAMESPACE::LuaScript::PopStack(int size) const
{
	// pop the stack upto given depth
	lua_pop(mState, size);
}

void AQUA_NAMESPACE::LuaScript::AddKeyValPair(const LuaTable& table) const
{
	for (const auto& [key, value] : table.KeyValuePairs)
	{
		PushSymbol(key);
		PushSymbol(value);

		lua_settable(mState, -3);
	}
}

void AQUA_NAMESPACE::LuaScript::PushSymbol(const LuaSymbol& symbol) const
{
	void* pUserData = nullptr;

	switch (symbol.Typename)
	{
		case LuaType::eNone:
			break;
		case LuaType::eNil:
			lua_pushnil(mState);
			break;
		case LuaType::eBoolean:
			lua_pushboolean(mState, symbol.Boolean);
			break;
		case LuaType::eLightUserData:
			lua_pushlightuserdata(mState, symbol.Pointer);
			break;
		case LuaType::eNumber:
			lua_pushnumber(mState, symbol.Number);
			break;
		case LuaType::eString:
			lua_pushstring(mState, symbol.String.c_str());
			break;
		case LuaType::eTable:
			// table is where we've to recursively traverse the fields
			AddKeyValPair(symbol.Table);
			break;
		case LuaType::eFunction:
			lua_pushcfunction(mState, symbol.CFunction);
			break;
		case LuaType::eUserData:
			pUserData = (uint8_t*)lua_newuserdata(mState, symbol.PushedData.size());
			std::memcpy(pUserData, symbol.PushedData.data(), symbol.PushedData.size());
			break;
		case LuaType::eThread:
			lua_pushthread(symbol.Thread);
			break;
		case LuaType::eNumTypes:
			// invalid type
			break;
		default:
			break;
	}
}

void AQUA_NAMESPACE::LuaScript::SetMatrixLib()
{
	LuaMatrix<lua_Number, 2, 1> dvec2(mState);
	LuaMatrix<lua_Number, 3, 1> dvec3(mState);
	LuaMatrix<lua_Number, 4, 1> dvec4(mState);
	LuaMatrix<lua_Number, 2, 2> dmat2x2(mState);
	LuaMatrix<lua_Number, 3, 2> dmat2x3(mState);
	LuaMatrix<lua_Number, 4, 2> dmat2x4(mState);
	LuaMatrix<lua_Number, 2, 3> dmat3x2(mState);
	LuaMatrix<lua_Number, 3, 3> dmat3x3(mState);
	LuaMatrix<lua_Number, 4, 3> dmat3x4(mState);
	LuaMatrix<lua_Number, 2, 4> dmat4x2(mState);
	LuaMatrix<lua_Number, 3, 4> dmat4x3(mState);
	LuaMatrix<lua_Number, 4, 4> dmat4x4(mState);

	LuaMatrix<lua_Integer, 2, 1> ivec2(mState);
	LuaMatrix<lua_Integer, 3, 1> ivec3(mState);
	LuaMatrix<lua_Integer, 4, 1> ivec4(mState);
	LuaMatrix<lua_Integer, 2, 2> imat2x2(mState);
	LuaMatrix<lua_Integer, 3, 2> imat2x3(mState);
	LuaMatrix<lua_Integer, 4, 2> imat2x4(mState);
	LuaMatrix<lua_Integer, 2, 3> imat3x2(mState);
	LuaMatrix<lua_Integer, 3, 3> imat3x3(mState);
	LuaMatrix<lua_Integer, 4, 3> imat3x4(mState);
	LuaMatrix<lua_Integer, 2, 4> imat4x2(mState);
	LuaMatrix<lua_Integer, 3, 4> imat4x3(mState);
	LuaMatrix<lua_Integer, 4, 4> imat4x4(mState);

	SetMetaTable(dvec2.MetaTableName(), dvec2.mMetaTable);
	SetMetaTable(dvec3.MetaTableName(), dvec3.mMetaTable);
	SetMetaTable(dvec4.MetaTableName(), dvec4.mMetaTable);
	SetMetaTable(dmat2x2.MetaTableName(), dmat2x2.mMetaTable);
	SetMetaTable(dmat2x3.MetaTableName(), dmat2x3.mMetaTable);
	SetMetaTable(dmat2x4.MetaTableName(), dmat2x4.mMetaTable);
	SetMetaTable(dmat3x2.MetaTableName(), dmat2x2.mMetaTable);
	SetMetaTable(dmat3x3.MetaTableName(), dmat2x3.mMetaTable);
	SetMetaTable(dmat3x4.MetaTableName(), dmat2x4.mMetaTable);
	SetMetaTable(dmat4x2.MetaTableName(), dmat2x2.mMetaTable);
	SetMetaTable(dmat4x3.MetaTableName(), dmat2x3.mMetaTable);
	SetMetaTable(dmat4x4.MetaTableName(), dmat2x4.mMetaTable);

	SetMetaTable(ivec2.MetaTableName(), ivec2.mMetaTable);
	SetMetaTable(ivec3.MetaTableName(), ivec3.mMetaTable);
	SetMetaTable(ivec4.MetaTableName(), ivec4.mMetaTable);
	SetMetaTable(imat2x2.MetaTableName(), imat2x2.mMetaTable);
	SetMetaTable(imat2x3.MetaTableName(), imat2x3.mMetaTable);
	SetMetaTable(imat2x4.MetaTableName(), imat2x4.mMetaTable);
	SetMetaTable(imat3x2.MetaTableName(), imat2x2.mMetaTable);
	SetMetaTable(imat3x3.MetaTableName(), imat2x3.mMetaTable);
	SetMetaTable(imat3x4.MetaTableName(), imat2x4.mMetaTable);
	SetMetaTable(imat4x2.MetaTableName(), imat2x2.mMetaTable);
	SetMetaTable(imat4x3.MetaTableName(), imat2x3.mMetaTable);
	SetMetaTable(imat4x4.MetaTableName(), imat2x4.mMetaTable);
}
