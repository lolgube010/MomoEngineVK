#pragma once
#include <SDL_events.h>
#include <SDL_keycode.h>

class Input
{
public:
    void Init();
    void Update();
    void ProcessEvent(const SDL_Event& aE, bool& aQuit);

    bool IsKeyHeld(SDL_Scancode aKey) const;
    bool IsKeyPressed(SDL_Scancode aKey) const;
    bool IsKeyReleased(SDL_Scancode aKey) const;
    
    int GetMouseX() const;
    int GetMouseY() const;

    int GetMouseDeltaX() const;
    int GetMouseDeltaY() const;

    bool IsButtonHeld(const SDL_GameControllerButton aButton) const;

    bool IsButtonJustPressed(const SDL_GameControllerButton aButton) const;

    // --- Controller Axis Queries (Left Stick, Right Stick, Triggers) ---
    float GetAxis(const SDL_GameControllerAxis aAxis) const;

private: 
    // Keyboard
    const Uint8* _currentState = nullptr;
    std::vector<Uint8> _previousState;

    // Mouse
    int _mouseDeltaX = 0, _mouseDeltaY = 0;
    int _mouseX = 0, _mouseY = 0;

    // Controller
    SDL_GameController* _controller = nullptr;
    std::vector<Uint8> _currButtons;
    std::vector<Uint8> _prevButtons;

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
