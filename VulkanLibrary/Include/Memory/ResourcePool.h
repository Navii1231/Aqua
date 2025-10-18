#pragma once
#include "MemoryConfig.h"
#include "GenericBuffer.h"
#include "Image.h"
#include "ImageView.h"
#include "../Process/Commands.h"

VK_BEGIN

class ResourcePool
{
public:
	ResourcePool() = default;

	template <typename T, typename ...Properties>
	Buffer<T> CreateBuffer(Properties&&... properties) const;

	template <typename ...Properties>
	GenericBuffer CreateGenericBuffer(Properties&&... properties) const;

	VKLIB_API Image CreateImage(const ImageCreateInfo& info) const;

	VKLIB_API Core::Ref<vk::Sampler> CreateSampler(const SamplerInfo& samplerInfo, SamplerCache cache = {}) const;

	VKLIB_API SamplerCache CreateSamplerCache() const;

	std::shared_ptr<const WorkingClass> GetQueueManager() const { return mWorkingClass; }

	explicit operator bool() const { return static_cast<bool>(mDevice); }

private:
	Core::Ref<vk::Device> mDevice;
	PhysicalDevice mPhysicalDevice;

	CommandPools mBufferCommandPools;
	CommandPools mImageCommandPools;

	std::shared_ptr<const WorkingClass> mWorkingClass;

	friend class Context;
};

Image Clone(Context ctx, const Image& image);
ImageView Clone(Context ctx, const ImageView& view);

template <typename T, typename ...Properties>
VK_NAMESPACE::Buffer<T> VK_NAMESPACE::ResourcePool::CreateBuffer(Properties&&... props) const
{
	auto Device = mDevice;

	Core::BufferResource Chunk;
	Core::BufferConfig Config{};

	Config.ElemCount = 0;
	Config.LogicalDevice = *Device;
	Config.PhysicalDevice = mPhysicalDevice.Handle;
	Config.TypeSize = sizeof(T);
	(Config.SetProperty(std::forward<Properties>(props)),...);

	// Creating an empty buffer
	Chunk.BufferHandles = Core::CreateRef<Core::Buffer>([Device](Core::Buffer& buffer)
	{
		if (buffer.Handle)
		{
			Device->destroyBuffer(buffer.Handle);
			Device->freeMemory(buffer.Memory);
		}
	}, vkLib::Core::Buffer());

	Chunk.Device = Device;
	Chunk.BufferHandles->Config = Config;
	Chunk.BufferHandles->Config.ElemCount = 0;

	Buffer<T> buffer(std::move(Chunk));
	buffer.mWorkingClass = GetQueueManager();
	buffer.mCommandPools = mBufferCommandPools;
	buffer.mChunk.RscPool = std::make_shared<ResourcePool>(*this);

	buffer.Reserve(Config.ElemCount == 0 ? 1 : Config.ElemCount);
	buffer.Resize(Config.ElemCount);

	return buffer;
}

template <typename... Properties>
VK_NAMESPACE::GenericBuffer VK_NAMESPACE::ResourcePool::CreateGenericBuffer(Properties&&... props) const
{
	return CreateBuffer<GenericBuffer::Type>(std::forward<Properties>(props)...);
}

VK_END
