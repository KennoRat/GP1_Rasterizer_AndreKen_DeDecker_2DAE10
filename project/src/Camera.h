#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Maths.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle) :
			origin{ _origin },
			fovAngle{ _fovAngle }
		{
		}


		Vector3 origin{};
		float fovAngle{ 90.f };
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{ Vector3::UnitZ };
		Vector3 up{ Vector3::UnitY };
		Vector3 right{ Vector3::UnitX };

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};

		void Initialize(float _fovAngle = 90.f, Vector3 _origin = { 0.f,0.f,0.f })
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			//TODO W1
			//ONB => invViewMatrix
			//Inverse(ONB) => ViewMatrix
			Vector3 WorldUp{ Vector3::UnitY };

			right = Vector3::Cross(WorldUp, forward);
			right.Normalize();

			up = Vector3::Cross(forward, right);
			up.Normalize();

			Vector4 xRow{ right.x, right.y, right.z, 0 };
			Vector4 yRow{ up.x, up.y, up.z, 0 };
			Vector4 zRow{ forward.x, forward.y, forward.z, 0 };
			Vector4 tRow{ origin.x, origin.y, origin.z, 1 };

			invViewMatrix = Matrix(xRow, yRow, zRow, tRow);
			viewMatrix = invViewMatrix.Inverse();

			//ViewMatrix => Matrix::CreateLookAtLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix()
		{
			//TODO W3

			//ProjectionMatrix => Matrix::CreatePerspectiveFovLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			//Camera Update Logic
			const float cameraSpeed{ 10.f };
			const float cameraRotation{ 2.f };

			//Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

			//Movement WASD
			if (pKeyboardState[SDL_SCANCODE_W]) origin += forward * cameraSpeed * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_S]) origin -= forward * cameraSpeed * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_A]) origin -= right * cameraSpeed * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_D]) origin += right * cameraSpeed * deltaTime;

			//Change FOV
			if (pKeyboardState[SDL_SCANCODE_E]) fovAngle -= cameraSpeed * deltaTime;
			if (pKeyboardState[SDL_SCANCODE_R]) fovAngle += cameraSpeed * deltaTime;

			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);


			//LMB + RMB + Y-axis = Up or Down
			if (mouseState == (SDL_BUTTON(1) + SDL_BUTTON(3)) && mouseY > 0) origin -= up * cameraSpeed * deltaTime;
			else if (mouseState == (SDL_BUTTON(1) + SDL_BUTTON(3)) && mouseY < 0) origin += up * cameraSpeed * deltaTime;

			//LMB + Y-axis = forward or backwards
			if (mouseState == SDL_BUTTON(1) && mouseY > 0) origin -= forward * cameraSpeed * deltaTime;
			else if (mouseState == SDL_BUTTON(1) && mouseY < 0) origin += forward * cameraSpeed * deltaTime;

			// Rotate left and right (Yaw)
			if (mouseState == SDL_BUTTON(1) && mouseX > 0) totalYaw += cameraRotation * deltaTime;
			else if (mouseState == SDL_BUTTON(1) && mouseX < 0) totalYaw -= cameraRotation * deltaTime;

			if (mouseState == SDL_BUTTON(3) && mouseX > 0) totalYaw += cameraRotation * deltaTime;
			else if (mouseState == SDL_BUTTON(3) && mouseX < 0) totalYaw -= cameraRotation * deltaTime;

			// Rotate up and down (Pitch)
			if (mouseState == SDL_BUTTON(3) && mouseY > 0) totalPitch -= cameraRotation * deltaTime;
			else if (mouseState == SDL_BUTTON(3) && mouseY < 0) totalPitch += cameraRotation * deltaTime;

			Matrix finalRotation{ Matrix::CreateRotationX(totalPitch) * Matrix::CreateRotationY(totalYaw) };

			forward = finalRotation.TransformVector(Vector3::UnitZ);

			forward.Normalize();
			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix(); //Try to optimize this - should only be called once or when fov/aspectRatio changes
		}
	};
}
