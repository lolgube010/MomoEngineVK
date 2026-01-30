#pragma once
#include <vk_types.h>
#include <SDL_events.h>

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch{ 0.f };
    // horizontal rotation
    float yaw{ 0.f };

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetRotationMatrix() const;

    void ProcessSDLEvent(const SDL_Event& aE);

    void Update();

};
