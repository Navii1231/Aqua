#if 0
#if _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <Windows.h>
#else
#error This platform is not supported yet!
#endif
#endif

#include "GLFW/glfw3.h"
#include <GLFW/glfw3native.h>

#include "../Core/Config.h"
