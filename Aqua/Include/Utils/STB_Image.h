#pragma once
#include "../Core/AqCore.h"
#include "stb/stb_image.h"

AQUA_BEGIN

class StbImage
{
public:
	struct Buffer
	{
		vkLib::GenericBuffer Buf;
		int Width = 0;
		int Height = 0;
		int Channels = 0;
		vk::Format Format{};

		vkLib::ResourcePool RscPool{};
	};

public:
	StbImage() = default;
	StbImage(vkLib::ResourcePool pool) : mResourcePool(pool) {}

	void SetResourcePool(vkLib::ResourcePool manager) { mResourcePool = manager; }

	inline std::expected<Buffer, bool> LoadImage(const std::string& filepath);
	inline std::expected<Buffer, bool> LoadImageF(const std::string& filepath);

	inline static vkLib::Image Convert(const Buffer& image_buffer, vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eSampled);

private:
	vkLib::ResourcePool mResourcePool;
};

std::expected<StbImage::Buffer, bool> StbImage::LoadImage(const std::string& filepath)
{
	Buffer image_buffer;

	stbi_uc* result = stbi_load(filepath.c_str(), &image_buffer.Width, &image_buffer.Height, &image_buffer.Channels, 4);

	if (!result)
		return std::unexpected(false);

	std::vector<int> pixels(result, result + image_buffer.Width * image_buffer.Height * 4);

	image_buffer.Buf = mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer);
	image_buffer.Buf << pixels;

	stbi_image_free(result);

	image_buffer.Format = vk::Format::eR8G8B8A8Unorm;
	image_buffer.RscPool = mResourcePool;

	return image_buffer;
}

std::expected<AQUA_NAMESPACE::StbImage::Buffer, bool> StbImage::LoadImageF(const std::string& filepath)
{
	Buffer image_buffer;

	float* result = stbi_loadf(filepath.c_str(), &image_buffer.Width, &image_buffer.Height, &image_buffer.Channels, 4);

	if (!result)
		return std::unexpected(false);

	std::vector<float> pixels(result, result + image_buffer.Width * image_buffer.Height * 4);

	image_buffer.Buf = mResourcePool.CreateGenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer);
	image_buffer.Buf << pixels;

	stbi_image_free(result);

	image_buffer.Format = vk::Format::eR8G8B8A8Unorm;
	image_buffer.RscPool = mResourcePool;

	return image_buffer;
}

vkLib::Image StbImage::Convert(const Buffer& image_buffer, vk::ImageUsageFlags flags)
{
	vkLib::ImageCreateInfo createInfo{};
	createInfo.Format = image_buffer.Format;
	createInfo.MemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
	createInfo.Tiling = vk::ImageTiling::eOptimal;
	createInfo.Usage = flags;

	createInfo.Extent = vk::Extent3D(image_buffer.Width, image_buffer.Height, 1);

	vkLib::Image image = image_buffer.RscPool.CreateImage(createInfo);

	image.TransitionLayout(vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eComputeShader);
	image.CopyBufferData(image_buffer.Buf);

	return image;
}

AQUA_END
