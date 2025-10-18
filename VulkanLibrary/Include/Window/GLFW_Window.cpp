#include "Core/vkpch.h" // This cpp file should exist in the src folder
#include "GLFW_Window.h"

static uint32_t sWindowCount = 0;
static std::mutex sWindowLock;

class WindowCallbacks
{
public:
	static void framebuffer_size_callback(GLFWwindow*, int, int);
	static void key_callback(GLFWwindow*, int, int, int, int);
	static void char_callback(GLFWwindow* window, unsigned int keyCode);
	static void mouse_button_callback(GLFWwindow*, int, int, int);
	static void window_close_callback(GLFWwindow* window);
	static void scroll_callback(GLFWwindow* window, double x, double y);
	static void cursor_pos_callback(GLFWwindow* window, double x, double y);
	static void window_pos_callback(GLFWwindow* window, int x, int y);
	static void window_size_callback(GLFWwindow* window, int x, int y);
	static void error_callback(int errCode, const char* errString);
};

OpenGLWindow::OpenGLWindow(const WindowProps& props)
	: mWindow(), mData({ props, props.name, true, false }) 
{
	std::scoped_lock locker(sWindowLock);

	if (!glfwInit() && sWindowCount == 0)
	{
		mData.state = false;
		return;
	}

	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if(props.fullScr)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	mWindow = glfwCreateWindow(props.width, props.height, props.name.c_str(), nullptr, nullptr);

	glfwGetWindowSize(mWindow, &mData.props.width, &mData.props.height);

	if (!mWindow)
	{
		mData.state = false;
		glfwTerminate();

		_STL_ASSERT(mData.state, "Failed to create a Window!");

		return;
	}

	sWindowCount++;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	glfwMakeContextCurrent(mWindow);

	mData.APIversion = "No API";//(const char*)glGetString(GL_VERSION);
	SetVSync(props.vSync);

	SetWindowCallBacks();

	SetWindowDefaultStyle();
}

OpenGLWindow::~OpenGLWindow()
{
	std::scoped_lock locker(sWindowLock);
	sWindowCount--;

	ShutDown();

	if(sWindowCount == 0)
		glfwTerminate();

	mWindow = nullptr;
}

void OpenGLWindow::SetVSync(bool val)
{
	mData.props.vSync = val;
	glfwSwapInterval(val);
}

void OpenGLWindow::ShutDown()
{
	glfwDestroyWindow((GLFWwindow*)mWindow);
}

bool OpenGLWindow::IsWindowClosed() const
{
	return glfwWindowShouldClose((GLFWwindow*)mWindow);
}

void OpenGLWindow::OnUpdate() const
{
	//glfwSwapBuffers(mWindow);
	glfwPollEvents();
}

void OpenGLWindow::SetTitle(const std::string& title)
{
	mData.props.name = title;
	glfwSetWindowTitle(mWindow, mData.props.name.c_str());
}

void OpenGLWindow::PollUserEvents() const
{
	glfwPollEvents();
}

void OpenGLWindow::HideCursor()
{
	mData.cursorHidden = true;
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void OpenGLWindow::RetrieveCursor()
{
	mData.cursorHidden = false;
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool OpenGLWindow::IsCursorHidden() const
{
	return mData.cursorHidden;
}

void OpenGLWindow::SwapFramebuffers()
{
	glfwSwapBuffers(mWindow);
	glfwPollEvents();
}

glm::vec2 OpenGLWindow::GetCursorPosition() const
{
	double x, y;
	glfwGetCursorPos(mWindow, &x, &y);
	return { (float) x, (float) y };
}

glm::ivec2 OpenGLWindow::GetWindowPosition() const
{
	glm::ivec2 pos{};
	glfwGetWindowPos(mWindow, &pos.x, &pos.y);

	return pos;
}

void OpenGLWindow::HideCursor() const
{
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void OpenGLWindow::RetrieveCursor() const
{
	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void OpenGLWindow::SetWindowDefaultStyle()
{
}

bool OpenGLWindow::KeyPressed(int key) const
{
	return glfwGetKey(mWindow, key);
}

bool OpenGLWindow::MouseButtonPressed(int key) const
{
	return glfwGetMouseButton(mWindow, key);
}

void OpenGLWindow::SetFramebufferSizeCallback(std::function<bool(const glm::ivec2&)> fn)
{
	mData.mFramebufferSizeCallback = fn;
}

void OpenGLWindow::SetKeyCallback(std::function<bool(GLFWKeyAction action, int key, int mod, int scancode)> fn)
{
	mData.mKeyCallback = fn;
}

void OpenGLWindow::SetCharCallback(std::function<bool(uint32_t)> fn)
{
	mData.mCharCallback = fn;
}

void OpenGLWindow::SetMouseButtonCallback(std::function<bool(GLFWKeyAction, int, int)> fn)
{
	mData.mMouseButtonCallback = fn;
}

void OpenGLWindow::SetWindowPosCallback(std::function<bool(const glm::ivec2&)> fn)
{
	mData.mWindowPositionCallback = fn;
}

void OpenGLWindow::SetWindowSizeCallback(std::function<bool(const glm::vec2&)> fn)
{
	mData.mFramebufferSizeCallback = fn;
}

void OpenGLWindow::SetWindowCloseCallback(std::function<bool()> fn)
{
	mData.mWindowCloseCallback = fn;
}

void OpenGLWindow::SetScrollCallback(std::function<bool(const glm::vec2&)> fn)
{
	mData.mScrollCallback = fn;
}

void OpenGLWindow::SetCursorPosCallback(std::function<bool(const glm::vec2&)> fn)
{
	mData.mCursorPosCallback = fn;
}

void OpenGLWindow::SetWindowCallBacks()
{
	glfwSetWindowUserPointer(mWindow, &mData);

	glfwSetFramebufferSizeCallback(mWindow, WindowCallbacks::framebuffer_size_callback);
	glfwSetKeyCallback(mWindow, WindowCallbacks::key_callback);
	glfwSetCharCallback(mWindow, WindowCallbacks::char_callback);
	glfwSetMouseButtonCallback(mWindow, WindowCallbacks::mouse_button_callback);
	glfwSetWindowPosCallback(mWindow, WindowCallbacks::window_pos_callback);
	glfwSetWindowSizeCallback(mWindow, WindowCallbacks::window_size_callback);
	glfwSetWindowCloseCallback(mWindow, WindowCallbacks::window_close_callback);
	glfwSetScrollCallback(mWindow, WindowCallbacks::scroll_callback);
	glfwSetCursorPosCallback(mWindow, WindowCallbacks::cursor_pos_callback);
	glfwSetErrorCallback(WindowCallbacks::error_callback);
}

void WindowCallbacks::framebuffer_size_callback(GLFWwindow* window, int x, int y)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->props.width = x;
	me->props.height = y;

	me->mFramebufferSizeCallback({ x, y });
}

void WindowCallbacks::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mKeyCallback(GLFWKeyAction(action), key, mods, scancode);
}

void WindowCallbacks::char_callback(GLFWwindow* window, unsigned int keyCode)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mCharCallback(keyCode);
}

void WindowCallbacks::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mMouseButtonCallback(GLFWKeyAction(action), button, mods);
}

void WindowCallbacks::window_pos_callback(GLFWwindow* window, int x, int y)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mWindowPositionCallback({ x, y });
}

void WindowCallbacks::window_size_callback(GLFWwindow* window, int x, int y)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->props.width = x;
	me->props.height = y;

	me->mFramebufferSizeCallback({ x, y });
}

void WindowCallbacks::window_close_callback(GLFWwindow* window)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mWindowCloseCallback();
}

void WindowCallbacks::scroll_callback(GLFWwindow* window, double x, double y)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mScrollCallback({ static_cast<float>(x), static_cast<float>(y) });
}

void WindowCallbacks::cursor_pos_callback(GLFWwindow* window, double x, double y)
{
	OpenGLWindow::UserData* me = (OpenGLWindow::UserData*)glfwGetWindowUserPointer(window);

	me->mCursorPosCallback({ static_cast<float>(x), static_cast<float>(y) });
}

void WindowCallbacks::error_callback(int errCode, const char* errString)
{
	char buffer[1024];
	sprintf_s(buffer, "OpenGL window ran into an error: \nError Code: %x\nInfo: %s", errCode, errString);

	_STL_ASSERT(false, buffer);
}
