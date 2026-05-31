#include <engine_main/game_module.h>
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>

namespace
{
    // Single source of truth for the gameplay module's name. Must match the CMake target
    // (add_library(MomoGame ...)); the dll/pdb/live-copy names below are all derived from it.
    constexpr const char* kModuleName = "MomoGame";

    // The in-app rebuild should target the same config the host was built in.
#ifdef NDEBUG
    constexpr const char* kBuildConfig = "Release";
#else
    constexpr const char* kBuildConfig = "Debug";
#endif

    // Directory of the running exe, with a trailing separator. CopyFile / GetFileAttributes
    // resolve relative paths against the current WORKING directory, which is NOT the exe
    // folder when launched from the debugger. LoadLibrary searches the exe dir on its own,
    // which is why loading by bare name worked but copying by bare name doesn't. Cached: the
    // exe path is fixed for the process lifetime, and this is hit on the per-frame poll.
    const std::string& ExeDir()
    {
        static const std::string dir = []
        {
            char buf[MAX_PATH];
            const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            const std::string path(buf, n);
            const size_t slash = path.find_last_of("\\/");
            return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
        }();
        return dir;
    }

    // Repo root: the exe lives at <repo>/bin/<config>, so two levels up. One place that
    // encodes the output-dir depth, shared by the source watcher and the rebuild command.
    std::string RepoRoot() { return ExeDir() + "..\\..\\"; }

    std::string SourceDllPath() { return ExeDir() + kModuleName + ".dll"; } // build output, next to the exe
    std::string SourcePdbPath() { return ExeDir() + kModuleName + ".pdb"; }

    bool FileExists(const std::string& aPath)
    {
        return GetFileAttributesA(aPath.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    // A pdb filename the SAME byte length as "<module>.pdb", so it can overwrite the path
    // embedded in the copied DLL in place (directory prefix and trailing bytes stay put).
    // The length is kept identical by replacing the module name's last 3 chars with a
    // zero-padded counter, so this stays correct if kModuleName ever changes. Wraps every
    // 1000 reloads, which is fine since stale copies are deleted as we go.
    std::string LivePdbName(int aIndex)
    {
        std::string base = kModuleName;
        char counter[4];
        std::snprintf(counter, sizeof(counter), "%03d", aIndex % 1000);
        base.replace(base.size() - 3, 3, counter);
        return base + ".pdb";
    }

    // Unload a module and delete its private dll + pdb copies. Safe with null/empty args,
    // so it serves every teardown path (load failure, unload, reload-retire-old).
    void RetireModule(void* aHandle, const std::string& aDllPath, const std::string& aPdbPath)
    {
        if (aHandle)           { FreeLibrary(static_cast<HMODULE>(aHandle)); }
        if (!aDllPath.empty()) { DeleteFileA(aDllPath.c_str()); }
        if (!aPdbPath.empty()) { DeleteFileA(aPdbPath.c_str()); }
    }

    // Overwrite the build's pdb filename embedded in the DLL's debug directory with the
    // per-load copy's name (equal length, see LivePdbName). The debugger then loads the
    // copy's pdb and leaves the build's pdb unlocked, so the next rebuild can overwrite it
    // even while a debugger is attached.
    void RedirectEmbeddedPdb(const std::string& aDllPath, const std::string& aNewPdbName)
    {
        std::fstream f(aDllPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!f)
        {
            return;
        }
        const std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        const std::string needle = std::string(kModuleName) + ".pdb";
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
    const std::string copyPath = ExeDir() + kModuleName + "_live_" + std::to_string(index) + ".dll";

    if (!CopyFileA(SourceDllPath().c_str(), copyPath.c_str(), FALSE))
    {
        return false; // build may be mid-link and holding the source locked; caller retries later
    }

    // Give the copy its own pdb and point it there, so the build's pdb is never locked by
    // the debugger. Skipped cleanly if there's no pdb (e.g. release build).
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
        RetireModule(nullptr, copyPath, pdbCopyPath);
        return false;
    }

    // One symbol to resolve: it fills the whole table. Adding game entry points never
    // touches this code again. (A null getApi leaves the table zeroed -> caught below.)
    using GameGetAPI_t = void (*)(GameAPI*);
    const auto getApi = reinterpret_cast<GameGetAPI_t>(GetProcAddress(h, "Game_GetAPI"));
    GameAPI api{};
    if (getApi) { getApi(&api); }
    if (!api.Init || !api.Update || !api.DrawImGui)
    {
        RetireModule(h, copyPath, pdbCopyPath);
        return false;
    }

    outHandle  = h;
    outApi     = api;
    outPath    = copyPath;
    outPdbPath = pdbCopyPath;
    return true;
}

void GameModule::AdoptModule(void* aHandle, const GameAPI& aApi, std::string aDllPath, std::string aPdbPath)
{
    _handle      = aHandle;
    _api         = aApi;
    _livePath    = std::move(aDllPath);
    _livePdbPath = std::move(aPdbPath);
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

    AdoptModule(handle, api, std::move(path), std::move(pdbPath));
    _lastSourceTime = QueryGameSourceTime(); // baseline so we don't rebuild on startup
    return true;
}

void GameModule::Unload()
{
    RetireModule(_handle, _livePath, _livePdbPath);
    _handle = nullptr;
    _livePath.clear();
    _livePdbPath.clear();
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

    RetireModule(_handle, _livePath, _livePdbPath); // new module is good; retire the old one
    AdoptModule(newHandle, newApi, std::move(newPath), std::move(newPdbPath));

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

    // The source scan stats every game file, so throttle it: a save noticed within a
    // fraction of a second is imperceptible for hot-reload, and this runs every frame.
    const auto now = std::chrono::steady_clock::now();
    if (now - _lastSourceCheck < std::chrono::milliseconds(250))
    {
        return;
    }
    _lastSourceCheck = now;

    const uint64_t srcTime = QueryGameSourceTime();
    if (srcTime != 0 && srcTime != _lastSourceTime)
    {
        // A game source file was saved. Record it first so a failed compile (or a save
        // mid-build) retries cleanly, then kick off the build; PollBuild reloads on success.
        _lastSourceTime = srcTime;
        RequestRebuild();
    }
}

void GameModule::RequestRebuild()
{
    if (_buildProc)
    {
        return; // a build is already in flight
    }

    const std::string buildDir = RepoRoot() + "build";
    const std::string cmd = std::string("cmake --build \"") + buildDir + "\" --config " + kBuildConfig + " --target " + kModuleName;

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

    // The game sources live at <repo>/src/game. Absent in a shipped build -> return 0 so
    // the watcher simply does nothing.
    const fs::path dir = fs::path(RepoRoot()) / "src" / "game";

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
