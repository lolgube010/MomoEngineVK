#pragma once
#include "game/game_api.h"
#include <cstdint>
#include <string>

// Host-side handle to the gameplay DLL. Owns the function-pointer table, the loaded
// module, and the hot-reload machinery. The engine only ever calls the game through
// here, so the DLL can be swapped underneath it at runtime without the call sites caring.
class GameModule
{
public:
    bool Load();              // loads a private copy of the built DLL and resolves the table
    void Unload();
    bool Reload();            // swaps in a freshly-built DLL, keeping the old one if the new fails
    bool IsLoaded() const { return _api.Update != nullptr; }

    // Called once by the host. The state pointer + bridge are cached so Reload() can
    // re-run the Game_Init handshake on its own (both stay valid across reloads).
    void Init(GameState* aState, const ImGuiBridge* aBridge);

    void Update(GameState* aState, double aDT, const InputData* aInput) const { _api.Update(aState, aDT, aInput); }
    void DrawImGui(GameState* aState) const { _api.DrawImGui(aState); }

    // Hot-reload controls (host-only; the DLL knows nothing about them).
    void PollAutoReload();              // call once per frame: reloads if enabled and the build changed
    bool SourceChanged() const;         // is the built DLL newer than the one we loaded?
    bool& AutoReloadEnabled() { return _autoReload; }

    // Rebuild from inside the app: spawns the compile, then reloads when it finishes.
    void RequestRebuild();              // kick off an async `cmake --build ... --target MomoGame`
    void PollBuild();                   // call once per frame: reloads when the spawned build succeeds
    bool IsBuilding() const { return _buildProc != nullptr; }
    bool LastBuildFailed() const { return _lastBuildFailed; }

private:
    // Copies the built DLL (and its pdb, redirected) to fresh uniquely-named files and
    // loads + resolves it into the given outputs, without touching the currently-live
    // module. Returns false on any failure (e.g. the build is mid-link and still locked).
    bool LoadCopy(void*& outHandle, GameAPI& outApi, std::string& outPath, std::string& outPdbPath);
    uint64_t QuerySourceWriteTime() const;

    GameAPI     _api{};
    void*       _handle = nullptr;   // HMODULE of the live copy; void* keeps <windows.h> out of the header
    std::string _livePath;           // the dll copy we currently hold open (deleted on unload/reload)
    std::string _livePdbPath;        // its private pdb copy (so the build's pdb stays unlocked)

    GameState*  _state  = nullptr;   // host-owned; stable across reloads
    ImGuiBridge _bridge{};           // cached; its context + allocator stay valid across reloads

    uint64_t _loadedWriteTime = 0;   // write time of the built DLL at our last (re)load
    int      _copyCounter     = 0;   // makes each live-copy filename unique
    bool     _autoReload      = true;

    void* _buildProc       = nullptr; // HANDLE of an in-flight rebuild process (void* keeps <windows.h> out)
    bool  _lastBuildFailed = false;
};
