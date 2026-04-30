#include <Input.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <vk/engine.h>

void Input::Init()
{
    int numKeys;

    _currentState = SDL_GetKeyboardState(&numKeys);
    _previousState.resize(numKeys, 0);

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);

    _currButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
    _prevButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
}

void Input::PreUpdate()
{
    std::copy_n(_currentState, _previousState.size(), _previousState.begin());
    std::ranges::copy(_currButtons, _prevButtons.begin());
}

void Input::PostUpdate()
{
    SDL_GetRelativeMouseState(&_mouseDeltaX, &_mouseDeltaY);
    SDL_GetMouseState(&_mouseX, &_mouseY);

    if (_controller)
    {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        {
            _currButtons[i] = SDL_GetGamepadButton(_controller, static_cast<SDL_GamepadButton>(i));
        }
    }
}

void Input::ProcessEvent(const SDL_Event& aE)
{
    if (aE.type == SDL_EVENT_GAMEPAD_ADDED)
    {
        if (!_controller)
        {
            _controller = SDL_OpenGamepad(aE.gdevice.which);
            fmt::print("Controller Connected!\n");
        }
    }
    else if (aE.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        SDL_Gamepad* closed = SDL_GetGamepadFromID(aE.gdevice.which);
        if (_controller == closed)
        {
            SDL_CloseGamepad(_controller);
            _controller = nullptr;
            fmt::print("Controller Disconnected!\n");
        }
    }
}

bool Input::IsKeyHeld(const SDL_Scancode aKey) const
{
    return _currentState[aKey] != 0;
}

bool Input::IsKeyPressed(const SDL_Scancode aKey) const
{
    return _currentState[aKey] != 0 && _previousState[aKey] == 0;
}

bool Input::IsKeyReleased(const SDL_Scancode aKey) const
{
    return _currentState[aKey] == 0 && _previousState[aKey] != 0;
}

float Input::GetMouseX() const
{ return _mouseX; }

float Input::GetMouseY() const
{ return _mouseY; }

float Input::GetMouseDeltaX() const
{ return _mouseDeltaX; }

float Input::GetMouseDeltaY() const
{ return _mouseDeltaY; }

bool Input::IsButtonHeld(const SDL_GamepadButton aButton) const
{
    return _controller && _currButtons[aButton] != 0;
}

bool Input::IsButtonJustPressed(const SDL_GamepadButton aButton) const
{
    return _controller && _currButtons[aButton] != 0 && _prevButtons[aButton] == 0;
}

float Input::GetAxis(const SDL_GamepadAxis aAxis) const
{
    if (!_controller)
        return 0.0f;

    // SDL returns axis values from -32768 to 32767.
    // We normalize this to a float between -1.0f and 1.0f for easier math.
    const int16_t value = SDL_GetGamepadAxis(_controller, aAxis);

    // Add a small dead-zone to prevent "stick drift"
    if (abs(value) < 8000)
        return 0.0f;

    return static_cast<float>(value) / 32767.0f;
}

Input::~Input()
{
    if (_controller)
    {
        SDL_CloseGamepad(_controller);
    }
}

Input::Input() = default;

Input& Input::Instance()
{
    static Input instance{};
    return instance;
}
