#pragma once
#include <game/camera.h>

// Host-owned POD: lives in the EXE so it survives DLL reloads; the game writes it only through Game_Update, the engine reads it back directly.
struct GameState
{
    Camera _cameraData;
};