#pragma once
#include "../../Core/AqCore.h"
#include "../../Material/MaterialConfig.h"
#include "../../Material/MaterialBuilder.h"

AQUA_BEGIN

#define TEMPLATE_POINT            "Points"
#define TEMPLATE_LINE             "Lines"
#define TEMPLATE_PBR              "PBR"
#define TEMPLATE_BlinnPhong       "BlinnPhong"
#define TEMPLATE_Diffuse          "Diffuse"
#define TEMPLATE_Glossy           "Glossy"
#define TEMPLATE_Wireframe        "Wireframe"

// for effective material caching
struct MaterialTemplate
{
	std::string Name;
	
	MAT_NAMESPACE::Material MatOp{EXEC_NAMESPACE::NodeID(-1)};
	MAT_NAMESPACE::ShaderParameterSet ParameterSet;
	uint32_t ParStride;
	DeferGFXMaterialCreateInfo GFXCreateInfo;
	DeferCmpMaterialCreateInfo CmpCreateInfo;
	MAT_NAMESPACE::Platform RendererPlatform = MAT_NAMESPACE::Platform::eLightingRaster;
};

AQUA_END
