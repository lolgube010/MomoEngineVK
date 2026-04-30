#include <Input.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <engine.h>

void Input::Init()
{
    int numKeys;
    _currentState = SDL_GetKeyboardState(&numKeys);

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);

    _currButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
    _prevButtons.resize(SDL_GAMEPAD_BUTTON_COUNT, 0);
}

void Input::PostUpdate()
{
    float dx, dy;
    SDL_GetRelativeMouseState(&dx, &dy);
    _mouseDeltaX += dx;
    _mouseDeltaY += dy;
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
    if (aE.type == SDL_EVENT_KEY_DOWN && !aE.key.repeat)
    {
        _justPressed.insert(aE.key.scancode);
    }
    else if (aE.type == SDL_EVENT_KEY_UP)
    {
        _justReleased.insert(aE.key.scancode);
    }
    else if (aE.type == SDL_EVENT_GAMEPAD_ADDED)
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

void Input::FlushKeyEvents()
{
    _justPressed.clear();
    _justReleased.clear();
}

bool Input::IsKeyHeld(const SDL_Scancode aKey) const
{
    return _currentState[aKey] != 0;
}

bool Input::IsKeyPressed(const SDL_Scancode aKey) const
{
    return _justPressed.contains(aKey);
}

bool Input::IsKeyReleased(const SDL_Scancode aKey) const
{
    return _justReleased.contains(aKey);
}

float Input::GetMouseX() const
{ return _mouseX; }

float Input::GetMouseY() const
{ return _mouseY; }

float Input::GetMouseDeltaX() const
{ return _mouseDeltaX; }

float Input::GetMouseDeltaY() const
{ return _mouseDeltaY; }

void Input::ResetMouseDelta()
{
    _mouseDeltaX = 0.f;
    _mouseDeltaY = 0.f;
}

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

    const int16_t value = SDL_GetGamepadAxis(_controller, aAxis);

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
