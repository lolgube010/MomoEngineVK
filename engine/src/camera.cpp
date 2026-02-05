#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

glm::mat4 Camera::GetViewMatrix() const
{
    // to create a correct model view, we need to move the world in opposite direction to the camera so we will create the camera model matrix and invert
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 cameraRotation = GetRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::GetRotationMatrix() const
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into the final rotation matrix
    const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::ProcessSDLEvent(const SDL_Event& aE)
{
    const auto& key = aE.key.keysym.sym;
    if (aE.type == SDL_KEYDOWN) 
    {
        if (aE.key.repeat == 0)
        {
	        if (key == SDLK_w) { velocity.z += -1; }
	        if (key == SDLK_s) { velocity.z += 1; }
	        if (key == SDLK_a) { velocity.x += -1; }
	        if (key == SDLK_d) { velocity.x += 1; }
        }
    }

    if (aE.type == SDL_KEYUP) 
    {
        if (key == SDLK_w) { velocity.z -= -1; }
        if (key == SDLK_s) { velocity.z -= 1; }
        if (key == SDLK_a) { velocity.x -= -1; }
        if (key == SDLK_d) { velocity.x -= 1; }
    }

    if (aE.type == SDL_MOUSEMOTION) 
    {
        yaw += static_cast<float>(aE.motion.xrel) / 200.f;
        pitch -= static_cast<float>(aE.motion.yrel) / 200.f;

        constexpr float maxMinPitch = 0.90f;
        pitch = glm::clamp(pitch, -maxMinPitch, maxMinPitch);
    }
}

// TODO:
// Movement in this code is frame-dependant, as we aren't taking the speed of the engine into account. This is done for simplicity in the case, if you want to improve it, you would need to pass deltaTime (time between frames) to the update() function, and multiply the velocity by that. In the tutorial, we are more or less FPS locked to monitor speed due to the options we have used in the swapchain, and we aren't rendering enough data to slow down the engine.
void Camera::Update()
{
	const glm::mat4 cameraRotation = GetRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}
