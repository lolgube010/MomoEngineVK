#pragma once
#include <vk_types.h>
#include <SDL_events.h>

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

    void Update();
    bool _isLocked = false;
    float _tempCameraFov = 90.f;
};
