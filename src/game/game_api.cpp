#include "game_api.h"
#include <imgui.h>
#include "game_state.h"
#include "game_imgui.h"
#include "input/Input.h"

// Internal to the DLL. Not exported; never resolved by GetProcAddress.
static void Update_Camera(Camera& aCamera, float aDt, const InputData& aInput);

extern "C" {

void Game_Init(GameState* aState, const ImGuiBridge* aImGui)
{
    // GImGui and the allocator globals are per-module, so a freshly loaded game
    // module starts with its own. Point them at the host's. Must run before any
    // ImGui call in this module, and again after every hot-reload.
    ImGui::SetCurrentContext(aImGui->ctx);
    ImGui::SetAllocatorFunctions(aImGui->allocFunc, aImGui->freeFunc, aImGui->userData);
}

// This was previously GameState::Update()
void Game_Update(GameState* aState, const double aDT, const InputData* aInput)
{
    Update_Camera(aState->_cameraData, static_cast<float>(aDT), *aInput);
}

void Game_DrawImGui(GameState* aState)
{
    // Context + allocator were adopted in Game_Init, so ImGui calls here are safe.
    GameImGui::DrawImGui(*aState);
}

} // extern "C"

// This was previously GameState::Update_Camera()
static void Update_Camera(Camera& aCamera, const float aDt, const InputData& aInput)
{
    if (aInput.IsKeyPressed(SDL_SCANCODE_TAB))
    {
        aCamera._isLocked = !aCamera._isLocked;
    }
    if (aInput.IsKeyPressed(SDL_SCANCODE_CAPSLOCK))
    {
        aCamera._wantMouseCaptured = !aCamera._wantMouseCaptured;
    }

    if (aCamera._isLocked)
    {
        return;
    }

    aCamera._yaw += aInput.GetMouseDeltaX() * aCamera._mouseSensitivity;
    aCamera._pitch -= aInput.GetMouseDeltaY() * aCamera._mouseSensitivity;
    aCamera._pitch = glm::clamp(aCamera._pitch, -aCamera._maxPitch, aCamera._maxPitch);

    aCamera._velocity = glm::vec3(0.0f); // Reset velocity every frame
    aCamera._velocity.z += static_cast<float>(aInput.IsKeyHeld(SDL_SCANCODE_S) - aInput.IsKeyHeld(SDL_SCANCODE_W));
    aCamera._velocity.x += static_cast<float>(aInput.IsKeyHeld(SDL_SCANCODE_D) - aInput.IsKeyHeld(SDL_SCANCODE_A));

    const glm::mat4 cameraRotation = Camera_Util::get_rotation_matrix(aCamera);
    aCamera._position += glm::vec3(cameraRotation * glm::vec4(aCamera._velocity * aCamera._moveSpeed * aDt * 5.f, 0.f));
}
