#pragma once
#include <cstdint>
#include <SDL3/SDL_events.h>

// POD snapshot: trivially copyable so it can cross a DLL boundary by value.
// No heap members, no borrowed pointers, no SDL handles. The engine-side Input
// owns those and fills this each frame.
struct InputData
{
    bool _keyCurr[SDL_SCANCODE_COUNT]{};
    bool _keyPrev[SDL_SCANCODE_COUNT]{};

    float _mouseDeltaX = 0, _mouseDeltaY = 0;
    float _mouseX = 0, _mouseY = 0;

    bool _hasController = false;
    uint8_t _currButtons[SDL_GAMEPAD_BUTTON_COUNT]{};
    uint8_t _prevButtons[SDL_GAMEPAD_BUTTON_COUNT]{};
    int16_t _axes[SDL_GAMEPAD_AXIS_COUNT]{};

    bool _relativeMouseActive = false;

    // Header-inline so the game module compiles its own copy (no cross-boundary link).
    bool IsKeyHeld(const SDL_Scancode aKey) const { return _keyCurr[aKey]; }
    bool IsKeyPressed(const SDL_Scancode aKey) const { return _keyCurr[aKey] && !_keyPrev[aKey]; }
    bool IsKeyReleased(const SDL_Scancode aKey) const { return !_keyCurr[aKey] && _keyPrev[aKey]; }

    float GetMouseX() const { return _mouseX; }
    float GetMouseY() const { return _mouseY; }
    float GetMouseDeltaX() const { return _mouseDeltaX; }
    float GetMouseDeltaY() const { return _mouseDeltaY; }

    bool IsButtonHeld(const SDL_GamepadButton aButton) const { return _hasController && _currButtons[aButton] != 0; }
    bool IsButtonJustPressed(const SDL_GamepadButton aButton) const { return _hasController && _currButtons[aButton] != 0 && _prevButtons[aButton] == 0; }

    float GetAxis(const SDL_GamepadAxis aAxis) const
    {
        if (!_hasController)
        {
            return 0.0f;
        }
        const int16_t value = _axes[aAxis];
        if (value > -8000 && value < 8000)
        {
            return 0.0f;
        }
        return static_cast<float>(value) / 32767.0f;
    }
};

class Input
{
public:
    void Init(SDL_Window* aSDLWindow);
    void PostUpdate();
    void ProcessEvent(const SDL_Event& aE);
    void EndOfFrame();

    void ResetMouseDelta();
    void SetRelativeMouseMode(bool aState);
    const InputData& GetInputDataSnapShot() const;
private:
    InputData _inputData;

    // Engine-only state, kept out of the POD snapshot.
    SDL_Window* _SDL_Window = nullptr;
    SDL_Gamepad* _controller = nullptr;
    const bool* _keyboardState = nullptr;  // borrowed SDL keyboard array

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
