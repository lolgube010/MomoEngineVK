#pragma once

namespace ImGui
{
    template <typename T, typename Getter, typename Setter>
    static void SliderFloat(const char* label, T* t, Getter getter, Setter setter, float min, float max,
                            const char* format = "%.3f")
    {
        float current = (t->*getter)();
        float new_value = current;

        ImGui::SliderFloat(label, &new_value, min, max, format);

        if (current != new_value)
        {
            (t->*setter)(new_value);
        }
    }

} // namespace ImGui
