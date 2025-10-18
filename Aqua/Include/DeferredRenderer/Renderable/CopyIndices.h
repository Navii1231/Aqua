#pragma once
#include "Renderable.h"

AQUA_BEGIN

struct CopyIdxPipeline : public vkLib::ComputePipeline
{
    CopyIdxPipeline();

    CopyIdxPipeline(vkLib::PShader copyComp)
    { this->SetShader(copyComp); }

    AQUA_API void UpdateDescriptors(vkLib::GenericBuffer dst, vkLib::GenericBuffer src);

    AQUA_API void operator()(vk::CommandBuffer commandBuffer, vkLib::GenericBuffer dst, vkLib::GenericBuffer src, uint32_t vertexCount);

    static std::string sDefaultImplementation;
};

AQUA_END
