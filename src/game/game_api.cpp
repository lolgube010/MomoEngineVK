#include "game_api.h"
#include <imgui.h>
#include "game_state.h"
#include "game_imgui.h"
#include "input/Input.h"

// Everything here is internal to the DLL now. Only Game_GetAPI (at the bottom) is
// exported; the host reaches these functions through the table it fills, never by
// resolving them individually.
static void Update_Camera(Camera& aCamera, float aDt, const InputData& aInput);

static void Game_Init(GameState* aState, const ImGuiBridge* aImGui)
{
    // GImGui and the allocator globals are per-module, so a freshly loaded game
    // module starts with its own. Point them at the host's. Must run before any
    // ImGui call in this module, and again after every hot-reload.
    ImGui::SetCurrentContext(aImGui->ctx);
    ImGui::SetAllocatorFunctions(aImGui->allocFunc, aImGui->freeFunc, aImGui->userData);
}

// This was previously GameState::Update()
static void Game_Update(GameState* aState, const double aDT, const InputData* aInput)
{
    Update_Camera(aState->_cameraData, static_cast<float>(aDT), *aInput);
}

static void Game_DrawImGui(GameState* aState)
{
    // Context + allocator were adopted in Game_Init, so ImGui calls here are safe.
    GameImGui::DrawImGui(*aState);
}

// The single exported entry point: hand the host the whole function table.
extern "C" void Game_GetAPI(GameAPI* aOutApi)
{
    aOutApi->Init      = &Game_Init;
    aOutApi->Update    = &Game_Update;
    aOutApi->DrawImGui = &Game_DrawImGui;
}

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
    aCamera._position += glm::vec3(cameraRotation * glm::vec4(aCamera._velocity * aCamera._moveSpeed * aDt, 0.f));
}
