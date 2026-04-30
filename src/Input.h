#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

class Input
{
public:
    void Init();
    void PreUpdate();
    void PostUpdate();
    void ProcessEvent(const SDL_Event& aE);

    bool IsKeyHeld(SDL_Scancode aKey) const;
    bool IsKeyPressed(SDL_Scancode aKey) const;
    bool IsKeyReleased(SDL_Scancode aKey) const;

    float GetMouseX() const;
    float GetMouseY() const;

    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;

    bool IsButtonHeld(SDL_GamepadButton aButton) const;

    bool IsButtonJustPressed(SDL_GamepadButton aButton) const;

    // --- Controller Axis Queries (Left Stick, Right Stick, Triggers) ---
    float GetAxis(SDL_GamepadAxis aAxis) const;

private:
    // Keyboard
    const bool* _currentState = nullptr;
    std::vector<uint8_t> _previousState;

    // Mouse
    float _mouseDeltaX = 0, _mouseDeltaY = 0;
    float _mouseX = 0, _mouseY = 0;

    // Controller
    SDL_Gamepad* _controller = nullptr;
    std::vector<uint8_t> _currButtons;
    std::vector<uint8_t> _prevButtons;

public: // singleton slop
    static Input& Instance();
    
    Input(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(const Input&) = delete;
    Input& operator=(Input&&) = delete;
private: 
    Input();
    ~Input();
};
