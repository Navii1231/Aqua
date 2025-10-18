#pragma once
#include "FactoryConfig.h"

AQUA_BEGIN

// TODO: #volatile to #future_changes
// For now, it's working more like a #state_machine
class VertexFactory
{
public:
	VertexFactory() = default;
	virtual ~VertexFactory() = default;

	vkLib::VertexInputDesc GetVertexInputStreamInfo() const { return mVertexInputStream; }

	void SetResourcePool(vkLib::ResourcePool pool) { mResourcePool = pool; }

	// operations at binding level
	AQUA_API void Reset();
	AQUA_API void SetVertexBindings(const VertexBindingMap& map);

	// we need property trick here as well
	template <typename ...Properties>
	void SetIndexProperties(Properties&&... props)
	{
		(SetIndexProperty(std::forward<Properties>(props)), ...);
	}

	template <typename ...Properties>
	void SetVertexProperties(const std::string& name, Properties&& ... props)
	{
		(SetVertexProperty(name, std::forward<Properties>(props)), ...);
	}

	template <typename ...Properties>
	void SetAllVertexProperties(Properties&& ...props)
	{
		for (auto& [name, binding] : mVertexBindingProperties)
		{
			(SetVertexProperty(name, std::forward<Properties>(props)), ...);
		}
	}

	// State validation and vertex buffer generation
	AQUA_API std::expected<bool, VertexError> Validate();
	AQUA_API void ClearBuffers();

	template<typename Fn>
	void TraverseBuffers(Fn&& fn);

	AQUA_API void ReserveVertices(uint32_t count);
	AQUA_API void ReserveIndices(uint32_t count);

	// buffer access
	vkLib::GenericBuffer operator[](const std::string& name) const { return mVertexResources.at(name); }
	vkLib::GenericBuffer operator[](const std::string& name) { return mVertexResources[name]; }

	AQUA_API bool BindingExists(const std::string& name) const;

	vkLib::GenericBuffer GetIndexBuffer() const { return mIndexBuffer; }
	const VertexBindingMap& GetVertexBindings() const { return mVertexBindings; }
									    
	// static function to generate vertex stream info using vertex binding map
	AQUA_API static vkLib::VertexInputDesc GenerateVertexInputStreamInfo(const VertexBindingMap& bindings);
	// TODO: incomplete function
	AQUA_API static VertexBindingMap GenerateVertexBindingInfo(const vkLib::VertexInputDesc& vkDesc);
	AQUA_API static bool CheckVertexBindings(VertexBindingMap vertexBindings);

	AQUA_API static void CopyVertexBuffer(vk::CommandBuffer cmd, vkLib::GenericBuffer dst, vkLib::GenericBuffer src);

private:
	VertexBindingMap mVertexBindings;
	VertexBindingPropertiesMap mVertexBindingProperties;
	VertexResourceMap mVertexResources;

	vkLib::GenericBuffer mIndexBuffer;

	vk::BufferUsageFlags mIndexUsage = vk::BufferUsageFlagBits::eIndexBuffer;
	vk::MemoryPropertyFlags mIndexMemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;

	// TODO: For now, this is fixed all across the application
	vk::IndexType mIndexType = vk::IndexType::eUint32;
	vkLib::VertexInputDesc mVertexInputStream;

	vk::CommandBuffer mCommandBuffer;
	vkLib::ResourcePool mResourcePool;

	constexpr static uint32_t sDefaultVertexCount = 100;
	constexpr static uint32_t sDefaultIndexCount = 100;

private:
	void Initialize();
	bool CheckForUniqueNames();
	bool VerifyAllBindingExists();

	void SetVertexProperty(const std::string& name, vk::BufferUsageFlags flags)
	{ mVertexBindingProperties[name].Usage = flags; }
	
	void SetVertexProperty(const std::string& name, vk::MemoryPropertyFlags flags)
	{ mVertexBindingProperties[name].MemProps = flags; }

	void SetIndexProperty(vk::BufferUsageFlags flags)
	{ mIndexUsage = flags; }

	void SetIndexProperty(vk::MemoryPropertyFlags flags)
	{ mIndexMemProps = flags; }

};

template<typename Fn>
void AQUA_NAMESPACE::VertexFactory::TraverseBuffers(Fn&& fn)
{
	for (const auto&[idx, attribute] : mVertexBindings)
	{
		fn(static_cast<uint32_t>(idx), attribute.Name, mVertexResources[attribute.Name]);
	}
}

AQUA_END
