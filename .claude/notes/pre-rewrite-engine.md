<!-- AI-AUTHORED: written and maintained by Claude (AI assistant). Distilled from the deleted specialist agents. -->
# Pre-rewrite engine: salvage reference

This describes the old engine on branch `pre-rewrite` (and `main`), which is heavily AI-assisted. It's for Claude
to consult while Momo decides what to salvage. **Don't hand these facts to Momo as answers.** Use them to ask
better questions ("how did the old engine pick memory for mapped buffers, and why did it need an extra flag?").

## Build (likely salvage candidates, needing a CMake pass)
- Targets: the `MomoVK` exe, `MomoGame` (the hot-reload gameplay DLL), a `Shaders` custom target (a hard dependency
  of the exe), `TracyProfiler` (the profiler UI), and optionally `GenerateIncludeGraphs` (Doxygen + Graphviz).
- Shaders: GLSL via glslangValidator, HLSL via dxc (SM 6.6, vulkan1.3, scalar layout, HV 2021, embedded debug
  info in Debug), and Slang via slangc when it's found. Output goes to
  `shaders/bin/{debug,release}/{lang}/{stage}/`. Files are discovered with a `CONFIGURE_DEPENDS` glob.
- Definitions: `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `VK_NO_PROTOTYPES` (for volk), `IMGUI_IMPL_VULKAN_USE_VOLK`,
  `TRACY_ENABLE`, `MOMOVK_ENABLE_DEBUG_NAMES`, `MOMOVK_ENABLE_RENDERDOC`.
- Dependencies: VMA, GLM, SDL3, volk and dxc come from `$VULKAN_SDK`. vkbootstrap, imgui, fastgltf, fmt, tracy,
  stb and Jolt are vendored. A post-build step copies the runtime DLLs next to the exe. There's a PCH (STL, GLM,
  volk, fmt, fastgltf, VMA). Jolt is linked but never initialized.
- `TRACY_GPU_ENABLE` defaults off, because `vkGetQueryPoolResults` cost about 10ms per frame on some GPUs.
  Calibrated Tracy contexts caused stalls of 60ms or more.

## Systems that existed (each is a possible own-it or rewrite topic)
- A coordinator singleton, a renderer that owns all Vulkan state, a scene, an ImGui wrapper, input, CVars,
  DebugDraw, a RenderDoc wrapper and Tracy macros.
- Hot-reload gameplay DLL: one exported function fills a table of function pointers. Host-owned POD game state
  survives reloads. The DLL is copied before loading. The new module is loaded before the old one is freed.
  Source files are watched by mtime and rebuilt automatically. There's also a PDB path patch for
  debugger-attached reloads, which is the most arcane part. The DLL's ImGui context and allocator are handed
  over through a bridge struct.
- Rendering: it draws to an HDR offscreen image (RGBA16F), then blits to the swapchain, then draws ImGui on the
  swapchain. Reverse-Z with D32. Sync2 barriers, dynamic rendering, 2 frames in flight.
- Descriptors: set 0 holds per-frame scene data, set 1 holds the per-material UBO (256B, static_asserted). It
  used a growable descriptor allocator and a bindless texture cache (TextureID → array index, with fallback
  textures).
- Vertex pulling through a buffer device address in push constants (48B vertex, 72B push constants). There was
  no vertex input state.
- Materials: four pipelines sharing one layout (opaque/transparent × fill/wireframe).
- Frame loop: fixed step with an accumulator, a spiral-of-death clamp, and dt snapping to common refresh rates.

## Gotchas worth turning into questions
- With `vkAcquireNextImageKHR`, `OUT_OF_DATE` means the semaphore was not signaled. `SUBOPTIMAL` means it was
  signaled, so it has to be consumed by a present.
- Present semaphores are per swapchain image, not per frame in flight.
- VMA's `AUTO` usage needs a `HOST_ACCESS_*` flag to get mappable memory.
- Compute writes to storage images need the `GENERAL` layout.
