#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>          // or "tracy/Tracy.hpp" if you adjust include paths
#include <tracy/TracyVulkan.hpp>    // for GPU support (optional but very useful in Vulkan projects)

// CPU zones (most common)
#define PROFILE_SCOPE           ZoneScoped
#define PROFILE_SCOPE_N(name)   ZoneScopedN(name)
#define PROFILE_SCOPE_C(color)  ZoneScopedC(color)   // color = 0xRRGGBB

// Named + colored (handy for render passes)
#define PROFILE_NAMED(name)     ZoneNamedN(___tracy, name, true)

// Frame marker — call once per frame!
#define PROFILE_FRAME           FrameMark
#define PROFILE_FRAME_N(name)   FrameMarkNamed(name)

// Optional: messages / plots
#define PROFILE_MSG(msg)        TracyMessage(msg)
#define PROFILE_PLOT(name, val) TracyPlot(name, val)

// Vulkan GPU zones (highly recommended!)
// Requires you to have a TracyVkCtx created (see below)
#define PROFILE_GPU(cmdbuf, name)     TracyVkZone(cmdbuf, name)
#define PROFILE_GPU_C(cmdbuf, name, color)  TracyVkZoneC(cmdbuf, name, color)
#define PROFILE_GPU_COLLECT(cmdbuf)   TracyVkCollect(cmdbuf)

#else
// No-op when Tracy is disabled (TRACY_ENABLE=OFF or not set)
#define PROFILE_SCOPE
#define PROFILE_SCOPE_N(name)
#define PROFILE_SCOPE_C(color)
#define PROFILE_NAMED(name)
#define PROFILE_FRAME
#define PROFILE_FRAME_N(name)
#define PROFILE_MSG(msg)
#define PROFILE_PLOT(name, val)
#define PROFILE_GPU(cmdbuf, name)
#define PROFILE_GPU_C(cmdbuf, name, color)
#define PROFILE_GPU_COLLECT(cmdbuf)
#endif