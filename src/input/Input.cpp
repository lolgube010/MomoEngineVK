#include <input/Input.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <engine_main/engine.h>

void Input::PostUpdate()
{
    float dx, dy;
    SDL_GetRelativeMouseState(&dx, &dy);
    _inputData._mouseDeltaX += dx;
    _inputData._mouseDeltaY += dy;
    SDL_GetMouseState(&_inputData._mouseX, &_inputData._mouseY);

    if (_inputData._controller)
    {
        // Advance edges: this frame's buttons become last frame's before re-sampling,
        // so IsButtonJustPressed can compare curr vs prev.
        _inputData._prevButtons = _inputData._currButtons;

        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        {
            _inputData._currButtons[i] = SDL_GetGamepadButton(_inputData._controller, static_cast<SDL_GamepadButton>(i));
        }

        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
        {
            _inputData._axes[i] = SDL_GetGamepadAxis(_inputData._controller, static_cast<SDL_GamepadAxis>(i));
        }
    }
}

void Input::ProcessEvent(const SDL_Event& aE)
{
    if (aE.type == SDL_EVENT_KEY_DOWN && !aE.key.repeat)
    {
        _inputData._justPressed.insert(aE.key.scancode);
    }
    else if (aE.type == SDL_EVENT_KEY_UP)
    {
        _inputData._justReleased.insert(aE.key.scancode);
    }
    else if (aE.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        if (!_inputData._controller)
        {
            _inputData._controller = SDL_OpenGamepad(aE.gdevice.which);
            fmt::print("Controller Connected!\n");
        }
    }
    else if (aE.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        const SDL_Gamepad* closed = SDL_GetGamepadFromID(aE.gdevice.which);
        if (_inputData._controller == closed)
        {
            SDL_CloseGamepad(_inputData._controller);
            _inputData._controller = nullptr;
            fmt::print("Controller Disconnected!\n");
        }
    }
}

void Input::EndOfFrame()
{
    FlushKeyEvents();
    ResetMouseDelta();
}

void Input::FlushKeyEvents()
{
    _inputData._justPressed.clear();
    _inputData._justReleased.clear();
}

bool InputData::IsKeyHeld(const SDL_Scancode aKey) const
{
    return _currentState[aKey] != 0;
}

bool InputData::IsKeyPressed(const SDL_Scancode aKey) const
{
    return _justPressed.contains(aKey);
}

bool InputData::IsKeyReleased(const SDL_Scancode aKey) const
{
    return _justReleased.contains(aKey);
}

float InputData::GetMouseX() const
{
    return _mouseX;
}

float InputData::GetMouseY() const
{
    return _mouseY;
}

float InputData::GetMouseDeltaX() const
{
    return _mouseDeltaX;
}

float InputData::GetMouseDeltaY() const
{
    return _mouseDeltaY;
}

bool InputData::IsButtonHeld(const SDL_GamepadButton aButton) const
{
    return _controller && _currButtons[aButton] != 0;
}

bool InputData::IsButtonJustPressed(const SDL_GamepadButton aButton) const
{
    return _controller && _currButtons[aButton] != 0 && _prevButtons[aButton] == 0;
}

float InputData::GetAxis(const SDL_GamepadAxis aAxis) const
{
    if (!_controller)
    {
        return 0.0f;
    }

    const int16_t value = _axes[aAxis];

    if (abs(value) < 8000)
    {
        return 0.0f;
    }

    return static_cast<float>(value) / 32767.0f;
}

void Input::Init(SDL_Window* aSDLWindow)
{
    int numKeys;
    _inputData._currentState = SDL_GetKeyboardState(&numKeys);

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);

    _inputData._currButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
    _inputData._prevButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
    _SDL_Window = aSDLWindow;
}


void Input::ResetMouseDelta()
{
    _inputData._mouseDeltaX = 0.f;
    _inputData._mouseDeltaY = 0.f;
}

void Input::SetRelativeMouseMode(const bool aState) const
{
    SDL_SetWindowRelativeMouseMode(_SDL_Window, aState);
}

void Input::ToggleRelativeMouseMode() const
{
    SDL_SetWindowRelativeMouseMode(_SDL_Window, !SDL_GetWindowRelativeMouseMode(_SDL_Window));
}

const InputData& Input::GetInputDataSnapShot() const
{
    return _inputData;
}

Input::~Input()
{
    if (_inputData._controller)
    {
        SDL_CloseGamepad(_inputData._controller);
    }
}

Input::Input() = default;

Input& Input::Instance()
{
    static Input instance{};
    return instance;
}
