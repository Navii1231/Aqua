#pragma once
#include "../../Core/AqCore.h"
#include "Renderable.h"

AQUA_BEGIN

// TODO: we could either make the renderable info a blob data structure
// or we could make it a generic template
template <typename Info>
class RenderableBuilder
{
public:
	using MyInfo = Info;
	using CopyFn = std::function<void(vkLib::GenericBuffer&, const MyInfo&, RenderableInfo&)>;
	using CopyFnMap = std::unordered_map<std::string, CopyFn>;

	struct BufferProperties
	{
		CopyFn CpyFn;
		vk::BufferUsageFlags Usage = vk::BufferUsageFlagBits::eStorageBuffer;
		vk::MemoryPropertyFlags MemProps = vk::MemoryPropertyFlagBits::eHostCoherent;
	};

	using BufPropMap = std::unordered_map<std::string, BufferProperties>;

public:
	RenderableBuilder() = default;
	~RenderableBuilder() = default;

	void SetRscPool(vkLib::ResourcePool pool) { mResourcePool = pool; }

	template <typename ...Properties>
	void SetVertexProperties(const std::string& name, Properties&&... props);

	template <typename ...Properties>
	void SetIndexProperties(Properties&&... props);

	Renderable CreateRenderable(const MyInfo& renderableInfo);

private:
	vkLib::ResourcePool mResourcePool;

	BufPropMap mVertexProperties;
	BufferProperties mIndexProperties;

private:
	void SetVertexProperty(const std::string& name, vk::BufferUsageFlags flags) 
	{ mVertexProperties[name].Usage = flags; }

	void SetVertexProperty(const std::string& name, vk::MemoryAllocateFlags flags)
	{ mVertexProperties[name].MemProps = flags; }

	void SetVertexProperty(const std::string& name, const CopyFn& fn)
	{ mVertexProperties[name].CpyFn = fn; }

	void SetIndexProperty(vk::BufferUsageFlags flags)
	{ mIndexProperties.Usage = flags; }

	void SetIndexProperty(vk::MemoryAllocateFlags flags)
	{ mIndexProperties.MemProps = flags; }

	void SetIndexProperty(const CopyFn& fn)
	{ mIndexProperties.CpyFn = fn; }
};

AQUA_END

template <typename Info>
template <typename ...Properties>
void AQUA_NAMESPACE::RenderableBuilder<Info>::SetVertexProperties(const std::string& name, Properties&&... props)
{
	(SetVertexProperty(name, std::forward<Properties>(props)), ...);
}

template <typename Info>
template <typename ...Properties>
void AQUA_NAMESPACE::RenderableBuilder<Info>::SetIndexProperties(Properties&&... props)
{
	(SetIndexProperty(std::forward<Properties>(props)), ...);
}

template <typename Info>
AQUA_NAMESPACE::Renderable AQUA_NAMESPACE::RenderableBuilder<Info>::CreateRenderable(const Info& renderableInfo)
{
	Renderable renderable;

	renderable.mVertexBuffers.reserve(mVertexProperties.size());

	for (auto& [name, prop] : mVertexProperties)
	{
		renderable.mVertexBuffers[name] = mResourcePool.CreateGenericBuffer(
			prop.Usage | vk::BufferUsageFlagBits::eVertexBuffer, prop.MemProps);

		renderable.mVertexBuffers[name].Reserve(64);

		prop.CpyFn(renderable.mVertexBuffers[name], renderableInfo, renderable.Info);
	}

	renderable.mIndexBuffer = mResourcePool.CreateGenericBuffer(
		mIndexProperties.Usage | vk::BufferUsageFlagBits::eIndexBuffer, mIndexProperties.MemProps);

	renderable.mIndexBuffer.Reserve(64);

	mIndexProperties.CpyFn(renderable.mIndexBuffer, renderableInfo, renderable.Info);

	return renderable;
}

