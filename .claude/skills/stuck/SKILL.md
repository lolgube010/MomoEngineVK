---
name: stuck
description: Graduated hint ladder for when Momo is stuck on a bug, concept, or implementation. Invoke proactively (Momo never calls it) whenever Momo asks "how do I…", "why doesn't…", or is blocked. Never skip rungs, never give code.
---
<!-- AI-AUTHORED: written and maintained by Claude (AI assistant). -->

# Hint ladder

Climb one rung per exchange. Only go up when Momo has tried the current rung and is still stuck. Say which rung
you're on ("rung 2") so Momo can ask for the next one deliberately.

0. **Diagnose.** Ask what they expected, what happened and what they've tried. Ask for the validation output, the
   RenderDoc capture or the error. Usually stop here: a good question is the whole hint.
1. **Concept and source.** Name the concept and point to where it's explained: a spec section, man page, vkguide
   chapter, paper or sample repo, with a link. Don't summarize it for them.
2. **Where to look.** Name the file and function in their code where the problem or change lives, plus the
   relevant API names (e.g. `vkCmdPipelineBarrier2`, `VkImageMemoryBarrier2::oldLayout`).
3. **Narrow nudge.** Ask a pointed question or give a one-sentence observation about the specific wrong
   assumption ("which stage writes that image before the blit?").
4. **Pseudocode.** Give at most ~6 plain-language steps for the narrowest piece only. No C++ syntax, no types, no
   copyable lines.

There is no rung 5. If they're still stuck after rung 4, suggest a debugging experiment (RenderDoc, a debug-draw
visualization, a minimal repro) and tell them to take a break if it's late.

After it's solved, ask Momo to explain in one or two sentences why the fix works, and note the lesson in
`.claude/journal.md`.
