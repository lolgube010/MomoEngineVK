#include <game/game_state.h>

#include "cvars/cvars.h"
#include "input/Input.h"

void GameState::Update(const double aDT, const InputData& aInputData)
{
    Update_Camera(_cameraData, static_cast<float>(aDT), aInputData);
}

void GameState::Update_Camera(Camera& aCameraPOV, const float aDt, const InputData& aInputData) const
{
    if (aInputData.IsKeyPressed(SDL_SCANCODE_TAB))
    {
        aCameraPOV._isLocked = !aCameraPOV._isLocked;
    }
    if (aInputData.IsKeyPressed(SDL_SCANCODE_CAPSLOCK))
    {
        aCameraPOV._wantMouseCaptured = !aCameraPOV._wantMouseCaptured;
    }

    if (aCameraPOV._isLocked)
    {
        return;
    }

    aCameraPOV._yaw += aInputData.GetMouseDeltaX() * aCameraPOV._mouseSensitivity;
    aCameraPOV._pitch -= aInputData.GetMouseDeltaY() * aCameraPOV._mouseSensitivity;
    aCameraPOV._pitch = glm::clamp(aCameraPOV._pitch, -aCameraPOV._maxPitch, aCameraPOV._maxPitch);

    aCameraPOV._velocity = glm::vec3(0.0f); // Reset velocity every frame
    aCameraPOV._velocity.z += static_cast<float>(aInputData.IsKeyHeld(SDL_SCANCODE_S) - aInputData.IsKeyHeld(SDL_SCANCODE_W));
    aCameraPOV._velocity.x += static_cast<float>(aInputData.IsKeyHeld(SDL_SCANCODE_D) - aInputData.IsKeyHeld(SDL_SCANCODE_A));

    const glm::mat4 cameraRotation = Camera_Util::get_rotation_matrix(_cameraData);
    aCameraPOV._position += glm::vec3(cameraRotation * glm::vec4(aCameraPOV._velocity * aCameraPOV._moveSpeed * aDt, 0.f));
}

const Camera& GameState::GetCameraData() const
{
    return _cameraData;
}

Camera& GameState::GetCameraDataMutable()
{
    return _cameraData;
}