#pragma once
#include "Execution/Graph.h"
#include "Utils/EditorCamera.h"

struct CameraMovementKeys
{
	ImGuiKey Forward        = ImGuiKey_W;
	ImGuiKey Backward       = ImGuiKey_S;
	ImGuiKey Left           = ImGuiKey_A;
	ImGuiKey Right          = ImGuiKey_D;
	ImGuiKey Up             = ImGuiKey_E;
	ImGuiKey Down           = ImGuiKey_Q;
};

class Layer
{
public:
	Layer() = default;
	Layer(bool deferStart)
		: mStartInvoked(!deferStart) {}

	virtual ~Layer() = default;

	virtual bool OnStart() = 0;
	virtual bool OnUpdate(std::chrono::nanoseconds) = 0;
	virtual bool OnUIUpdate(std::chrono::nanoseconds) = 0;

	virtual bool InvokeFramebufferCallback(const glm::ivec2&) { return false; }
	virtual bool InvokeKeyCallback(GLFWKeyAction, int, int, int) { return false; }
	virtual bool InvokeCharCallback(uint32_t) { return false; }
	virtual bool InvokeMouseButtonCallback(GLFWKeyAction, int, int) { return false; }
	virtual bool InvokeWindowPositionCallback(const glm::ivec2&) { return false; }
	virtual bool InvokeWindowCloseCallback() { return false; }
	virtual bool InvokeCursorPosCallback(const glm::ivec2&) { return false; }
	virtual bool InvokeScrollCallback(const glm::ivec2&) { return false; }

	bool HasStartInvoked() const { return mStartInvoked; }

private:
	bool mStartInvoked = false;

	friend class Application;

private:
	bool FramebufferCallbackHelper(const glm::ivec2& position) { return InvokeFramebufferCallback(position); }
	bool KeyCallbackHelper(GLFWKeyAction action, int key, int mods, int scancode) { return InvokeKeyCallback(action, key, mods, scancode); }
	bool CharCallbackHelper(uint32_t value) { return InvokeCharCallback(value); }
	bool MouseButtonCallbackHelper(GLFWKeyAction action, int button, int mods) { return InvokeMouseButtonCallback(action, button, mods); }
	bool WindowPositionCallbackHelper(const glm::ivec2& position) { return InvokeWindowPositionCallback(position); }
	bool WindowCloseCallbackHelper() { return InvokeWindowCloseCallback(); }
	bool CursorPosCallbackHelper(const glm::ivec2& position) { return InvokeCursorPosCallback(position); }
	bool ScrollCallbackHelper(const glm::ivec2& scroll) { return InvokeScrollCallback(scroll); }

protected:
	template <typename _Camera>
	Aqua::CameraMovementFlags MoveCamera(_Camera& camera, std::chrono::nanoseconds elaspedTime, bool allowOrientation = true, const CameraMovementKeys& keys = {})
	{
		if (!ImGui::IsWindowHovered())
			return {};

		Aqua::CameraMovementFlags movement;

		auto addMovement = [&movement](ImGuiKey key, Aqua::CameraMovement direction)
			{
				if (ImGui::IsKeyDown(key))
				{
					movement.SetFlag(direction);
				}
			};

		addMovement(keys.Forward, Aqua::CameraMovement::eForward);
		addMovement(keys.Backward, Aqua::CameraMovement::eBackward);
		addMovement(keys.Left, Aqua::CameraMovement::eLeft);
		addMovement(keys.Right, Aqua::CameraMovement::eRight);
		addMovement(keys.Up, Aqua::CameraMovement::eUp);
		addMovement(keys.Down, Aqua::CameraMovement::eDown);

		auto mousePosition = ImGui::GetMousePos();

		camera.OnUpdate(elaspedTime, movement, { mousePosition.x, mousePosition.y }, allowOrientation && ImGui::IsMouseDown(ImGuiMouseButton_Left));

		return movement;
	}
};
