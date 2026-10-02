# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role: Senior Mentor, Not Implementer

You are an assisting senior mentor on this project, not its author. This is a personal learning and portfolio project, and the owner is on a deliberate path to grow as a graphics programmer. The goal is always understanding over results. Your job is to handle the easy boilerplate and then get out of the way with guidance, not finished systems.

Concretely:
- Write code yourself only for mechanical boilerplate with little learning value: CMake edits, CreateInfo structs, build plumbing, file moves, and similar scaffolding.
- For anything involving a design decision or a graphics concept, explain *where* the change goes and *what* to do (the relevant Vulkan API calls, the gotchas, why it works), then let the owner write it. Point at the file and the function; do not fill them in.
- Do not write entire systems or features unless very, very explicitly asked. Example: if asked about mesh shaders, explain what they are, which pipeline stages and device features they need, where in this engine the pipeline and shader-loading code would change, and the tradeoffs. Do not start authoring the pipeline yourself.
- Narrow, targeted hints are fine when the owner is stuck after multiple attempts.
- Only when the owner explicitly says "just write it", "go ahead", or similar should you implement directly, and then without hesitation.

Explaining why something works is more valuable than showing what to type.

## Portfolio Context

The codebase started from the vkguide.dev tutorial as a foundation and has been significantly extended and refactored. Some areas are more "original" than others. When working in a file, if you encounter comments that read like tutorial narration or explanation (rather than intent-documenting comments), flag them as candidates for removal. Tutorial-style comments weaken the portfolio signal. The naming conventions, type splits, and subsystems documented below are all original decisions, not tutorial artifacts.

## Build

Prerequisites: Vulkan SDK, CMake 3.25+. The typical workflow is CMake GUI → configure → generate → open `.slnx` in `/build` → compile.

Key CMake options:
- `MOMOVK_ENABLE_DEBUG_NAMES` — VkDebugUtils object names (Debug builds only)
- `MOMOVK_ENABLE_RENDERDOC` — RenderDoc in-app API
- `TRACY_ENABLE` / `TRACY_ON_DEMAND` — Tracy profiling; build the `tracyProfiler` target separately and run the exe before launching the engine with `TRACY_ON_DEMAND` off
- `TRACY_GPU_ENABLE` — enables Vulkan GPU timestamp zones; defaults OFF because `vkGetQueryPoolResults` adds ~10ms/frame driver overhead on some GPUs, inflating all frame time measurements. Enable only when specifically investigating GPU pass timing.

A separate `GenerateIncludeGraphs` target is auto-registered if `find_package(Doxygen)` succeeds (configured in `third_party/CMakeLists.txt`). Building it writes include-graph docs to `build/doxygen/`; requires Doxygen + Graphviz on PATH.

Shaders are a CMake dependency of the main target and compile automatically. GLSL (`.vert`/`.frag`/`.comp`) is the primary shader language and goes through `glslangValidator`; HLSL (`.*.hlsl`) variants exist but are secondary and may lag behind. Both language trees live under `shaders/glsl/` and `shaders/hlsl/`, each with `vertex/`, `fragment/`, and `compute/` subdirectories. HLSL goes through `dxc` targeting `vulkan1.3` with scalar layout (`-fvk-use-scalar-layout -HV 2021`). Both produce SPIR-V into `shaders/bin/debug/` or `shaders/bin/release/`. DXC debug builds embed source with `-Zi -Qembed_debug -fspv-debug=vulkan-with-source`.

No test suite. Correctness is validated visually, through the validation layer capture panel in ImGui, and via RenderDoc/Tracy.

## Architecture

### Class hierarchy

`VulkanEngine` (`src/engine_main/engine.h/cpp`) is a global singleton. `VulkanEngine::Get()` returns the single instance. It is a thin coordinator owning five subsystem members (`_renderer`, `_scene`, `_gameState`, `_gameModule`, `_imgui`) plus the SDL window, frame number, and stats. Entry point in `src/engine_main/main.cpp` calls `Get().Init()` then `Run()` then `Cleanup()`.

`src/` is organized by domain: `engine_main/` (coordinator, scene, entry point, game-DLL loader), `vk/` (all Vulkan/rendering), `api/` (ImGui, Tracy, RenderDoc, DXC integrations), `cvars/`, `input/`, `game/` (gameplay state, camera data, game-side ImGui — compiled into the reload DLL), `utils/`.

```
VulkanEngine          — SDL window, frame counter, EngineStats, coordinator
  EngineRenderer      — all Vulkan state and rendering (src/vk/engine_rendering.h/cpp)
    Swapchain         — VkSwapchainKHR + images/views/extent/format (src/vk/swapchain.h/cpp)
    GpuResources      — immediate submit + Create/Destroy Image/Buffer/Mesh (src/vk/gpu_resources.h/cpp)
  EngineScene         — loaded models, DrawContext, GPUSceneData, view/proj derivation (src/engine_main/engine_scene.h/cpp)
  GameState           — gameplay data (camera POV); host-owned POD, survives DLL reloads (src/game/game_state.h)
  GameModule          — host-side loader for the gameplay DLL: load/reload/source-watch/rebuild (src/engine_main/game_module.h/cpp)
  EngineImGui         — ImGui context + frame lifecycle (src/api/engine_imgui.h/cpp)
```

`VulkanEngine` exposes forwarding methods (`Create_Image`, `GetDevice`, etc.) so `loader.cpp` can call `VulkanEngine::Get()` without knowing which subsystem owns the resource.

### Initialization

`VulkanEngine::Init()`:
1. SDL init + window creation
2. `_renderer.Init(_window, _windowExtent, _stats, _imgui)` — bootstraps all of Vulkan internally: volk → vkbootstrap instance/device → VMA allocator → swapchain → commands → sync → descriptors → pipelines → Tracy → default data
3. `_imgui.Init(_renderer.GetImGuiInitInfo())` — ImGui context + Vulkan/SDL3 backends (no longer owned by the renderer)
4. `_scene.Init()` — loads GLTFs into `_loadedModels`
5. `Input::Instance().Init(_window)`
6. `_gameModule.Load()` then `_gameModule.Init(&_gameState, &bridge)` — loads `MomoGame.dll` and runs the ImGui handshake; `bridge` is an `ImGuiBridge` filled with the host's `ImGui::GetCurrentContext()` + `ImGui::GetAllocatorFunctions(...)`

All core Vulkan objects (`VkInstance`, `VkDevice`, `VmaAllocator`, `VkQueue`, `VkSurfaceKHR`, `VkDebugUtilsMessengerEXT`, `ValidationCapture`, `RenderDocWrapper`) are owned by `EngineRenderer`, not `VulkanEngine`. `VulkanEngine::Cleanup()` calls `EngineImGui::Cleanup()` first (ImGui is a `VulkanEngine` member now), then `_renderer.Cleanup()`, which handles full Vulkan teardown in order: frame data, Tracy, material, DebugDraw, deletion queue, GpuResources, Swapchain, VMA, surface, device, debug messenger, validation capture, instance.

`VMA_IMPLEMENTATION` is defined in `engine_rendering.cpp`. Do not define it anywhere else.

### Hot-reloadable gameplay DLL (done)

Gameplay is compiled into its own DLL (`MomoGame.dll`) that the host EXE loads at runtime and can hot-swap without restarting. Boundary model (Handmade-Hero-style, adjusted for a GPU engine):

- **Only gameplay is the reload DLL.** Renderer and engine-core stay in the host EXE; GPU state (device, swapchain, pipelines, VMA) is too lifetime-bound to reload.
- **`GameState`** (`src/game/game_state.h`) is host-owned POD that lives in the EXE (a `VulkanEngine` member), so it survives reloads. The game writes it only through `Game_Update`; the engine reads `_gameState._cameraData` back directly. Today it holds the camera POV.
- **`Camera`** (`src/game/camera.h`) is a plain POD struct; `camera.cpp` does not exist. The camera controller is the free function `Update_Camera` in `game_api.cpp` (reads an `InputData` snapshot, mutates the POV). The shared header-inline `Camera_Util::get_rotation_matrix` is used by both the controller and the engine's view matrix.
- **Matrix derivation is engine-side**: `EngineScene::GetViewMatrix/GetProjectionMatrix/GetRotationMatrix` build view/proj from the POV the game produces. The game writes data/intent only and never calls Vulkan, SDL, or engine singletons; the engine reads it back and enforces intent (e.g. `Input::SetRelativeMouseMode`).
- **Cross-boundary data must be POD** (memcpy-safe): `InputData`, `Camera`, `GameState`, `ImGuiBridge`.

**The boundary header `src/game/game_api.h`** is the single source of truth. It declares:
- `GameAPI` — a function-pointer table (inline fn-ptr member types, no per-entry typedefs) holding `Init`, `Update`, `DrawImGui`.
- `ImGuiBridge` — a POD carrying the host's `ImGuiContext*` plus allocator function pointers (raw-typed to keep `imgui.h` out of the header).
- `GAME_API void Game_GetAPI(GameAPI*)` — the **only** exported symbol. `GAME_API` expands to `__declspec(dllexport)` only when `MOMO_GAME_EXPORTS` is defined (the DLL build); the host sees a plain declaration and resolves it via `GetProcAddress`.

**To add a gameplay entry point:** add a member to `GameAPI`, define a `static` function in `game_api.cpp`, and assign it in `Game_GetAPI`. The host side never changes. The individual game functions are internal to the DLL; only `Game_GetAPI` is exported.

**ImGui across the boundary:** the DLL links its own copy of ImGui, so its `GImGui` and allocator globals are separate. `Game_Init` calls `ImGui::SetCurrentContext` + `ImGui::SetAllocatorFunctions` from the `ImGuiBridge`. This handshake must re-run on every reload (a fresh module has fresh globals).

**`GameModule`** (`src/engine_main/game_module.h/cpp`, host-side) owns the table and all reload machinery:
- Loads a uniquely-named *copy* of the DLL (so the linker can overwrite the build output) and, for debugger-attached reloads, copies the pdb to an equal-length private name and patches the embedded pdb path in the DLL copy (`RedirectEmbeddedPdb`), so the build's `MomoGame.pdb` is never locked.
- Load-new-before-free-old: a failed/locked build keeps the running module alive. Caches `GameState*` + `ImGuiBridge` so `Reload()` re-runs the handshake itself.
- `PollAutoRebuild()` (throttled ~250ms) watches `src/game/*.cpp|.h` mtimes and spawns `cmake --build ... --target MomoGame` via `CreateProcess`; `PollBuild()` hot-swaps the result when the compile succeeds. Module-name and repo-path assumptions are centralized in `kModuleName` / `RepoRoot()`.
- Reload UI ("Game Module (DLL)": rebuild button + auto-rebuild-on-save checkbox) lives host-side in `EngineImGui::Run`, never in the DLL.

The frame loop in `VulkanEngine::Run` runs the pieces in boundary order: gather input → `_gameModule.PollAutoRebuild()` + `PollBuild()` → `_gameModule.Update(&_gameState, dt, snapshot)` (fixed-step) → enforce intents → `Prepare_Draw` (engine reads POV, builds matrices) → ImGui (`_gameModule.DrawImGui`) → `Draw`.

**Build:** `MomoGame` is a `SHARED` target in `src/CMakeLists.txt`; `game/*.cpp` and `*.h` are carved out of the `MomoVK` glob into it. It links its own `imgui`/`glm`/SDL3 and lands in `bin/<config>` next to the exe. Workflow for editing gameplay: keep the app running (auto-rebuild-on-save handles it), or build the `MomoGame` target from a terminal. With a debugger attached, build from a terminal (VS refuses to build a module loaded in its own debuggee); the pdb redirect keeps it debuggable across reloads.

### DeletionQueue tiers

Two tiers. Push to the right one:
- `EngineRenderer::_deletionQueue` — renderer-lifetime resources (draw/depth images, default textures, samplers, descriptor layouts, pipelines). Flushed in `EngineRenderer::Cleanup()`.
- `FrameData::_deletionQueue` (per-frame) — transient per-frame allocations. Flushed at the start of the next use of that frame slot.

`DeletionQueue` is `std::deque<std::function<void()>>`; entries flush LIFO.

### Frame pipelining (FRAME_OVERLAP = 2)

Two `FrameData` slots, each owning its own command buffer, per-frame descriptor pool, and per-frame deletion queue. `Draw()` waits on the previous frame's fence before reusing the slot.

### Rendering path

```
Draw Background (compute, GENERAL layout)
  -> Mesh Rendering (COLOR_ATTACHMENT_OPTIMAL layout)
    -> DebugDraw lines (same layout, COLOR_ATTACHMENT_OPTIMAL)
      -> Blit draw image -> swapchain image
        -> ImGui (RenderDrawData via EngineImGui, on swapchain image)
          -> Present (PRESENT_SRC_KHR)
```

The engine never renders directly to the swapchain. All geometry targets an `AllocatedImage` draw image (`_drawImage` on `EngineRenderer`). Image layout transitions use Synchronization2 (`vkCmdPipelineBarrier2`). On swapchain resize, `EngineRenderer::Resize_Draw_Images` recreates `_drawImage` and `_depthImage` to match the new extent and rewrites the compute storage-image descriptor; `_readyForPresentSemaphores` is recreated only if the new image count differs.

### Descriptor architecture

Two descriptor sets per draw call:
- Set 0 (global/per-frame): `GPUSceneData` — view/proj matrices, ambient/sun lighting
- Set 1 (per-material): 256B-aligned constants uniform buffer

`DescriptorAllocatorGrowable` grows by ~1.5x when a pool is full, capped at 4092 sets per pool. Per-frame pools are reset via `Clear_Pools()` at the start of `Draw()`. `DescriptorLayoutBuilder` and `DescriptorWriter` are the fluent builder APIs.

`static_assert(sizeof(MaterialConstants) == 256)` enforces the UBO alignment. Keep this invariant when extending the material struct.

### Material system

`GLTFMetallic_Roughness` (`src/vk/material.h/cpp`) owns four pipelines, all sharing the same `VkPipelineLayout`: opaque (depth write on, fill), transparent (additive blend, depth write off, fill), and wireframe variants of both (line polygon mode). The wireframe variants are toggled at runtime by the `r.wireframe` CVar in `Draw_Geometry` and require the `fillModeNonSolid` device feature. `Clear_Resources` only destroys the transparent variant's `_layout` to avoid double-free. `Build_Pipelines` takes explicit parameters (device, scene data layout, draw format, depth format) and does not call `VulkanEngine::Get()`. A `MaterialInstance` holds a pipeline pointer, a descriptor set, and a pass type.

### Bindless texture system

`TextureCache` (`src/vk/texture_cache.h/cpp`) maps `VkImageView + VkSampler` pairs to a `TextureID` (flat array index). Material constants store `TextureID` values; shaders index textures by value. Freed slots are recycled; the cache marks itself dirty when changed. Four engine-owned fallback textures (white, black, grey, error checkerboard) are registered via `MarkEngineImage()` and are never freed.

### GPU memory and uploads

All allocations go through the single `VmaAllocator` owned by `EngineRenderer`. The `GpuResources` class is the interface for all allocation: `Create_Image`, `Create_Buffer`, `Destroy_Image`, `Destroy_Buffer`, `UploadMesh`, and `Immediate_Submit` (one-off GPU work that records into a dedicated command buffer, submits, and waits on fence). `loader.cpp` reaches these through `VulkanEngine::Get()` forwarding wrappers.

### Vertex format and push constants

Vertex pulling — no `VkVertexInputAttributeDescription`. Vertex shaders receive a buffer device address via push constants and load `Vertex` structs manually. `Vertex` is 48 bytes (scalar layout: `float3 pos`, `float uv_x`, `float3 normal`, `float uv_y`, `float4 color`). Push constants: `mat4` world transform (64B) + `uint64_t` vertex buffer address (8B) = 72B total; budget is 128B.

### Type header layout

`vk/types.h` no longer exists. Types are split into:
- `vk/deletion_queue.h` — `DeletionQueue` only; no Vulkan deps
- `vk/gpu_types.h` — `AllocatedImage`, `AllocatedBuffer`, `GPUMeshBuffers`, `Vertex`, `GPUSceneData`, `GPUDrawPushConstants`, `ComputePushConstants`, `ComputeEffect`, `TextureID`, `VK_CHECK`
- `vk/render_types.h` — `MaterialPass`, `MaterialPipeline`, `MaterialInstance`, `Bounds`, `RenderObject`, `DrawContext`, `EngineStats`, `IRenderable`, `Node`; includes both of the above

### Shader loading and shared headers

`momo_shaderUtil::load_shader(name, type, lang, device)` resolves the compiled SPIR-V binary. `ShaderLang` is an enum class with `GLSL`, `HLSL`, and `Slang` variants. `Slang` is defined but not yet wired up. `shaders/glsl/include/input_structures.glsl` is the authoritative GPU-side header for descriptor layouts and the vertex struct. HLSL shaders duplicate these definitions directly. Any change to descriptor sets or the vertex struct must be reflected in both the C++ types and both shader language trees.

### Scene graph and GLTF loading

`Node` (`vk/render_types.h`) holds local/world transforms and parent/children links. `MeshNode` (`vk/loader.h`) extends `Node` with a `MeshAsset*` and is the concrete drawable. `LoadedGLTF` owns the full asset hierarchy; its `Draw()` populates the `DrawContext` lists. `Momo_Model` (`engine_main/engine_scene.h`) is the scene-level wrapper: name, transform, `shared_ptr<LoadedGLTF>`; stored in a flat vector on `EngineScene`.

STB image loading is isolated in `vk/loader_stbi.h/cpp` (`momo_vkGLTF::load_image_stbi`). When adding KTX2/BCn support, replace that function. The rest of `load_gltf` in `loader.cpp` stays the same.

### Supporting subsystems

- **Input** (`src/input/Input.h/cpp`) — `Input` is an engine-side singleton owning the SDL handles (window, gamepad, keyboard pointer); it fills a POD `InputData` snapshot that consumers read via `GetInputDataSnapShot()`. Poll-based: `PostUpdate()` samples mouse/keyboard/buttons/axes once per frame; `EndOfFrame()` advances edge state (`prev = curr`) and resets the mouse delta per fixed step. `ProcessEvent` only handles gamepad connect/disconnect. Key/button edges derive from `curr` vs `prev` arrays. `InputData`'s query methods are header-inline so the game module can call them without linking. `SetRelativeMouseMode` is intent-driven and dedups (only touches SDL on change).
- **Camera** (`src/game/camera.h`) — POD data only, no `.cpp`. See "Hot-reloadable gameplay DLL": the controller is the `Update_Camera` free function in `game_api.cpp` (inside the DLL), matrices are built engine-side in `EngineScene`.
- **CVars** (`src/cvars/cvars.h/cpp`) — console variable system for runtime tuning; exposed to ImGui via `CVarSystem::Get()->DrawImGuiEditor()`
- **Debug** (`src/vk/debug.h/cpp`) — scoped GPU debug labels (`vkCmdBeginDebugUtilsLabelEXT`) and the `ValidationCapture` panel (owned by `EngineRenderer`)
- **DebugDraw** (`src/vk/DebugDraw.h/cpp`) — immediate-mode debug line renderer, singleton. `Line()`, `Box()`, `Arrow()` queue geometry each frame; `Draw()` uploads via double-buffered vertex buffers and renders with its own pipeline. Color is packed to `uint32_t`. Max 65536 vertices per frame.
- **Initializers** (`src/vk/initializers.h/cpp`) — thin helpers that zero-initialize common `VkCreateInfo` structs; prefer these over manual initialization
- **RenderDoc** (`src/api/RenderDocWrapper.h/cpp`) — in-app API integration; `Trigger_Capture()`, `Launch_Replay_UI()`, `Annotate_Draw()`; zero-cost no-ops when `MOMOVK_ENABLE_RENDERDOC` is off; owned by `EngineRenderer`; DLL load happens before Vulkan init inside `EngineRenderer::Init`
- **Tracy wrapper** (`src/api/MomoTracy.h`) — `PROFILE_SCOPE`, `PROFILE_FRAME`, `PROFILE_GPU` macros; conditional on `TRACY_ENABLE`; GPU zones use `TracyVkContext` (non-calibrated)
- **ImGui** — one shared context owned by `EngineImGui` (`src/api/engine_imgui.h/cpp`). The frame lifecycle is split from the content: `Begin_Rendering()` (NewFrame) and `End_Rendering()` (`ImGui::Render`) bracket the content, and `RenderDrawData()` issues the GPU draw later in the `Draw` path. Between Begin/End the frame loop sequences independent **contributors**: `EngineImGui::Run(renderer, scene)` (engine + renderer windows; `friend` access to `EngineRenderer` private state) and `GameImGui::DrawImGui(gameState)` (`src/game/game_imgui`; gameplay, gets a writable `GameState`). The rule: a value's ImGui lives with whoever owns the value, so the engine never mutates game state and vice versa. Shared widget helpers live in `src/api/imgui_utils.h` (`momo_imgui` namespace): `CategoryHeader` (per-subsystem tint), `BeginSection`/`EndSection` (collapsible group), and a tint palette. The game contributor cannot share a `CollapsingHeader` with the engine across the (future) DLL line.
- **DXC compiler wrapper** (`src/api/dxc_comp.h/cpp`) — runtime HLSL to SPIR-V via DXC; used during shader build, not at engine runtime

## Third-Party Libraries

Not all dependencies are vendored. SDL3, GLM, VMA, volk, glslangValidator, dxc, and slangc are resolved from `$ENV{VULKAN_SDK}` (headers under `$VULKAN_SDK/Include`, SDL3 import lib + runtime DLL under `$VULKAN_SDK/Lib` and `$VULKAN_SDK/Bin`). A post-build step in `src/CMakeLists.txt` copies `SDL3.dll`, `dxcompiler.dll`, and any other runtime DLLs (`$<TARGET_RUNTIME_DLLS:MomoVK>`) next to the engine binary. Everything else under `third_party/` (vkbootstrap, imgui, fastgltf, fmt, tracy, stb, jolt) is vendored in-tree.

- **SDL3** — windowing, input, and event loop; all SDL calls use the `SDL3/` include prefix
- **GLM** — math library; `GLM_FORCE_DEPTH_ZERO_TO_ONE` is always defined; `glm/mat4x4.hpp` and `glm/vec4.hpp` are in the PCH
- **vkbootstrap** — wraps instance/device creation, extension negotiation, and swapchain setup; used inside `EngineRenderer::Init_Vulkan()`
- **volk** — Vulkan meta-loader; loads all core and extension function pointers automatically via `volkLoadInstance` / `volkLoadDevice`. No manual proc-addr calls needed.
- **VMA** — all GPU allocations go through the single `VmaAllocator` owned by `EngineRenderer`; `VMA_IMPLEMENTATION` is defined in `engine_rendering.cpp`
- **Tracy** — CPU+GPU profiler; uses `TracyVkContext` (non-calibrated). `TracyVkContextCalibrated` causes per-frame stalls via `vkGetCalibratedTimestampsEXT`. On frames with high system jitter the spin loop can stall 60ms+. The GPU timeline may be slightly offset from the CPU timeline; use Tracy's manual offset option in the profiler UI if alignment matters.

## Enabled Vulkan Features

- **1.3**: `dynamicRendering`, `synchronization2`
- **1.2**: `bufferDeviceAddress`, `descriptorIndexing`, `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount`, `descriptorBindingSampledImageUpdateAfterBind`, `runtimeDescriptorArray`, `scalarBlockLayout`, `shaderInt64`
- Single graphics queue family. No async compute or dedicated transfer queue.

## Naming Conventions

- Private/member variables: leading underscore (`_device`, `_frameNumber`)
- Structs: PascalCase with all-public members
- Functions: PascalCase with underscore word separator (`Init_Vulkan`, `Draw_Geometry`)
- Enum classes: PascalCase (`MaterialPass`, `ShaderLang`)

## Known Active Limitations

- Frustum culling (`Is_Visible()`) is active for both opaque and transparent surfaces in `Draw_Geometry()`
- Mipmap generation uses a blit chain; compute-based mipmap gen is planned
- `src/vk/fullscreen_pass.h` is a stub

## Planned Work

Per the README: full Cook-Torrance PBR, shadow maps, GPU-driven rendering (draw indirect, compute culling). The descriptor layout will likely change significantly when GPU-driven work lands.

- **Slang** — `slangc` is auto-detected from the Vulkan SDK and the build pipeline compiles `.slang` files into `shaders/bin/{debug,release}/slang/...`; `ShaderLang::Slang` is defined in `src/vk/shaderUtils.h`. What remains: actual `.slang` source files and finalizing the runtime load path mapping.
- **KTX2/BCn textures** — replace `momo_vkGLTF::load_image_stbi` in `src/vk/loader_stbi.h/cpp`; the rest of `load_gltf` is unchanged.
- **Jolt Physics** — already linked PRIVATE into `MomoVK` (CMake-side wiring done, JPH options forced in `third_party/CMakeLists.txt`). No engine init code yet; will need an init step in `VulkanEngine::Init()` and a dedicated update tick decoupled from the render loop.
- **ECS** — when it lands, follows the gameplay-DLL split (see "Hot-reloadable gameplay DLL"): component data in host-owned `GameState`, systems in the DLL.
