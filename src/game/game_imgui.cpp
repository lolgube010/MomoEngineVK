#include <game/game_imgui.h>
#include <imgui.h>
#include <api/imgui_utils.h>

#include "game_state.h"

void GameImGui::DrawImGui(GameState& aGameState)
{
    if (ImGui::Begin("settings"))
    {
        if (momo_imgui::BeginSection("Gameplay", momo_imgui::GAMEPLAY_TINT))
        {
            if (momo_imgui::CategoryHeader("Camera", momo_imgui::GAMEPLAY_TINT))
            {
                ImGui::SliderFloat("hej_hej!", &aGameState._cameraData._cameraFOV, 1, 180);
                ImGui::Value("Pitch (rad)", aGameState._cameraData._pitch);
            }
            momo_imgui::EndSection();
        }
    }
    ImGui::End();
}
