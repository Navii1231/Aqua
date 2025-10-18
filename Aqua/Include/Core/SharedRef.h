#pragma once
#include "AqCore.h"
#include "Config.h"

AQUA_BEGIN

#if USING_VKLIB_SHARED_REF

template <typename Type>
using SharedRef = vkLib::Core::Ref<Type>;

template <typename Type>
SharedRef<Type> MakeRef(const Type& type)
{
	return vkLib::Core::CreateRef(type, [](const Type& type) { /*do nothing, let it die*/ });
}

template <typename Type, typename ...ARGS>
SharedRef<Type> MakeRef(const ARGS&&... args)
{
	return std::CreateRef<Type>(std::forward<ARGS>(args)...);
}

template <typename Type>
Type* GetRefAddr(const SharedRef<Type> shared)
{
	return shared.get();
}

#else

template <typename Type>
using SharedRef = std::shared_ptr<Type>;

template <typename Type>
SharedRef<Type> MakeRef(const Type& type)
{
	return std::make_shared<Type>(type);
}

template <typename Type, typename ...ARGS>
SharedRef<Type> MakeRef(ARGS&&... args)
{
	return std::make_shared<Type>(std::forward<ARGS>(args)...);
}

template <typename Type>
Type* GetRefAddr(const SharedRef<Type> shared)
{
	return shared.get();
}

#endif

AQUA_END
