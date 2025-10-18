#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

// Enum for camera movements
enum class CameraMovement : uint32_t 
{
	eForward               = 1 << 0,
	eBackward              = 1 << 1,
	eLeft                  = 1 << 2,
	eRight                 = 1 << 3,
	eUp                    = 1 << 4,
	eDown                  = 1 << 5
};

enum class CameraMode
{
	eCustom             = 1,
	e2D                 = 2,
	e3D                 = 3,
};

// Custom CameraMovementFlags class to handle bitwise operations
class CameraMovementFlags 
{
public:
	CameraMovementFlags() : mFlags(0) {}
	CameraMovementFlags(CameraMovement movement) : mFlags(static_cast<uint32_t>(movement)) {}

	void SetFlag(CameraMovement movement) {
		mFlags |= static_cast<uint32_t>(movement);
	}

	void ClearFlag(CameraMovement movement) {
		mFlags &= ~static_cast<uint32_t>(movement);
	}

	bool HasFlag(CameraMovement movement) const {
		return (mFlags & static_cast<uint32_t>(movement)) != 0;
	}

	void ClearAll() {
		mFlags = 0;
	}

private:
	uint32_t mFlags;
};

// Struct for camera specifications
struct EditorCamera3DSpecs 
{
	// 3d settings
	float Fov = glm::radians(90.0f);                   // Field of view (in degrees)
	float NearClip = 0.1f;               // Near clip plane (in meters)
	float FarClip = 100.0f;              // Far clip plane (in meters)
	float AspectRatio = 16.0f / 9.0f;    // Aspect ratio
};

// EditorCamera class
class EditorCamera 
{
public:
	using NanoSeconds = std::chrono::nanoseconds;

	EditorCamera()
	{
		UpdateProjection();
		UpdateView();
	}

	// Set camera specifications
	void SetCameraSpec3D(const EditorCamera3DSpecs& specs)
	{
		mCameraSpecs = specs;
		mMode = CameraMode::e3D;
		UpdateProjection();
	}

	void SetCameraSpec2D(const glm::vec3& rectPos, const glm::vec3& rectSize)
	{
		mRectPos = rectPos;
		mRectSize = rectSize;
		mMode = CameraMode::e2D;
		UpdateProjection();
	}

	void SetCustomProjection(const glm::mat4& proj)
	{
		mProjection = proj;
		mMode = CameraMode::eCustom;
	}

	// Set position directly (updates view matrix)
	void SetPosition(const glm::vec3& position)
	{
		mPosition = position;
		UpdateView();
	}

	// Set orientation directly (takes orientation matrix and calculates yaw, pitch, roll)
	void SetOrientation(const glm::mat3& orientation)
	{
		mForwardDirection = glm::normalize(orientation[2]);  // Z-axis (forward)
		mRightDirection = glm::normalize(orientation[0]);    // X-axis (right)
		mUpDirection = glm::normalize(orientation[1]);       // Y-axis (up)

		// Manually calculate pitch, yaw, and roll from the orientation matrix
		mYaw = glm::degrees(std::atan2(orientation[1][0], orientation[1][2]));
		mPitch = glm::degrees(std::acos(orientation[1][1]));
		mRoll = glm::degrees(std::atan2(-orientation[0][1], orientation[2][1]));

		UpdateView();
	}

	// Updated OnUpdate method
	bool OnUpdate(NanoSeconds deltaTime, const CameraMovementFlags& movementFlags,
		const glm::vec2& mousePos, bool rotate = true)
	{
		// Convert nanoseconds to seconds
		float dt = static_cast<float>(deltaTime.count()) / NanoSecondsTicksInOneSecond();

		mMoved = false;
		bool CameraMoved = false;
		bool CameraRotated = false;

		glm::vec3 movement(0.0f);
		float speed = mMoveSpeed * dt;

		auto UpdateMovement = [&CameraMoved, &movement, &movementFlags]
			(CameraMovement flag, const glm::vec3& moveIn)
		{
			if (movementFlags.HasFlag(flag))
			{
				movement += moveIn;
				CameraMoved |= true;
			}
		};

		UpdateMovement(CameraMovement::eForward,   mForwardDirection * speed);
		UpdateMovement(CameraMovement::eBackward, -mForwardDirection * speed);
		UpdateMovement(CameraMovement::eRight,     mRightDirection * speed);
		UpdateMovement(CameraMovement::eLeft,     -mRightDirection * speed);
		UpdateMovement(CameraMovement::eUp,        mUpDirection * speed);
		UpdateMovement(CameraMovement::eDown,     -mUpDirection * speed);

		mPosition += movement;

		if (rotate) 
		{
			// Assuming the mousePos coming from GLFW window
			// i.e. the vertical range goes from bottom to top of the screen
			float dx = mousePos.x - mLastMouseX;
			float dy = mousePos.y - mLastMouseY;

			mYaw += dx * mRotationSpeed * mHorizontalScale;
			mPitch += dy * mRotationSpeed * mVerticalScale;

			if (mPitch > 89.0f) mPitch = 89.0f;
			if (mPitch < -89.0f) mPitch = -89.0f;

			// todo: floating imprecision will bite back in the future
			// but we don't care for now
			CameraRotated |= (dx != 0.0f || dy != 0.0f);
		}

		mLastMouseX = mousePos.x;
		mLastMouseY = mousePos.y;

		mMoved |= CameraMoved;
		mMoved |= CameraRotated;

		if(mMoved)
			UpdateView();

		return mMoved;
	}

	bool Moved() const { return mMoved; }

	// Get separate view and projection matrices
	const glm::mat4& GetViewMatrix() const { return mView; }
	const glm::mat4& GetProjectionMatrix() const { return mProjection; }

	// Set linear and rotation speeds
	void SetLinearSpeed(float speed) { mMoveSpeed = speed; }
	void SetRotationSpeed(float speed) { mRotationSpeed = speed; }

	void SetVerticalScale(float scale) { mVerticalScale = scale; }
	void SetHorizontalScale(float scale) { mHorizontalScale = scale; }

	// Constant for  float NanoSecondsTicksInOneSecond() { return 1e9f; }
	//converting nanoseconds to seconds
	static constexpr float NanoSecondsTicksInOneSecond() { return 1e9f; }


private:
	// Camera specifications
	EditorCamera3DSpecs mCameraSpecs;
	glm::vec3 mRectPos;
	glm::vec3 mRectSize;

	CameraMode mMode = CameraMode::e3D;

	// Camera properties
	glm::vec3 mPosition = glm::vec3(0.0f, 0.0f, -5.0f);
	glm::vec3 mForwardDirection = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 mRightDirection = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 mUpDirection = glm::vec3(0.0f, 1.0f, 0.0f);

	float mYaw = 0.0f;
	float mPitch = 0.0f;
	float mRoll = 0.0f;

	float mVerticalScale = 1.0f;
	float mHorizontalScale = 1.0f;

	float mMoveSpeed = 5.0f;
	float mRotationSpeed = 0.1f;
	float mLastMouseX = 0.0f;
	float mLastMouseY = 0.0f;

	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProjection = glm::mat4(1.0f);

	bool mMoved = false;

private:

	static constexpr glm::mat4 InvertY_Axis()
	{
		return glm::mat4(
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f }
		);
	}

	// Updates the projection matrix based on current camera specs
	void UpdateProjection()
	{
		switch (mMode)
		{
			case CameraMode::eCustom:
				break;
			case CameraMode::e2D:
				mProjection = glm::orthoLH(
					-mRectSize.x / 2.0f + mRectPos.x, mRectSize.x / 2.0f + mRectPos.x,
					-mRectSize.y / 2.0f + mRectPos.y, mRectSize.y / 2.0f + mRectPos.y,
					-mRectSize.z / 2.0f + mRectPos.z, mRectSize.z / 2.0f + mRectPos.z);
				break;
			case CameraMode::e3D:
				mProjection = glm::perspectiveLH(
					mCameraSpecs.Fov,
					mCameraSpecs.AspectRatio, 
					mCameraSpecs.NearClip, 
					mCameraSpecs.FarClip) * InvertY_Axis();
				break;
			default:
				break;

		}
	}

	// Updates the view matrix based on position, yaw, and pitch
	void UpdateView()
	{
		// Calculate the new Front vector
		glm::mat4 rotationMatrix(1.0f);
		rotationMatrix = glm::rotate(rotationMatrix,
			glm::radians(mYaw), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw rotation

		rotationMatrix = glm::rotate(rotationMatrix,
			glm::radians(mPitch), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch rotation

		mForwardDirection = rotationMatrix[2];
		mRightDirection = rotationMatrix[0];
		mUpDirection = rotationMatrix[1];

		mView = glm::lookAtLH(mPosition, mPosition + mForwardDirection, mUpDirection);

		//mView *= InvertY_Axis();
	}
};

AQUA_END
