#pragma once
#include "ProcessConfig.h"
#include "Commands.h"

VK_BEGIN

template<typename T>
class Buffer;

class RecordableResource
{
public:
	RecordableResource(const std::shared_ptr<WorkingClass>& queueManager, const CommandPools& commandPool)
		: mWorkingClass(queueManager), mCommandPools(commandPool) {}

	virtual void BeginCommands(vk::CommandBuffer commandBuffer) const { DefaultBegin(commandBuffer); }
	virtual void EndCommands() const { DefaultEnd(); }

	template<typename Fn>
	void InvokeOneTimeProcess(uint32_t index, Fn&& fn) const;

	template <typename Fn>
	void InvokeProcess(uint32_t index, Fn&& fn) const;

	virtual ~RecordableResource() = default;

protected:
	mutable vk::CommandBuffer mWorkingCommandBuffer = nullptr;

	WorkingClassRef mWorkingClass;
	CommandPools mCommandPools;

	RecordableResource() = default;

	void DefaultBegin(vk::CommandBuffer commandBuffer) const
	{
		_STL_ASSERT(mWorkingCommandBuffer == nullptr, "You must call EndCommands before calling "
			"BeginCommands again");

		mWorkingCommandBuffer = commandBuffer;
	}

	void DefaultEnd() const
	{
		_STL_ASSERT(mWorkingCommandBuffer, "EndCommands can't find a suitable command buffer\n"
			"Did you forget to call BeginCommands?");

		mWorkingCommandBuffer = nullptr;
	}

	template <typename _Rsc>
	friend _Rsc Clone(Context, const _Rsc&);

	template <typename T1, typename T2>
	friend Buffer<T1> ReinterpretCast(Buffer<T2>);
};

template <typename Fn>
void VK_NAMESPACE::RecordableResource::InvokeProcess(uint32_t index, Fn&& fn) const
{
	auto cmdBufAlloc = mCommandPools[index];
	std::scoped_lock locker(cmdBufAlloc);

	auto executor = mWorkingClass->FetchWorker(index);
	auto cmdBuf = cmdBufAlloc.Allocate();

	fn(cmdBuf);

	// maybe we've to wrap this guy in too
	executor.Enqueue(cmdBuf);
	executor.WaitIdle();

	cmdBufAlloc.Free(cmdBuf);
}

template<typename Fn>
void VK_NAMESPACE::RecordableResource::InvokeOneTimeProcess(uint32_t index, Fn&& fn) const
{
	auto cmdBufAlloc = mCommandPools[index];

	std::scoped_lock locker(cmdBufAlloc);

	auto executor = mWorkingClass->FetchWorker(index);
	auto cmdBuf = cmdBufAlloc.BeginOneTimeCommands();

	// Recording done inside this function
	fn(cmdBuf);

	cmdBufAlloc.EndOneTimeCommands(cmdBuf, executor);
}

VK_END
