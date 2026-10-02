<!-- AI-AUTHORED: written and maintained by Claude (AI assistant). Quotes marked "Momo:" are Momo's words. -->
# Journal

## Raise next session
- **CMake pass:** Momo wants Claude to take a cleanup pass over the CMake setup (root, `src/`, `shaders/`,
  `third_party/`). Salvage it from `pre-rewrite` and explain the changes. Claude is allowed to do this one.
- **Approach decision:** Momo is leaning toward keeping `pre-rewrite` as the archive and starting
  "scratch-ish" on `rewrite`, salvaging what they understand (CMake first). Help them settle what "scratch-ish"
  means: which pieces to port, and in what order.

## Parking lot
Ideas Momo had mid-session that were off the goal. Review them at `session end` and move them into the backlog
or drop them.

## 2026-10-02: reboot
Back after ~5 months. Reset the rules: Momo writes all code, Claude only mentors, and anything Claude touches
gets a header. The old specialist agents are deleted, and their useful facts are in
`.claude/notes/pre-rewrite-engine.md`. State is versioned in `.claude/` so it follows Momo between machines.
Momo: the hot-reload DLL "is so fucking cool but it's so clunky and I have no idea how it actually works". Momo
wants to be able to build something like it themselves. That's a strong motivation hook, so use it.
Feedback: Momo loves being pointed to a specific source ("read this, here"). It makes the whole thing feel
doable. Keep it the default move: a precise link plus section, not a summary.

# Backlog (from Momo's old todos)

## Phase 0: get a grip again
- Skim vkguide again, then the extra chapters (incl. the hardware chapter: https://vkguide.dev/docs/extra-chapter/hardware/)
- https://amini-allight.org/post/dispatching-hundreds-of-thousands-of-compute-tasks
- Engine-architecture talk on game loops, swapchain config and frame pacing: https://www.youtube.com/watch?v=EM9utsGhaYs
- Clean out temporary and half-understood stuff (via /own-it)

## Investigations
- Debug draw from the game DLL: the singleton lives in the EXE, so how does the DLL reach it? (Same shape as the
  ImGui context problem. Momo should design this themselves.)
- Instancing: the vertex buffer address is identical across draws of the same mesh. Are meshes loaded more than
  once? Trace the mesh pipeline end to end.
- A better way to inspect a .gltf's contents (an ImGui tree?)
- Sun-direction debug draw

## Own projects (build it yourself, from scratch)
- Hot-reloadable gameplay DLL, v0: minimal and clunk-free. Load, look up one function, poll for a newer
  build, swap. Source: Handmade Hero days ~21–25. Add the fancy parts (copy-before-load, keeping state alive,
  debugger/PDB handling) one at a time, only when their absence actually hurts.

## Fun detours (for low-energy days)
- Sierpinski triangle by midpoint subdivision, with each triangle colored differently. Maybe an infinite zoom.
  https://immersivemath.com/ila/ch02_vectors/ch02.html#fig_vec_Sierpinski
- Ring buffer for an FPS/frametime graph

## Big rocks (later, in this order)
1. Cook-Torrance PBR (GGX, Smith, Fresnel-Schlick), then IBL. Karis 2013, LearnOpenGL PBR, Filament docs.
2. Shadow maps, then cascades
3. GPU-driven: indirect draws plus compute culling. vkguide GPU-driven chapter, niagara.
4. Mesh shaders: https://docs.vulkan.org/spec/latest/chapters/VK_NV_mesh_shader/mesh.html,
   https://github.com/nvpro-samples/gl_vk_meshlet_cadscene, https://medium.com/@williscool/task-and-mesh-shaders-a-practical-guide-vulkan-and-slang-25baebe6388e
- Paper to read: https://jcgt.org/published/0015/01/03/
