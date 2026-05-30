#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>

struct Camera
{
    glm::vec3 _velocity = {};
    // vertical rotation
    float _pitch = 0.f;
    glm::vec3 _position = {0.f, 5.f, 0.f};
    // horizontal rotation
    float _yaw = 0.f;
    float _cameraFOV = 90.f;
    bool _isLocked = false;
    bool _wantMouseCaptured = false;
    const float _mouseSensitivity = 0.005f;
    const float _maxPitch = 1.50f;
    const float _moveSpeed = 30.0f; // units/second (was 0.5f/frame * 60fps)
};

namespace Momo_CameraUtil
{
    inline glm::mat4 get_rotation_matrix(const Camera& aCamera)
    {
        const glm::quat pitchRotation = glm::angleAxis(aCamera._pitch, glm::vec3{1.f, 0.f, 0.f});
        const glm::quat yawRotation = glm::angleAxis(aCamera._yaw, glm::vec3{0.f, -1.f, 0.f});

        return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

    }
}
