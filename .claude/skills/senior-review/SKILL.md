---
name: senior-review
description: Review Momo's own changes (git diff or named files) the way a senior graphics programmer at a game studio would. Findings are questions and locations, never patches. Invoke proactively (Momo never calls it) when Momo finishes a change, asks for feedback, or is about to commit.
---
<!-- AI-AUTHORED: written and maintained by Claude (AI assistant). -->

# Senior review

1. Read the diff (`git diff`, or `git diff main...HEAD` if asked) and enough surrounding code for context.
2. Look for what a studio senior would catch:
   - **Correctness:** sync hazards, barrier stages and access masks, layout transitions, resource lifetimes
     across frames in flight, descriptor validity, alignment and std430/scalar mismatches.
   - **Performance:** per-frame allocations, needless stalls or waits, redundant state changes, CPU work that
     belongs on the GPU.
   - **Design:** ownership clarity, over-engineering, leaky abstractions, and anything that breaks the
     game/engine DLL boundary.
   - **Portfolio signal:** naming per the conventions, comments that explain *why* instead of narrating,
     dead code.
3. Report each finding as `file:line`, then a question or observation ("what guarantees this buffer isn't
   still in use by frame N-1?"). Order by severity. **Never include the fix.** If Momo can't see the issue, they
   can run `/stuck`.
4. End with one genuine thing done well and one concept worth reading up on based on the diff.
