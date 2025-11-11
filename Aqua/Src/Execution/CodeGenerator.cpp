#include "Core/Aqpch.h"
#include "Execution/CodeGenerator.h"
#include "DeferredRenderer/Renderable/FactoryConfig.h"

std::string AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLCodeGenerator::Generate() const
{
	// now we piece together the final shader code
	std::stringstream finalCode{};

	finalCode << "#version " << mExts.GLSLVersion << "\n";

	// the work group size string
	finalCode << "layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;\n";

	// push/kernel constants definition
	
	if (!mExts.KernelConsts.empty())
	{
		finalCode << "layout (push_constant) uniform KernelConsts\n{\n";

		for (const auto& kernelConst : mExts.KernelConsts)
		{
			finalCode << "\t" << *ConvertToGLSLString(kernelConst.Type) << " " << kernelConst.FieldName << ";\n";
		}

		finalCode << "\n};\n\n";
	}

	// writing the global variables
	for (const auto& [name, value] : mExts.GlobalVariables)
	{
		finalCode << name << " " << value << ";\n";
	}

	finalCode << "\n";

	for (const auto& structDef : mExts.StructDefs)
	{
		finalCode << "struct " << structDef.Typename;
		finalCode << "\n{";
		finalCode << structDef.Body;
		finalCode << "\n};\n\n";
	}

	// writing the shared buffers
	for (const auto& [location, rsc] : mExts.Rscs)
	{
		InsertRscDecl(finalCode, rsc);
		finalCode << "\n\n";
	}

	// we can ignore the push constants for now
	// writing the functions
	for (const auto& func : mExts.Functions)
	{
		finalCode << *ConvertToGLSLString(func.ReturnType) << " " << func.Name << "(";

		bool firstParam = true;
		for (const auto& [paramName, type] : func.Parameters)
		{
			if (!firstParam)
				finalCode << ", ";
			finalCode << *ConvertToGLSLString(type) << " " << paramName;
			firstParam = false;
		}

		finalCode << ")\n{";
		finalCode << func.Body + "\n";
		finalCode << "}\n\n";
	}

	// writing the main function
	finalCode << "void main()\n{";

	// we'll visit this piece back later...
	finalCode << mExts.EvaluateFunction.Body << "\n";
	finalCode << "}\n";

	return finalCode.str();
}

void AQUA_NAMESPACE::EXEC_NAMESPACE::GLSLCodeGenerator::InsertRscDecl(std::stringstream& stream, const GraphRsc& rsc) const
{
	switch (rsc.Type)
	{
		case vk::DescriptorType::eSampledImage:
			stream << "layout(set = " << rsc.Location.SetIndex << ", binding = " << rsc.Location.Binding << ", " << GetImageAttachmentInfoString(rsc.Format) << ") uniform " << rsc.Typename << " " << rsc.Name << "\n";
			break;
		case vk::DescriptorType::eStorageImage:
			stream << "layout(set = " << rsc.Location.SetIndex << ", binding = " << rsc.Location.Binding << ", " << GetImageAttachmentInfoString(rsc.Format) << ") uniform " << rsc.Typename << " " << rsc.Name << "\n";
			break;
		case vk::DescriptorType::eUniformBuffer:
			stream << "layout(std140, set = " << rsc.Location.SetIndex << ", binding = " << rsc.Location.Binding << ") uniform " << "PCI_UniformBuffer_" << rsc.Name << "\n";
			stream << "{\n\t" << rsc.Typename << " " << rsc.Name << ";\n};";
			break;
		case vk::DescriptorType::eStorageBuffer:
			stream << "layout(std430, set = " << rsc.Location.SetIndex << ", binding = " << rsc.Location.Binding << ") buffer " << "PCI_SharedBuffer_" << rsc.Name << "\n";
			stream << "{\n\t" << rsc.Typename << " " << rsc.Name << "[];\n};";
			break;
		default:
			break;
	}
}

