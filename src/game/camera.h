#pragma once

struct InputData;
struct SDL_Window;

struct Camera_POV
{
    glm::vec3 _velocity = {};
    // vertical rotation
    float _pitch = 0.f;
    glm::vec3 _position = {0.f, 5.f, 0.f};
    // horizontal rotation
    float _yaw = 0.f;
    float _cameraFov = 90.f;
    bool _isLocked = false;
    bool _wantMouseCaptured = false;
};

class Camera
{
public:
    Camera_POV _camData;

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetRotationMatrix() const;
    glm::mat4 GetProjectionMatrix(float aWidth, float aHeight) const;

    void Update(float aDt, const InputData& aInputData);
};
