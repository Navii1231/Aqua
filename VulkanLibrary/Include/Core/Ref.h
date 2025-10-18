#pragma once
#include "Config.h"

VK_BEGIN
VK_CORE_BEGIN

template <typename T, typename Deleter>
class ControlBlock
{
public:
	using MyType = T;
	using MyDeleter = Deleter;

public:
	ControlBlock() = default;
	~ControlBlock() = default;

	template <typename ...ARGS>
	ControlBlock(ARGS&&... args)
		: mHandle(std::forward<ARGS>(args)...) {}

	void Inc() { mRefCount++; }
	void Dec() { mRefCount--; }

	void DeleteObject() { mDeleter(mHandle); }

	T& GetHandle() { return mHandle; }
	const T& GetHandle() const { return mHandle; }

private:
	std::atomic<size_t> mRefCount;
	T mHandle;
	Deleter mDeleter;

	template <typename T>
	friend class Ref;

	template <typename Var, typename Fn, typename ...ARGS>
	friend Ref<Var> CreateRef(Fn&&, ARGS&&...);
};

template <typename T>
class Ref
{
public:
	using MyBlock = ControlBlock<T, std::function<void(T&)>>;
	using MyHandle = typename MyBlock::MyType;
	using MyDeleter = typename MyBlock::MyDeleter;

public:
	Ref() = default;

	Ref(const Ref& Other)
		: mBlock(Other.mBlock) { if(mBlock) mBlock->Inc(); }

	Ref& operator =(const Ref& Other);

	// Sets a new payload and deletes the existing one
	void SetValue(const T& handle);
	void ReplaceValue(const T& handle);

	void Reset();
	
	MyHandle* operator->() const { return &mBlock->mHandle; }

	MyHandle& operator*() { return mBlock->mHandle; }
	const MyHandle& operator*() const { return mBlock->mHandle; }

	bool operator==(const Ref& Other) const { return mBlock == Other.mBlock; }
	bool operator!=(const Ref& Other) const { return mBlock != Other.mBlock; }

	explicit operator bool() const { return mBlock; }

	~Ref() { Reset(); }

private:
	MyBlock* mBlock = nullptr;

private:
	// Helper functions...
	void TryDeleting();
	bool TryDetaching();

	// only accessible by [Ref CreateRef(...)] function
	Ref(MyBlock* block)
		: mBlock(block) {}

	template <typename Var, typename Fn, typename ...ARGS>
	friend Ref<Var> CreateRef(Fn&&, ARGS&&...);
};

template <typename T, typename Fn, typename ...ARGS>
Ref<T> CreateRef(Fn&& deleter, ARGS&&... args)
{
	typename Ref<T>::MyBlock* block = new typename Ref<T>::MyBlock(std::forward<ARGS>(args)...);

	block->mDeleter = std::move(deleter);
	block->mRefCount.store(1);

	return block;
}

template<typename T>
inline void Ref<T>::SetValue(const T& handle)
{
	// Ref must already exist in order for this to work
	_STL_ASSERT(mBlock, "Core::Ref must already exist for Ref::SetValue(T&&) to work!");

	mBlock->DeleteObject();

	mBlock->mHandle = handle;
}

template <typename T>
void Ref<T>::ReplaceValue(const T& handle)
{
	// Ref must already exist in order for this to work
	_STL_ASSERT(mBlock, "Core::Ref must already exist for Ref::ReplaceValue(const T&) to work!");

	bool Success = TryDetaching();

	if (!Success)
	{
		// Another owner of the control block exists so creating a new control block
		mBlock = new MyBlock(mBlock->mDeleter, handle);
		return;
	}

	// No owner of the control block exists which means we can recycle it for our new value
	std::_Construct_in_place(*mBlock, mBlock->mDeleter, handle);
}

template<typename T>
void Ref<T>::Reset()
{
	if (mBlock)
	{
		mBlock->Dec();
		TryDeleting();
		mBlock = nullptr;
	}
}

template<typename T>
void Ref<T>::TryDeleting()
{
	if (mBlock->mRefCount.load() == 0)
	{
		mBlock->DeleteObject();
		delete mBlock;
	}
}

template<typename T>
bool Ref<T>::TryDetaching()
{
	mBlock->Dec();

	if (mBlock->mRefCount.load() == 0)
	{
		mBlock->DeleteObject();
		return true;
	}

	return false;
}

template <typename T>
Ref<T>& Ref<T>::operator=(const Ref& Other)
{
	Reset();

	mBlock = Other.mBlock;

	if (mBlock)
		mBlock->Inc();

	return *this;
}

VK_CORE_END
VK_END
