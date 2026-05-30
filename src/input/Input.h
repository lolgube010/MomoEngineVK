#pragma once
#include <unordered_set>
#include <SDL3/SDL_events.h>

struct InputData
{
    // Live SDL keyboard state — valid for IsKeyHeld queries.
    const bool* _currentState = nullptr;

    // Filled from SDL_EVENT_KEY_DOWN / KEY_UP events; cleared per fixed update step.
    std::unordered_set<SDL_Scancode> _justPressed;
    std::unordered_set<SDL_Scancode> _justReleased;

    float _mouseDeltaX = 0, _mouseDeltaY = 0;
    float _mouseX = 0, _mouseY = 0;

    SDL_Gamepad* _controller = nullptr;
    std::vector<uint8_t> _currButtons;
    std::vector<uint8_t> _prevButtons;

    int16_t _axes[SDL_GAMEPAD_AXIS_COUNT]{};

    bool _relativeMouseActive = false;
    bool IsKeyHeld(SDL_Scancode aKey) const;
    bool IsKeyPressed(SDL_Scancode aKey) const;
    bool IsKeyReleased(SDL_Scancode aKey) const;

    float GetMouseX() const;
    float GetMouseY() const;
    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;

    bool IsButtonHeld(SDL_GamepadButton aButton) const;
    bool IsButtonJustPressed(SDL_GamepadButton aButton) const;

    float GetAxis(SDL_GamepadAxis aAxis) const;
};

class Input
{
public:
    void Init(SDL_Window* aSDLWindow);
    void PostUpdate();
    void ProcessEvent(const SDL_Event& aE);
    void EndOfFrame();

    // Clears just-pressed/released sets. Call after each fixed update step.
    void FlushKeyEvents();
    void ResetMouseDelta();
    void SetRelativeMouseMode(bool aState);
    // void ToggleRelativeMouseMode();
    const InputData& GetInputDataSnapShot() const;
private:
    InputData _inputData;
    SDL_Window* _SDL_Window;

public: // singleton
    static Input& Instance();

    Input(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(const Input&) = delete;
    Input& operator=(Input&&) = delete;
private:
    Input();
    ~Input();
};
