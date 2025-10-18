#pragma once
#include "vkpch.h"

VK_BEGIN
VK_CORE_BEGIN

template <typename T, typename Deleter>
class UniControlBlock
{
public:
	using MyType = T;
	using MyDeleter = Deleter;

public:
	UniControlBlock(const T& handle, Deleter deleter)
		: mHandle(handle), mDeleter(deleter) {}

	void DeleteObject() { mDeleter(mHandle); }

	T& GetHandle() { return mHandle; }
	const T& GetHandle() const { return mHandle; }

private:
	T mHandle;
	Deleter mDeleter;

	template <typename T>
	friend class UniRef;
};

template <typename T, typename Deleter>
class UniRef
{
public:
	using MyBlock = UniControlBlock<T, std::function<void(T)>>;
	using MyHandle = typename MyBlock::MyType;
	using MyDeleter = typename MyBlock::MyDeleter;

public:
	UniRef() = default;

	UniRef(T handle, const MyDeleter& deleter)
		: mBlock(new MyBlock(handle, deleter)) {}

	// Copying is not allowed...
	UniRef(const UniRef&) = delete;
	UniRef& operator =(const UniRef&) = delete;

	// Objects must share the same deleter
	// This might cause problems if you use a lambda as a deleter!
	UniRef(UniRef&& Other) noexcept;

	// Objects must share the same deleter
	// This might cause problems if you use a lambda as a deleter!
	UniRef& operator=(UniRef&& Other) noexcept;

	void Reset();

	MyHandle* operator->() const { return &mBlock->mHandle; }

	MyHandle& operator*() { return mBlock->mHandle; }
	const MyHandle& operator*() const { return mBlock->mHandle; }

	explicit operator bool() const { return mBlock; }

	~UniRef() { Reset(); }

private:
	MyBlock* mBlock = nullptr;
};

template <typename T, typename Deleter>
VK_NAMESPACE::VK_CORE::UniRef<T, Deleter>::UniRef(UniRef&& Other) noexcept
	: mBlock(nullptr)
{
	std::swap(mBlock, Other.mBlock);
}

template <typename T, typename Deleter>
VK_NAMESPACE::VK_CORE::UniRef<T, Deleter>& VK_NAMESPACE::VK_CORE::
	UniRef<T, Deleter>::operator=(UniRef&& Other) noexcept
{
	std::swap(mBlock, Other.mBlock);
}

template <typename T, typename Deleter>
void VK_NAMESPACE::VK_CORE::UniRef<T, Deleter>::Reset()
{
	mBlock->DeleteObject();
	delete mBlock;

	mBlock = nullptr;
}

VK_CORE_END
VK_END
