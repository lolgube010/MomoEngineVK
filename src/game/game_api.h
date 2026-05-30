// ReSharper disable CppInconsistentNaming
#pragma once
#include <cstddef> // size_t (for the allocator function-pointer signatures below)

// Marks the DLL's entry points. MOMO_GAME_EXPORTS is defined only while building the
// MomoGame target, so the game exports them and the host (which resolves them by name
// via GetProcAddress) sees plain declarations with no import dependency.
#ifdef MOMO_GAME_EXPORTS
    #define GAME_API __declspec(dllexport)
#else
    #define GAME_API
#endif

// Forward declarations so the API doesn't force heavy includes on the host
struct GameState;
struct InputData;
struct ImGuiContext;

// Everything ImGui needs to work from inside a separate module. The host fills this
// once and hands it to Game_Init; the game then points its own (per-module) ImGui
// globals at the host's, so both share one context and one heap. The function-pointer
// members are declared raw (not via imgui.h's typedefs) to keep this header dependency
// free, but the signatures match ImGuiMemAllocFunc / ImGuiMemFreeFunc exactly.
struct ImGuiBridge
{
    ImGuiContext* ctx                              = nullptr;
    void* (*allocFunc)(size_t sz, void* userData)  = nullptr;
    void  (*freeFunc)(void* ptr, void* userData)   = nullptr;
    void* userData                                 = nullptr;
};

#ifdef __cplusplus
extern "C"
{
#endif

// The exported entry points, defined in game_api.cpp. Today the host calls these
// directly (static link). At the DLL stage the host stops linking them and instead
// resolves them by name via GetProcAddress into the function-pointer table below.
GAME_API void Game_Init(GameState* aState, const ImGuiBridge* aImGui);
GAME_API void Game_Update(GameState* aState, double aDT, const InputData* aInput);
GAME_API void Game_DrawImGui(GameState* aState);

// The Function Pointer Types. These MUST match the Game_* signatures above
// exactly; GetProcAddress can't check it for you.
typedef void (*GameInit_t)(GameState* aState, const ImGuiBridge* aImGui);
typedef void (*GameUpdate_t)(GameState* aState, double aDT, const InputData* aInput);
typedef void (*GameDrawImGui_t)(GameState* aState);

// The Function Table (The Seam)
struct GameAPI
{
    GameInit_t Init;
    GameUpdate_t Update;
    GameDrawImGui_t DrawImGui;
};

#ifdef __cplusplus
}
#endif
