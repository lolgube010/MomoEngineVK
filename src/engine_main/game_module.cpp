#include "game_module.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>

namespace
{
    // Directory of the running exe, with a trailing separator. CopyFile / GetFileAttributes
    // resolve relative paths against the current WORKING directory, which is NOT the exe
    // folder when launched from the debugger. LoadLibrary searches the exe dir on its own,
    // which is why loading by bare name worked but copying by bare name doesn't.
    std::string ExeDir()
    {
        char buf[MAX_PATH];
        const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        const std::string path(buf, n);
        const size_t slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
    }

    std::string SourceDllPath() { return ExeDir() + "MomoGame.dll"; } // build output, next to the exe
    std::string SourcePdbPath() { return ExeDir() + "MomoGame.pdb"; }

    // The in-app rebuild should target the same config the host was built in.
#ifdef NDEBUG
    constexpr const char* kBuildConfig = "Release";
#else
    constexpr const char* kBuildConfig = "Debug";
#endif

    bool FileExists(const std::string& aPath)
    {
        return GetFileAttributesA(aPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    // A 12-char pdb filename, the SAME length as "MomoGame.pdb", so it can overwrite the
    // path embedded in the copied DLL in place. 3 digits wrap every 1000 reloads, which is
    // fine since stale copies are deleted as we go.
    std::string LivePdbName(int aIndex)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "MomoG%03d.pdb", aIndex % 1000);
        return name;
    }

    // Overwrite the build's pdb filename embedded in the DLL's debug directory with the
    // per-load copy's name (equal length, so the directory prefix and everything after stay
    // put). The debugger then loads the copy's pdb and leaves the build's MomoGame.pdb
    // unlocked, so the next rebuild can overwrite it even while a debugger is attached.
    void RedirectEmbeddedPdb(const std::string& aDllPath, const std::string& aNewPdbName)
    {
        std::fstream f(aDllPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!f)
        {
            return;
        }
        const std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        static const std::string needle = "MomoGame.pdb"; // 12 chars; matches LivePdbName length
        const auto it = std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end());
        if (it == bytes.end())
        {
            return; // no embedded pdb path (e.g. a release build without debug info)
        }

        f.clear(); // reading to EOF set eofbit; clear before repositioning to write
        f.seekp(it - bytes.begin(), std::ios::beg);
        f.write(aNewPdbName.data(), static_cast<std::streamsize>(aNewPdbName.size()));
    }
}

bool GameModule::LoadCopy(void*& outHandle, GameAPI& outApi, std::string& outPath, std::string& outPdbPath)
{
    // Load a COPY, never the build output itself: while a DLL is held open the linker can't
    // overwrite it, so the next rebuild would fail. Each copy gets a unique name so a reload
    // never has to overwrite the file it is currently running from.
    const int index = _copyCounter++;
    const std::string copyPath = ExeDir() + "MomoGame_live_" + std::to_string(index) + ".dll";

    if (!CopyFileA(SourceDllPath().c_str(), copyPath.c_str(), FALSE))
    {
        return false; // build may be mid-link and holding the source locked; caller retries later
    }

    // Give the copy its own pdb and point it there, so the build's MomoGame.pdb is never
    // locked by the debugger. Skipped cleanly if there's no pdb (e.g. release build).
    std::string pdbCopyPath;
    if (FileExists(SourcePdbPath()))
    {
        const std::string pdbName = LivePdbName(index);
        pdbCopyPath = ExeDir() + pdbName;
        CopyFileA(SourcePdbPath().c_str(), pdbCopyPath.c_str(), FALSE);
        RedirectEmbeddedPdb(copyPath, pdbName);
    }

    const HMODULE h = LoadLibraryA(copyPath.c_str());
    if (!h)
    {
        DeleteFileA(copyPath.c_str());
        if (!pdbCopyPath.empty()) { DeleteFileA(pdbCopyPath.c_str()); }
        return false;
    }

    // One symbol to resolve: it fills the whole table. Adding game entry points never
    // touches this code again.
    using GameGetAPI_t = void (*)(GameAPI*);
    const auto getApi = reinterpret_cast<GameGetAPI_t>(GetProcAddress(h, "Game_GetAPI"));
    if (!getApi)
    {
        FreeLibrary(h);
        DeleteFileA(copyPath.c_str());
        if (!pdbCopyPath.empty()) { DeleteFileA(pdbCopyPath.c_str()); }
        return false;
    }

    GameAPI api{};
    getApi(&api);
    if (!api.Init || !api.Update || !api.DrawImGui)
    {
        FreeLibrary(h);
        DeleteFileA(copyPath.c_str());
        if (!pdbCopyPath.empty()) { DeleteFileA(pdbCopyPath.c_str()); }
        return false;
    }

    outHandle  = h;
    outApi     = api;
    outPath    = copyPath;
    outPdbPath = pdbCopyPath;
    return true;
}

bool GameModule::Load()
{
    void*       handle = nullptr;
    GameAPI     api{};
    std::string path;
    std::string pdbPath;
    if (!LoadCopy(handle, api, path, pdbPath))
    {
        return false;
    }

    _handle         = handle;
    _api            = api;
    _livePath       = path;
    _livePdbPath    = pdbPath;
    _lastSourceTime = QueryGameSourceTime(); // baseline so we don't rebuild on startup
    return true;
}

void GameModule::Unload()
{
    if (_handle)
    {
        FreeLibrary(static_cast<HMODULE>(_handle));
        _handle = nullptr;
    }
    if (!_livePath.empty())
    {
        DeleteFileA(_livePath.c_str()); // now unlocked; clean up the copy
        _livePath.clear();
    }
    if (!_livePdbPath.empty())
    {
        DeleteFileA(_livePdbPath.c_str());
        _livePdbPath.clear();
    }
    _api = {};
}

bool GameModule::Reload()
{
    // Load the new DLL BEFORE freeing the current one. If the build is mid-link (source
    // locked) or the fresh DLL is broken, LoadCopy fails and we keep running the old one.
    void*       newHandle = nullptr;
    GameAPI     newApi{};
    std::string newPath;
    std::string newPdbPath;
    if (!LoadCopy(newHandle, newApi, newPath, newPdbPath))
    {
        return false;
    }

    // New module is good — retire the old one and swap in the new table.
    if (_handle)
    {
        FreeLibrary(static_cast<HMODULE>(_handle));
    }
    if (!_livePath.empty())    { DeleteFileA(_livePath.c_str()); }
    if (!_livePdbPath.empty()) { DeleteFileA(_livePdbPath.c_str()); }

    _handle      = newHandle;
    _api         = newApi;
    _livePath    = newPath;
    _livePdbPath = newPdbPath;

    // Fresh module = fresh ImGui globals (and any game-side statics reset), so the
    // handshake must run again. GameState lives in the host, so its contents carry over.
    if (_state)
    {
        _api.Init(_state, &_bridge);
    }
    return true;
}

void GameModule::Init(GameState* aState, const ImGuiBridge* aBridge)
{
    _state  = aState;
    _bridge = *aBridge; // POD copy; the host keeps owning the context + allocator it points at
    _api.Init(_state, &_bridge);
}

void GameModule::PollAutoRebuild()
{
    if (!_autoRebuild || _buildProc)
    {
        return; // disabled, or a build is already running (PollBuild handles its completion)
    }
    const uint64_t now = QueryGameSourceTime();
    if (now != 0 && now != _lastSourceTime)
    {
        // A game source file was saved. Record it first so a failed compile (or a save
        // mid-build) retries cleanly, then kick off the build; PollBuild reloads on success.
        _lastSourceTime = now;
        RequestRebuild();
    }
}

void GameModule::RequestRebuild()
{
    if (_buildProc)
    {
        return; // a build is already in flight
    }

    // Build dir sits two levels up from the exe (<repo>/bin/<config> -> <repo>/build).
    const std::string buildDir = ExeDir() + "..\\..\\build";
    const std::string cmd = "cmake --build \"" + buildDir + "\" --config " + kBuildConfig + " --target MomoGame";

    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0'); // CreateProcessA needs a writable, null-terminated buffer

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // lpApplicationName == null -> the first token ("cmake") is resolved via PATH.
    // CREATE_NO_WINDOW keeps the compile headless; we surface only pass/fail in the UI.
    if (CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread); // only the process is tracked
        _buildProc       = pi.hProcess;
        _lastBuildFailed = false;
    }
    else
    {
        _lastBuildFailed = true; // couldn't launch cmake (not on PATH?)
    }
}

void GameModule::PollBuild()
{
    if (!_buildProc)
    {
        return;
    }
    const HANDLE proc = static_cast<HANDLE>(_buildProc);
    if (WaitForSingleObject(proc, 0) != WAIT_OBJECT_0)
    {
        return; // still compiling; check again next frame
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(proc, &exitCode);
    CloseHandle(proc);
    _buildProc = nullptr;

    if (exitCode == 0)
    {
        Reload(); // new DLL is on disk; swap it in (this also re-runs the ImGui handshake)
    }
    _lastBuildFailed = (exitCode != 0);
}

uint64_t GameModule::QueryGameSourceTime() const
{
    namespace fs = std::filesystem;

    // The game sources live at <repo>/src/game (two levels up from <repo>/bin/<config>).
    // Absent in a shipped build -> return 0 so the watcher simply does nothing.
    const fs::path dir = fs::path(ExeDir()) / ".." / ".." / "src" / "game";

    std::error_code ec;
    if (!fs::exists(dir, ec))
    {
        return 0;
    }

    fs::file_time_type newest{};
    bool found = false;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec))
    {
        if (ec) { break; }
        if (!entry.is_regular_file()) { continue; }
        const fs::path ext = entry.path().extension();
        if (ext != ".cpp" && ext != ".h") { continue; }

        const fs::file_time_type t = fs::last_write_time(entry, ec);
        if (ec) { continue; }
        if (!found || t > newest) { newest = t; found = true; }
    }

    return found ? static_cast<uint64_t>(newest.time_since_epoch().count()) : 0;
}
