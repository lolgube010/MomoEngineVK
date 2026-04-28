#pragma once

#if TRACY_ENABLE
#include <tracy/Tracy.hpp>

#define PROFILE_SCOPE               ZoneScoped
#define PROFILE_SCOPE_N(name)       ZoneScopedN(name)
#define PROFILE_SCOPE_C(color)      ZoneScopedC(color)
#define PROFILE_NAMED(name)         ZoneNamedN(___tracy, name, true)
#define PROFILE_FRAME               FrameMark
#define PROFILE_FRAME_N(name)       FrameMarkNamed(name)
#define PROFILE_MSG(msg)            TracyMessage(msg)
#define PROFILE_PLOT(name, val)     TracyPlot(name, val)

// GPU zones — requires TRACY_GPU_ENABLE on top of TRACY_ENABLE.
// Use TracyVkContext (non-calibrated). TracyVkContextCalibrated causes per-frame
// stalls: vkGetCalibratedTimestampsEXT spins in Collect() until deviation is within
// tolerance, and on jittery frames this can block for 60ms+. See Tracy issue #663.
#if TRACY_GPU_ENABLE
#include <tracy/TracyVulkan.hpp>
#define PROFILE_GPU(ctx, cmdbuf, name)              TracyVkZone(ctx, cmdbuf, name)
#define PROFILE_GPU_C(ctx, cmdbuf, name, color)     TracyVkZoneC(ctx, cmdbuf, name, color)
#define PROFILE_GPU_COLLECT(ctx, cmdbuf)            TracyVkCollect(ctx, cmdbuf)
#else
#define PROFILE_GPU(ctx, cmdbuf, name)
#define PROFILE_GPU_C(ctx, cmdbuf, name, color)
#define PROFILE_GPU_COLLECT(ctx, cmdbuf)
#endif

#else
#define PROFILE_SCOPE
#define PROFILE_SCOPE_N(name)
#define PROFILE_SCOPE_C(color)
#define PROFILE_NAMED(name)
#define PROFILE_FRAME
#define PROFILE_FRAME_N(name)
#define PROFILE_MSG(msg)
#define PROFILE_PLOT(name, val)
#define PROFILE_GPU(ctx, cmdbuf, name)
#define PROFILE_GPU_C(ctx, cmdbuf, name, color)
#define PROFILE_GPU_COLLECT(ctx, cmdbuf)
#endif
