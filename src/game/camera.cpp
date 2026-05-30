#include <game/camera.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <input/Input.h>
#include <SDL3/SDL.h>

glm::mat4 Camera::GetViewMatrix() const
{
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), _position);
    const glm::mat4 cameraRotation = GetRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::GetRotationMatrix() const
{
    const glm::quat pitchRotation = glm::angleAxis(_pitch, glm::vec3{ 1.f, 0.f, 0.f });
    const glm::quat yawRotation = glm::angleAxis(_yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 Camera::GetProjectionMatrix(const float aWidth, const float aHeight) const
{
    auto matrix = glm::perspective(glm::radians(_tempCameraFov), aWidth / aHeight, 10000.f, 0.1f);
    matrix[1][1] *= -1;  // invert the Y direction on projection matrix so that we are more similar to opengl and gltf axis
    return matrix;
}

void Camera::Update(SDL_Window* aWindow, float aDt)
{
    const auto& input = Input::Instance();
    if (input.IsKeyPressed(SDL_SCANCODE_TAB))
    {
        _isLocked = !_isLocked;
    }
    if (input.IsKeyPressed(SDL_SCANCODE_CAPSLOCK))
    {
        SDL_SetWindowRelativeMouseMode(aWindow, !SDL_GetWindowRelativeMouseMode(aWindow));
    }

    if (_isLocked)
    {
        return;
    }

    constexpr float mouseSensitivity = 0.005f;
    constexpr float maxPitch = 1.50f;

    _yaw += Input::Instance().GetMouseDeltaX() * mouseSensitivity;
    _pitch -= Input::Instance().GetMouseDeltaY() * mouseSensitivity;
    Input::Instance().ResetMouseDelta();
    _pitch = glm::clamp(_pitch, -maxPitch, maxPitch);

    _velocity = glm::vec3(0.0f); // Reset velocity every frame

    _velocity.z += Input::Instance().IsKeyHeld(SDL_SCANCODE_S) - Input::Instance().IsKeyHeld(SDL_SCANCODE_W);
    _velocity.x += Input::Instance().IsKeyHeld(SDL_SCANCODE_D) - Input::Instance().IsKeyHeld(SDL_SCANCODE_A);

    constexpr float MOVE_SPEED = 30.0f;  // units/second (was 0.5f/frame * 60fps)
	const glm::mat4 cameraRotation = GetRotationMatrix();
	_position += glm::vec3(cameraRotation * glm::vec4(_velocity * MOVE_SPEED * aDt, 0.f));
}
