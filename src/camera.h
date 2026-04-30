#pragma once
#include <vk/types.h>
#include <SDL3/SDL_events.h>

struct SDL_Window;

class Camera
{
public:
    glm::vec3 _velocity;
    // vertical rotation
    float _pitch{0.f};
    glm::vec3 _position;
    // horizontal rotation
    float _yaw{0.f};

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetRotationMatrix() const;
    glm::mat4 GetProjectionMatrix(float aWidth, float aHeight) const;

    void Update(SDL_Window* aWindow);
    bool _isLocked = false;
    float _tempCameraFov = 90.f;
};
