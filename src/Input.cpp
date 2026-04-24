#include "Input.h"

#include <SDL.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "vk_engine.h"

void Input::Init()
{
    int numKeys;

    _currentState = SDL_GetKeyboardState(&numKeys);
    _previousState.resize(numKeys, 0);

    // Controller Setup (Initialize SDL Subsystem if you haven't already)
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

    // Size controller button arrays to match SDL's max buttons
    _currButtons.resize(SDL_CONTROLLER_BUTTON_MAX, 0);
    _prevButtons.resize(SDL_CONTROLLER_BUTTON_MAX, 0);
}

void Input::PreUpdate()
{
    std::copy_n(_currentState, _previousState.size(), _previousState.begin());
    std::ranges::copy(_currButtons, _prevButtons.begin());
}

void Input::PostUpdate()
{
    SDL_GetRelativeMouseState(&_mouseDeltaX, &_mouseDeltaY);
    SDL_GetMouseState(&_mouseX, &_mouseY); // NEW: Absolute position

    // 3. Update Controller Button States (if a controller is connected)
    if (_controller)
    {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
        {
            _currButtons[i] = SDL_GameControllerGetButton(_controller, static_cast<SDL_GameControllerButton>(i));
        }
    }
}

void Input::ProcessEvent(const SDL_Event& aE)
{
    // Handle Controller Hotplugging
    if (aE.type == SDL_CONTROLLERDEVICEADDED)
    {
        if (!_controller)
        { // Only open if we don't already have one
            _controller = SDL_GameControllerOpen(aE.cdevice.which);
            fmt::print("Controller Connected!\n");
        }
    }
    else if (aE.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        // If the disconnected device is our active controller, close it
        const SDL_GameController* closed = SDL_GameControllerFromInstanceID(aE.cdevice.which);
        if (_controller == closed)
        {
            SDL_GameControllerClose(_controller);
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

int Input::GetMouseX() const
{ return _mouseX; }

int Input::GetMouseY() const
{ return _mouseY; }

int Input::GetMouseDeltaX() const
{ return _mouseDeltaX; }

int Input::GetMouseDeltaY() const
{ return _mouseDeltaY; }

bool Input::IsButtonHeld(const SDL_GameControllerButton aButton) const
{
    return _controller && _currButtons[aButton] != 0;
}

bool Input::IsButtonJustPressed(const SDL_GameControllerButton aButton) const
{
    return _controller && _currButtons[aButton] != 0 && _prevButtons[aButton] == 0;
}

float Input::GetAxis(const SDL_GameControllerAxis aAxis) const
{
    if (!_controller)
        return 0.0f;

    // SDL returns axis values from -32768 to 32767.
    // We normalize this to a float between -1.0f and 1.0f for easier math.
    const int16_t value = SDL_GameControllerGetAxis(_controller, aAxis);

    // Add a small dead-zone to prevent "stick drift"
    if (abs(value) < 8000)
        return 0.0f;

    return static_cast<float>(value) / 32767.0f;
}

Input::~Input()
{
    if (_controller)
    {
        SDL_GameControllerClose(_controller);
    }
}

Input::Input() = default;

Input& Input::Instance()
{
    static Input instance{};
    return instance;
}
