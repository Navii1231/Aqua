#include "Core/Aqpch.h"
#include "Execution/ParserConfig.h"

AQUA_BEGIN
EXEC_BEGIN

std::unordered_map<TypeName, std::string> sNumericTypesToStrings =
{
	{ TypeName::eInvalid, "Invalid" },
	{ TypeName::eInt8,    "int8"    },
	{ TypeName::eInt16,   "int16"   },
	{ TypeName::eInt32,   "int32"   },
	{ TypeName::eInt64,   "int64"   },
	{ TypeName::eFloat8,  "fp8"  },
	{ TypeName::eFloat16, "fp16" },
	{ TypeName::eFloat32, "fp32" },
	{ TypeName::eFloat64, "fp64" },
	{ TypeName::eUint8, "u8" },
	{ TypeName::eUint16, "u16" },
	{ TypeName::eUint32, "u32" },
	{ TypeName::eUint64, "u64" },
	{ TypeName::eVoid, "void" },
	{ TypeName::eBoolean, "bool" },
};

std::unordered_map<std::string, TypeName> sStringToNumericTypes =
{
	{ "Invalid", TypeName::eInvalid },
	{ "int8",    TypeName::eInt8    },
	{ "int16",   TypeName::eInt16   },
	{ "int32",   TypeName::eInt32   },
	{ "int64",   TypeName::eInt64   },
	{ "fp8",  TypeName::eFloat8  },
	{ "fp16", TypeName::eFloat16 },
	{ "fp32", TypeName::eFloat32 },
	{ "fp64", TypeName::eFloat64 },
	{ "u8", TypeName::eUint8 },
	{ "u16", TypeName::eUint16 },
	{ "u32", TypeName::eUint32 },
	{ "u64", TypeName::eUint64 },
	{ "void", TypeName::eVoid },
	{ "bool", TypeName::eBoolean},
};

EXEC_END
AQUA_END

std::expected<AQUA_NAMESPACE::EXEC_NAMESPACE::TypeName, bool> AQUA_NAMESPACE::EXEC_NAMESPACE::GetTypename(const std::string& type)
{
	if (sStringToNumericTypes.find(type) == sStringToNumericTypes.end())
		return std::unexpected(false);

	return sStringToNumericTypes.at(type);
}

std::expected<std::string, bool> AQUA_NAMESPACE::EXEC_NAMESPACE::GetNumericString(TypeName type)
{
	if (sNumericTypesToStrings.find(type) == sNumericTypesToStrings.end())
		return std::unexpected(false);

	return sNumericTypesToStrings.at(type);
}

std::expected<std::string, bool> AQUA_NAMESPACE::EXEC_NAMESPACE::ConvertToGLSLString(TypeName type)
{
	switch (type)
	{
	case TypeName::eInvalid:
		return "invalid";
	case TypeName::eInt8:
	case TypeName::eInt16:
	case TypeName::eInt32:
	case TypeName::eInt64:
		return "int";
	case TypeName::eFloat8:
	case TypeName::eFloat16:
	case TypeName::eFloat32:
	case TypeName::eFloat64:
		return "float";
	case TypeName::eUint8:
	case TypeName::eUint16:
	case TypeName::eUint32:
	case TypeName::eUint64:
		return "uint";
	case TypeName::eBoolean:
		return "bool";
	case TypeName::eVoid:
		return "void";
	default:
		return std::unexpected(false);
	};
}
