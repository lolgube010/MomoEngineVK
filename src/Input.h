#pragma once
#include <unordered_set>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

class Input
{
public:
    void Init();
    void PostUpdate();
    void ProcessEvent(const SDL_Event& aE);

    bool IsKeyHeld(SDL_Scancode aKey) const;
    bool IsKeyPressed(SDL_Scancode aKey) const;
    bool IsKeyReleased(SDL_Scancode aKey) const;
    // Clears just-pressed/released sets. Call after each fixed update step.
    void FlushKeyEvents();

    float GetMouseX() const;
    float GetMouseY() const;
    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;
    void ResetMouseDelta();

    bool IsButtonHeld(SDL_GamepadButton aButton) const;
    bool IsButtonJustPressed(SDL_GamepadButton aButton) const;

    float GetAxis(SDL_GamepadAxis aAxis) const;

private:
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
