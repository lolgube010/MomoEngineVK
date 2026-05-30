#include <game/game_imgui.h>
#include <imgui.h>
#include <api/imgui_utils.h>

#include "game_state.h"

void GameImGui::DrawImGui(GameState& aGameState)
{
    if (ImGui::Begin("settings"))
    {
        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::SliderFloat("FOV", &aGameState.GetCameraDataMutable()._cameraFOV, 1, 180);
            ImGui::Value("Pitch (rad)", aGameState.GetCameraData()._pitch);
        }
    }
    ImGui::End();
}
