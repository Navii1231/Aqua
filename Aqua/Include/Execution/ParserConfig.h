#pragma once
#include "GraphConfig.h"
#include "../Utils/Lexer.h"

AQUA_BEGIN
EXEC_BEGIN

enum class NodeType
{
	eInvalid       = 0,
	eFunction      = 1,
};

enum class TargetLanguage
{
	eGLSL          = 1,
	eHLSL          = 2,
};

enum class TypeName
{
	eInvalid       = 0,
	// numerics
	eInt8          = 1,
	eInt16         = 2,
	eInt32         = 3,
	eInt64         = 4,
	eFloat8        = 5,
	eFloat16       = 6,
	eFloat32       = 7,
	eFloat64       = 8,
	eUint8         = 9,
	eUint16        = 10,
	eUint32        = 11,
	eUint64        = 12,
	// others
	eBoolean       = 13,
	eVoid          = 14
};

enum class ConstructQualifier
{
	eNone                         = 0,
	eGlobalVar                    = 1,
	eStructDefinition             = 2,
	eFunctionDefinition           = 3,
	eRscDecl                      = 4,
	eComputeParDecl               = 5,
	eInvalid                      = 6,
};

enum class FunctionNodeType
{
	eInvalid                   = 0,
	eOpTransform               = 1,
};

struct FunctionNode
{
	// name
	std::string Name;
	// parameters section
	std::unordered_map<std::string, TypeName> Parameters;

	// aqua special parameters
	std::string AquaDependencyName = ""; // only for operations
	std::string AquaParsName = ""; // only for parameters
	std::string AquaInputName = ""; // only for inputs

	int AquaParsCount = 0;
	int AquaInputCount = 0;

	// return section
	TypeName ReturnType = TypeName::eInvalid;

	// body section, GLSL code with aqua parameters as placeholders
	std::string Body;

	FunctionNodeType Type = FunctionNodeType::eInvalid;
};

struct StructDefNode
{
	std::string Typename{};
	std::string Body{};
};

struct PushConstDecl
{
	TypeName Type = TypeName::eInvalid;
	uint32_t Idx = -1;
	std::string FieldName = "";
	std::vector<uint8_t> Values;
	vk::ShaderStageFlags ShaderType = vk::ShaderStageFlagBits::eCompute;

	constexpr static char sConstName[] = "KernelConsts";
};

// two types of kernels: operation kernels and dependency kernels
// later we can put these kernel extractions together to form the final kernel code
// the only type of kernel that will finally run on the GPU is the operation kernel
struct KernelExtractions
{
	uint32_t GLSLVersion = 440;

	glm::uvec3 InvocationCount = { 1, 1, 1 };
	glm::uvec3 WorkGroupSize = { 16, 16, 1 };

	GraphRscMap Rscs;
	std::vector<FunctionNode> Functions;
	std::vector<StructDefNode> StructDefs;
	std::unordered_map<std::string, std::string> GlobalVariables;
	std::vector<PushConstDecl> KernelConsts;

	FunctionNode EvaluateFunction{};

	FunctionNodeType NodeType = FunctionNodeType::eInvalid;
};

std::expected<TypeName, bool> GetTypename(const std::string& type);
std::expected<std::string, bool> GetNumericString(TypeName type);
std::expected<std::string, bool> ConvertToGLSLString(TypeName type);

extern std::unordered_map<TypeName, std::string> sNumericTypesToStrings;
extern std::unordered_map<std::string, TypeName> sStringsToNumericTypes;

struct RscTypeHandler
{
	std::string Typename = "";
	std::function<void(Lexer&, GraphRsc&)> Extract;
};

EXEC_END
AQUA_END
