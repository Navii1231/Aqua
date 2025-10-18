#pragma once
// VulkanEngine library dependency...
#include "Device/Context.h"
#include "Aqpch.h"

/*
* Aqua has a dependency on Assimp dll
* Particularly the Assimp Importer and Exporter to
* load and save 3D models. It must be respected at the final build
*/

#define AQUA_NAMESPACE    Aqua
#define AQUA_BEGIN        namespace AQUA_NAMESPACE {
#define AQUA_END          }

#define EXEC_NAMESPACE    Exec
#define EXEC_BEGIN        namespace EXEC_NAMESPACE {
#define EXEC_END          }

#define MAT_NAMESPACE     MatCore
#define MAT_BEGIN         namespace MAT_NAMESPACE {
#define MAT_END           }

// todo: will employ this namespace after we add the internet stuff
#define ASYNC_NAMESPACE   ASync
#define ASYNC_BEGIN       namespace ASYNC {
#define ASYNC_END         }

#define MAKE_SHARED(type, ...)   std::make_shared<type>(__VA_ARGS__)

#if AQUA_BUILD_STATIC
#define AQUA_API
#elif AQUA_BUILD_DLL
#define AQUA_API     __declspec(dllexport)
#else
#define AQUA_API     __declspec(dllimport)
#endif
