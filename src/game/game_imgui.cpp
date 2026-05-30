#include <game/game_imgui.h>
#include <imgui.h>
#include <api/imgui_utils.h>

#include "game_state.h"

void GameImGui::DrawImGui(GameState& aGameState)
{
    if (ImGui::Begin("settings"))
    {
        if (Momo_Imgui::BeginSection("Gameplay", Momo_Imgui::GAMEPLAY_TINT))
        {
            if (Momo_Imgui::CategoryHeader("Camera", Momo_Imgui::GAMEPLAY_TINT))
            {
                ImGui::SliderFloat("FOV", &aGameState._cameraData._cameraFOV, 1, 180);
                ImGui::Value("Pitch (rad)", aGameState._cameraData._pitch);
            }
            Momo_Imgui::EndSection();
        }
    }
    ImGui::End();
}
