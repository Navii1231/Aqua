#pragma once
#include "GLFW_Core.h"

struct GLFWwindow;

struct WindowProps
{
	std::string name;
	int width = 1280;
	int height = 800;
	bool vSync = true;
	bool fullScr = false;
};

enum class GLFWKeyAction
{
	ePressed         = GLFW_PRESS,
	eRelease         = GLFW_RELEASE,
	eRepeat          = GLFW_REPEAT,
};

class OpenGLWindow
{
public:
	VKLIB_API OpenGLWindow(const WindowProps& props);
	VKLIB_API ~OpenGLWindow();

	struct UserData
	{
		WindowProps props;
		std::string APIversion;
		bool state;

		bool cursorHidden;

		void* mUserBuffer = nullptr;

		// all the callbacks
		std::function<bool(const glm::ivec2&)> mFramebufferSizeCallback = [](const glm::ivec2&) { return true; };
		std::function<bool(GLFWKeyAction, int, int, int)> mKeyCallback = [](GLFWKeyAction, int, int, int) { return true;};
		std::function<bool(uint32_t)> mCharCallback = [](uint32_t) { return true; };
		std::function<bool(GLFWKeyAction, int, int)> mMouseButtonCallback = [](GLFWKeyAction, int, int) { return true; };
		std::function<bool(const glm::ivec2&)> mWindowPositionCallback = [](const glm::ivec2&) { return true; };
		std::function<bool()> mWindowCloseCallback = []() { return true; };
		std::function<bool(const glm::vec2&)> mScrollCallback = [](const glm::vec2&) { return true; };
		std::function<bool(const glm::vec2&)> mCursorPosCallback = [](const glm::vec2&) { return true;};
	};

	OpenGLWindow(const OpenGLWindow&) = delete;
	OpenGLWindow& operator=(const OpenGLWindow&) = delete;

	VKLIB_API void SetVSync(bool val);
	VKLIB_API void ShutDown();

	VKLIB_API bool IsWindowClosed() const;
	VKLIB_API void OnUpdate() const;

	VKLIB_API void SetTitle(const std::string& title);

	VKLIB_API void PollUserEvents() const;

	VKLIB_API void HideCursor();
	VKLIB_API void RetrieveCursor();
	VKLIB_API bool IsCursorHidden() const;

	VKLIB_API void SwapFramebuffers();
	VKLIB_API glm::vec2 GetCursorPosition() const;
	VKLIB_API glm::ivec2 GetWindowPosition() const;

	VKLIB_API void HideCursor() const;
	VKLIB_API void RetrieveCursor() const;

	VKLIB_API void SetWindowDefaultStyle();

	VKLIB_API bool KeyPressed(int key) const;
	VKLIB_API bool MouseButtonPressed(int button) const;

	VKLIB_API void SetFramebufferSizeCallback(std::function<bool(const glm::ivec2&)> fn);
	VKLIB_API void SetKeyCallback(std::function<bool(GLFWKeyAction, int, int, int)> fn);
	VKLIB_API void SetCharCallback(std::function<bool(uint32_t)> fn);
	VKLIB_API void SetMouseButtonCallback(std::function<bool(GLFWKeyAction, int, int)> fn);
	VKLIB_API void SetWindowPosCallback(std::function<bool(const glm::ivec2&)> fn);
	VKLIB_API void SetWindowSizeCallback(std::function<bool(const glm::vec2&)> fn);
	VKLIB_API void SetWindowCloseCallback(std::function<bool()> fn);
	VKLIB_API void SetScrollCallback(std::function<bool(const glm::vec2&)> fn);
	VKLIB_API void SetCursorPosCallback(std::function<bool(const glm::vec2&)> fn);

	glm::ivec2 GetWindowSize() const { return { mData.props.width, mData.props.height }; }
	inline GLFWwindow* GetNativeHandle() { return mWindow; }

private:
	GLFWwindow* mWindow;
	UserData mData;

	void SetWindowCallBacks();

	friend class WindowCallbacks;
};

