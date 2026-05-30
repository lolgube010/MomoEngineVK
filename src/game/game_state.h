#pragma once
#include "camera.h"

// struct InputData;
//
// class GameState
// {
// public:
//     void Update(double aDT, const InputData& aInputData);
//     void Update_Camera(Camera& aCameraPOV, float aDt, const InputData& aInputData) const;
//
//     const Camera& GetCameraData() const;
//     Camera& GetCameraDataMutable();
//     
// private:
//     Camera _cameraData;
// };

struct GameState
{
public:
    Camera _cameraData;
};