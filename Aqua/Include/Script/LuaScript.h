#pragma once
#include "ScriptConfig.h"
#include "Core/SharedRef.h"

#include "../Utils/Lexer.h"

AQUA_BEGIN

class LuaScript
{
public:
	AQUA_API LuaScript();
	AQUA_API LuaScript(lua_State* state);
	AQUA_API ~LuaScript();

	LuaScript(const LuaScript&) = delete;
	LuaScript& operator=(const LuaScript&) = delete;

	AQUA_API void SetInstructions(const std::string& instructions);
	AQUA_API void SetFilepath(const std::string& filepath);

	AQUA_API void Run() const;

	// basic types
	AQUA_API std::expected<int64_t, LuaError> RetrieveInteger(const std::string& name) const;
	AQUA_API std::expected<double, LuaError> RetrieveFloat(const std::string& name) const;
	AQUA_API std::expected<std::string, LuaError> RetrieveString(const std::string& name) const;

	// covers basic types as well as tables, functions, user data
	AQUA_API std::expected<LuaSymbol, LuaError> RetrieveSymbol(const std::string& name) const;

	AQUA_API LuaError SetTable(const std::string& name, const LuaTable& table, const std::string& metaTableName = {}) const;

	AQUA_API LuaError Register(const std::string& name, lua_CFunction fn);

	AQUA_API LuaError SetMetaTable(const std::string& name, const LuaTable& table) const;

	// todo: will implement later
	AQUA_API LuaError SetSymbol(const std::string& name, const LuaSymbol& symbol) const;

	AQUA_API void BeginType(const std::string& name) const;
	AQUA_API void SubmitFunction(const std::string& fnName, InvokerT fn) const;
	AQUA_API void EndType() const;

	AQUA_API void RemoveType(const std::string& typeName);

	AQUA_API std::vector<LuaSymbol> GetStackElems() const;

	template <typename T, typename ...ARGS>
	T* Allocate(ARGS&&... args) const;

	AQUA_API void ClearStack() const;
	AQUA_API int GetStackCount() const;

	AQUA_API std::vector<LuaSymbol> CallFn(const std::string& name, const std::vector<LuaSymbol>& args, int expretcount = 1) const;

	lua_State* GetState() const { return mState; }

private:
	lua_State* mState;

	std::string mInstructions; // run by lua interpreter

	bool mBorrowedState = false;

	// type name to queue
	mutable std::string mCurrScopedType;
	mutable std::unordered_map<std::string, std::queue<InvokerTRef>> mInvokers;

	// the data and stuff that client needs to do with lua

	// this probably consist of setting up metatables, host functions
	// which defers the instructions and data to lua state machine

private:
	void RetrieveTable(LuaTable& symbol, int idx) const;
	LuaSymbol RetrieveSymbol(int idx) const;

	void PopStack(int size) const;

	void AddKeyValPair(const LuaTable& table) const;
	void PushSymbol(const LuaSymbol& symbol) const;

	void SetMatrixLib();
};

template <typename T, typename ...ARGS>
T* AQUA_NAMESPACE::LuaScript::Allocate(ARGS&&... args) const
{
	T* ptr = (T*)lua_newuserdata(mState, sizeof(T));
	std::construct_at(ptr, std::forward<ARGS>(args)...);
	return ptr;
}

AQUA_END

