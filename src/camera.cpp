#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <Input.h>
#include <SDL3/SDL.h>

glm::mat4 Camera::GetViewMatrix() const
{
    // to create a correct model view, we need to move the world in opposite direction to the camera so we will create the camera model matrix and invert
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), _position);
    const glm::mat4 cameraRotation = GetRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::GetRotationMatrix() const
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into the final rotation matrix
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

// TODO:
// Movement in this code is frame-dependant, as we aren't taking the speed of the engine into account.
void Camera::Update(SDL_Window* aWindow)
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
    _pitch = glm::clamp(_pitch, -maxPitch, maxPitch);

    // 4. Handle Movement (WASD)
    _velocity = glm::vec3(0.0f); // Reset velocity every frame

    _velocity.z += Input::Instance().IsKeyHeld(SDL_SCANCODE_S) - Input::Instance().IsKeyHeld(SDL_SCANCODE_W);
    _velocity.x += Input::Instance().IsKeyHeld(SDL_SCANCODE_D) - Input::Instance().IsKeyHeld(SDL_SCANCODE_A);

	const glm::mat4 cameraRotation = GetRotationMatrix();
	_position += glm::vec3(cameraRotation * glm::vec4(_velocity * 0.5f, 0.f));
}
