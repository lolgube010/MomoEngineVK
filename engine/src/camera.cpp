#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Input.h"

glm::mat4 Camera::GetViewMatrix() const
{
    // to create a correct model view, we need to move the world in opposite direction to the camera so we will create the camera model matrix and invert
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 cameraRotation = GetRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::GetRotationMatrix() const
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into the final rotation matrix
    const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

// TODO:
// Movement in this code is frame-dependant, as we aren't taking the speed of the engine into account. This is done for simplicity in the case, if you want to improve it, you would need to pass deltaTime (time between frames) to the update() function, and multiply the velocity by that. In the tutorial, we are more or less FPS locked to monitor speed due to the options we have used in the swapchain, and we aren't rendering enough data to slow down the engine.
void Camera::Update()
{
    const auto& input = Input::Instance();
    if (input.IsKeyPressed(SDL_SCANCODE_TAB))
    {
        isLocked = !isLocked;
    }
    if (input.IsKeyPressed(SDL_SCANCODE_CAPSLOCK))
    {
        SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!SDL_GetRelativeMouseMode()));
    }

    if (isLocked)
    {
        return;
    }

    constexpr float mouseSensitivity = 0.005f;
    constexpr float maxPitch = 1.50f;

    yaw += static_cast<float>(Input::Instance().GetMouseDeltaX()) * mouseSensitivity;
    pitch -= static_cast<float>(Input::Instance().GetMouseDeltaY()) * mouseSensitivity;
    pitch = glm::clamp(pitch, -maxPitch, maxPitch);

    // 4. Handle Movement (WASD)
    velocity = glm::vec3(0.0f); // Reset velocity every frame

    velocity.z += Input::Instance().IsKeyHeld(SDL_SCANCODE_S) - Input::Instance().IsKeyHeld(SDL_SCANCODE_W);
    velocity.x += Input::Instance().IsKeyHeld(SDL_SCANCODE_D) - Input::Instance().IsKeyHeld(SDL_SCANCODE_A);

	const glm::mat4 cameraRotation = GetRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}
