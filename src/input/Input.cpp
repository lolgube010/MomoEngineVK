#include <input/Input.h>

#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <engine_main/engine.h>

void Input::Init(SDL_Window* aSDLWindow)
{
    _keyboardState = SDL_GetKeyboardState(nullptr);

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);

    _SDL_Window = aSDLWindow;
    _inputData._relativeMouseActive = SDL_GetWindowRelativeMouseMode(_SDL_Window);
}

void Input::PostUpdate()
{
    float dx, dy;
    SDL_GetRelativeMouseState(&dx, &dy);
    _inputData._mouseDeltaX += dx;
    _inputData._mouseDeltaY += dy;
    SDL_GetMouseState(&_inputData._mouseX, &_inputData._mouseY);

    // Sample held state into curr; edges (curr vs prev) advance in EndOfFrame.
    std::memcpy(_inputData._keyCurr, _keyboardState, sizeof(_inputData._keyCurr));

    _inputData._hasController = (_controller != nullptr);
    if (_controller)
    {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        {
            _inputData._currButtons[i] = SDL_GetGamepadButton(_controller, static_cast<SDL_GamepadButton>(i));
        }

        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
        {
            _inputData._axes[i] = SDL_GetGamepadAxis(_controller, static_cast<SDL_GamepadAxis>(i));
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
        const SDL_Gamepad* closed = SDL_GetGamepadFromID(aE.gdevice.which);
        if (_controller == closed)
        {
            SDL_CloseGamepad(_controller);
            _controller = nullptr;
            fmt::print("Controller Disconnected!\n");
        }
    }
}

void Input::EndOfFrame()
{
    // Advance edges: this tick's state becomes last tick's, so the next tick's
    // IsKeyPressed/IsButtonJustPressed compare against it. Done per fixed step
    // so an edge fires once even when several substeps run in one frame.
    std::memcpy(_inputData._keyPrev, _inputData._keyCurr, sizeof(_inputData._keyCurr));
    std::memcpy(_inputData._prevButtons, _inputData._currButtons, sizeof(_inputData._currButtons));
    ResetMouseDelta();
}

void Input::ResetMouseDelta()
{
    _inputData._mouseDeltaX = 0.f;
    _inputData._mouseDeltaY = 0.f;
}

void Input::SetRelativeMouseMode(const bool aState)
{
    if (aState == _inputData._relativeMouseActive)
    {
        return;
    }
    _inputData._relativeMouseActive = aState;
    SDL_SetWindowRelativeMouseMode(_SDL_Window, aState);
}

const InputData& Input::GetInputDataSnapShot() const
{
    return _inputData;
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
