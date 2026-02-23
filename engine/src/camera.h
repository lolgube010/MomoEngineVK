#pragma once
#include <vk_types.h>
#include <SDL_events.h>

class Camera 
{
public:
    glm::vec3 velocity;
    // vertical rotation
    float pitch{ 0.f };
    glm::vec3 position;
    // horizontal rotation
    float yaw{ 0.f };

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetRotationMatrix() const;

    void ProcessSDLEvent(const SDL_Event& aE);

    void Update();
    bool isLocked = false;
};
