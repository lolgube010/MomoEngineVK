// ReSharper disable CppInconsistentNaming
#pragma once
#include <cstddef> // size_t (for the allocator function-pointer signatures below)

// Marks the DLL's one exported entry point (Game_GetAPI). MOMO_GAME_EXPORTS is defined
// only while building the MomoGame target, so the game exports it and the host (which
// resolves it by name via GetProcAddress) sees a plain declaration with no import dep.
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

// The gameplay module's function table, and the single source of truth for the boundary.
// To add an entry point: add a member here, define it in game_api.cpp, and assign it in
// Game_GetAPI. The host side (resolution in GameModule) never changes. Inline function-
// pointer types avoid maintaining a separate typedef per entry.
struct GameAPI
{
    void (*Init)(GameState* aState, const ImGuiBridge* aImGui);
    void (*Update)(GameState* aState, double aDT, const InputData* aInput);
    void (*DrawImGui)(GameState* aState);
};

// The ONLY exported symbol. The host resolves just this name, calls it, and receives the
// whole table at once; the individual game functions stay internal to the DLL.
GAME_API void Game_GetAPI(GameAPI* aOutApi);

#ifdef __cplusplus
}
#endif
