#pragma once
#include <imgui.h>

namespace ImGui
{
    template <typename T, typename Getter, typename Setter>
    static void SliderFloat(const char* label, T* t, Getter getter, Setter setter, float min, float max, const char* format = "%.3f")
    {
        float current = (t->*getter)();
        float new_value = current;

        SliderFloat(label, &new_value, min, max, format);

        if (current != new_value)
        {
            (t->*setter)(new_value);
        }
    }

} // namespace ImGui

namespace momo_imgui
{
    // One hue per subsystem so the shared settings window reads at a glance.
    inline constexpr ImVec4 ENGINE_TINT  {0.16f, 0.32f, 0.22f, 1.0f}; // green
    inline constexpr ImVec4 RENDERER_TINT{0.8f, 0.26f, 0.42f, 1.0f}; // pink
    inline constexpr ImVec4 GAMEPLAY_TINT{0.42f, 0.28f, 0.14f, 1.0f}; // amber
    
    inline ImVec4 Scale(const ImVec4& aColor, const float aScale)
    {
        return {aColor.x * aScale, aColor.y * aScale, aColor.z * aScale, aColor.w};
    }

    inline ImVec4 Add(const ImVec4& aColor, const ImVec4& anotherColor)
    {
        return {aColor.x + anotherColor.x, aColor.y + anotherColor.y, aColor.z + anotherColor.z, aColor.w + anotherColor.w};
    }

    // A CollapsingHeader tinted by its owning subsystem. Hovered/active are scaled
    // brighter so it still reads as interactive.
    inline bool CategoryHeader(const char* aLabel, const ImVec4& aTint, const ImGuiTreeNodeFlags aFlags = 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Header,        aTint);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Scale(aTint, 1.4f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  Scale(aTint, 1.7f));
        const bool open = ImGui::CollapsingHeader(aLabel, aFlags);
        ImGui::PopStyleColor(3);
        return open;
    }

    // A colored, labeled divider to mark where one contributor's region begins in a shared window.
    inline void SectionBanner(const char* aLabel, const ImVec4& aTint)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Scale(aTint, 2.2f));
        ImGui::SeparatorText(aLabel);
        ImGui::PopStyleColor();
    }

    // A top-level collapsible section. Wrap child CategoryHeaders between a
    // `if (BeginSection(...)) { ... EndSection(); }` so the whole group can be closed.
    inline bool BeginSection(const char* aLabel, const ImVec4& aTint)
    {
        const bool open = CategoryHeader(aLabel, aTint, ImGuiTreeNodeFlags_DefaultOpen);
        if (open)
        {
            ImGui::Indent();
        }
        return open;
    }

    inline void EndSection()
    {
        ImGui::Unindent();
    }
}
