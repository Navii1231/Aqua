#include "Core/Aqpch.h"
#include "DeferredRenderer/Renderable/VertexFactory.h"

void AQUA_NAMESPACE::VertexFactory::Reset()
{
	mVertexBindings.clear();
	mVertexResources.clear();
	mVertexBindingProperties.clear();
}

void AQUA_NAMESPACE::VertexFactory::SetVertexBindings(const VertexBindingMap& map)
{
	mVertexBindings = map;

	Initialize();
}

std::expected<bool, AQUA_NAMESPACE::VertexError> AQUA_NAMESPACE::VertexFactory::Validate()
{
	// Checking if the locations do not repeat
	// Checking if all the properties are coherent in all maps

	if (!CheckVertexBindings(mVertexBindings))
		return std::unexpected(VertexError::eDuplicateVertexLocations);

	if (!CheckForUniqueNames())
		return std::unexpected(VertexError::eDuplicateBindingNames);

	if (!VerifyAllBindingExists())
		return std::unexpected(VertexError::eBindingDoesntExist);

	mVertexInputStream = GenerateVertexInputStreamInfo(mVertexBindings);

	TraverseBuffers([this](uint32_t idx, const std::string& name, vkLib::GenericBuffer& buffer)
		{
			buffer = mResourcePool.CreateGenericBuffer(mVertexBindingProperties[name].Usage |
				vk::BufferUsageFlagBits::eVertexBuffer, mVertexBindingProperties[name].MemProps);
		});

	mIndexBuffer = mResourcePool.CreateGenericBuffer(mIndexUsage | vk::BufferUsageFlagBits::eIndexBuffer, mIndexMemProps);

	ReserveVertices(sDefaultVertexCount);
	ReserveIndices(sDefaultIndexCount);

	return true;
}

void AQUA_NAMESPACE::VertexFactory::ClearBuffers()
{
	TraverseBuffers([](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
		{
			buffer.Clear();
		});

	if(mIndexBuffer)
		mIndexBuffer.Clear();
}

void AQUA_NAMESPACE::VertexFactory::ReserveVertices(uint32_t count)
{
	TraverseBuffers([this, count](uint32_t idx, const std::string& name, vkLib::GenericBuffer buffer)
		{
			uint32_t stride = mVertexInputStream.Bindings[idx].stride;
			buffer.Reserve(count * stride);
		});
}

void AQUA_NAMESPACE::VertexFactory::ReserveIndices(uint32_t count)
{
	mIndexBuffer.Reserve(count * sizeof(uint32_t));
}

bool AQUA_NAMESPACE::VertexFactory::BindingExists(const std::string& name) const
{
	if (mVertexBindingProperties.find(name) == mVertexBindingProperties.end())
		return false;

	return true;
}

vkLib::VertexInputDesc AQUA_NAMESPACE::VertexFactory::GenerateVertexInputStreamInfo(const VertexBindingMap& bindings)
{
	vkLib::VertexInputDesc desc{};
	desc.Bindings.reserve(bindings.size());

	for (const auto&[idx, binding] : bindings)
	{
		uint32_t stride = 0;

		for (uint32_t attribIdx = 0; attribIdx < binding.Attributes.size(); attribIdx++)
		{
			const auto& [format, layout, whatever, bytes] = 
				ConvertIntoTagAttributes(binding.Attributes[attribIdx].Format);

			desc.Attributes.emplace_back(binding.Attributes[attribIdx].Location, idx, format, stride);

			stride += bytes;
		}

		desc.Bindings.emplace_back(idx, stride, binding.InputRate);
	}

	return desc;
}

AQUA_NAMESPACE::VertexBindingMap AQUA_NAMESPACE::VertexFactory::
	GenerateVertexBindingInfo(const vkLib::VertexInputDesc& vkDesc)
{
	VertexBindingMap bindings{};

	for (const auto& binding : vkDesc.Bindings)
	{
		bindings[binding.binding].Name = std::to_string(binding.binding);
		bindings[binding.binding].SetInputRate(binding.inputRate);

	}
	for (const auto& attribute : vkDesc.Attributes)
	{
		VertexBinding vertexBinding{};
	}

	return bindings;
}

bool AQUA_NAMESPACE::VertexFactory::CheckVertexBindings(VertexBindingMap vertexBindings)
{
	std::unordered_set<uint32_t> locations{};

	for (const auto& [idx, attributes] : vertexBindings)
	{
		for (const auto& attribute : attributes.Attributes)
		{
			if (locations.find(attribute.Location) != locations.end())
				return false;

			locations.insert(attribute.Location);
		}
	}

	return true;
}

void AQUA_NAMESPACE::VertexFactory::CopyVertexBuffer(vk::CommandBuffer cmd, vkLib::GenericBuffer dst, vkLib::GenericBuffer src)
{
	vk::BufferCopy copyRegion{};
	copyRegion.setDstOffset(dst.GetSize());
	copyRegion.setSrcOffset(0);
	copyRegion.setSize(src.GetSize());

	if (copyRegion.size == 0)
		return;

	dst.Resize(copyRegion.dstOffset + copyRegion.size);

	// using GPU to copy the memory...
	vkLib::RecordCopyBufferRegions(cmd, dst, src, { copyRegion });
}

void AQUA_NAMESPACE::VertexFactory::Initialize()
{
	// Initialize everything...

	for (const auto& [idx, binding] : mVertexBindings)
	{
		mVertexResources[binding.Name] = {};
		mVertexBindingProperties[binding.Name].BindingIdx = idx;
	}
}

bool AQUA_NAMESPACE::VertexFactory::CheckForUniqueNames()
{
	std::unordered_set<std::string> uniqueNames;

	for (const auto& [idx, binding] : mVertexBindings)
	{
		if (uniqueNames.find(binding.Name) != uniqueNames.end())
			return false;

		uniqueNames.insert(binding.Name);
	}

	return true;
}

bool AQUA_NAMESPACE::VertexFactory::VerifyAllBindingExists()
{
	for (const auto& [name, props] : mVertexBindingProperties)
	{
		// checking if the binding exists, and if exists then the names match with each other
		auto found = mVertexBindings.find(props.BindingIdx);

		if (found == mVertexBindings.end())
			return false;

		if (found->second.Name != name)
			return false;
	}

	return true;
}

