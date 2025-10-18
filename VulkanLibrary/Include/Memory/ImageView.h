#pragma once
#include "MemoryConfig.h"
#include "Image.h"

VK_BEGIN

class ImageView
{
public:
	ImageView() = default;
	~ImageView() = default;

	const ImageViewData& GetImageData() const { return *mData; }

	vk::ImageView GetNativeHandle() const { return mData->View; }
	glm::uvec2 GetSize() const { return mData->Size; }

	// Not sure if it's a good interface, but it makes sense for 
	// you can't interact with image view directly
	Image& operator*() { return mParent; }
	const Image& operator*() const { return mParent; }

	Image* operator->() { return &mParent; }
	const Image* operator->() const { return &mParent; }

	Image GetImage() const { return mParent; }

	explicit operator bool() const { return static_cast<bool>(mParent); }
	
private:
	Core::Ref<ImageViewData> mData;
	Image mParent;
	friend class Image;

	template <typename _Rsc>
	friend _Rsc Clone(Context, const _Rsc&);
};

VK_END
