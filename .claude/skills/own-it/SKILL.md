---
name: own-it
description: Ownership audit of a file or subsystem in MomoEngineVK. Momo explains the code, Claude probes for gaps, then records a verdict in .claude/ledger.md. Invoke proactively (Momo never calls it) when Momo is (re)learning or auditing existing code, deciding what to salvage from pre-rewrite, or asks "what's left that I don't understand?".
---
<!-- AI-AUTHORED: written and maintained by Claude (AI assistant). -->

# Ownership audit

Goal: every line in the repo is something Momo can defend in an interview.

1. **Pick a target.** Use the argument or the next `unknown` entry in `.claude/ledger.md`. If the ledger doesn't
   exist, build it first: list each file in `src/` and `shaders/` with status `unknown`.
2. **Momo explains first.** Ask Momo to walk through the target in their own words: what it's for, its
   lifetime, who calls it and what would break without it. Don't pre-explain.
3. **Probe.** Ask 3–5 questions a studio senior would ask: sync and lifetimes, why this design over the
   alternatives, cost, failure modes. Use "what happens if…" questions.
4. **Fill gaps with sources.** For anything shaky, point to the reading (see the shelf in CLAUDE.md). Don't
   lecture.
5. **Verdict.** Agree on one verdict with Momo and record it in the ledger with a one-line note:
   - `owned`: Momo can explain and defend it.
   - `reading`: the code is fine but Momo has gaps. List what to read.
   - `rewrite`: Momo should delete it and write their own version. Common for code Momo didn't write or
     over-engineered systems.
   - `cut`: not worth keeping (dead, speculative, or portfolio-negative).
6. Flag tutorial-narration comments, and comments that explain the obvious, for Momo to clean up.

Keep each audit small. One file or one concept per session is plenty.
