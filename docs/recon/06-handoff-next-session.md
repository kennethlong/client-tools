# 06 — Handoff: next session opens here

Stopping point at end of context window 1. The user is going to clear
context and reopen — this document captures exactly where to pick up.

## State of the world

- **Research phase: complete.** Build chain, boot sequence (17 phases),
  middleware inventory, swg-main diff. Sources in `docs/` (HTML) and
  `docs/research/` + `docs/recon/` (markdown).
- **Strategy decided:** Framing B (CMake + VS 2022, copy swg-main
  patterns where applicable). Phase 1 = MVB-0 (compile only). See
  memory `project_swg_client_build.md`.
- **GSD workflow: paused at codebase-map gate.**
  `/gsd-new-project` was invoked, reached Step 2, user chose "Run
  /gsd-map-codebase first", skill exited per contract. **No
  `.planning/` artefacts exist yet.** See memory
  `gsd_workflow_state.md`.
- **CLI quirk:** workflow files reference `gsd-sdk` but the actual
  binary is `gsd-tools.cjs` at
  `~/.claude/get-shit-done/bin/`. Invoke via `node`. See memory
  `gsd_tools_install.md`.

## What just happened (immediately before second clear)

1. swg-main submodules finished cloning. We diffed against whitengold
   and confirmed swg-main is **server-only** — no client patches to
   adopt. Findings in `docs/recon/07-swg-source-diff-findings.md`.
2. User approved **Framing B** (CMake + VS 2022, copy swg-main
   patterns). Strategy memory updated.
3. `/gsd-new-project` invoked. `gsd-tools` CLI installed via npx (it's
   not pre-installed; binary lives at
   `~/.claude/get-shit-done/bin/gsd-tools.cjs`).
4. Init query returned `is_brownfield=true`,
   `needs_codebase_map=true`, `agents_installed=true`,
   `project_exists=false`.
5. User chose **"Run /gsd-map-codebase first"** at the workflow's
   codebase-map gate. Skill exited.
6. **Context cleared here.**

## First action when context resumes

### Step 1 — run `/gsd-map-codebase`

The user already opted in to running `/gsd-map-codebase` before
re-entering `/gsd-new-project`. Just invoke it. When it asks for scope,
nudge it toward the **client side** — the user is running the server
elsewhere via swg-main, so server analysis is informational only.

Have the mapper agents **consume** existing recon rather than re-discover:

- `CLAUDE.md` — repo guide
- `docs/research/swgclient-build.md` — build chain
- `docs/research/runtime-middleware.md` — middleware classification
- `docs/research/animation-system.md`, `foliage-system.md` — known in-house components
- `docs/recon/05-client-boot-sequence.md` — 17 boot phases
- `docs/recon/07-swg-source-diff-findings.md` — swg-main is server-only

Output: `.planning/codebase/{ARCHITECTURE,STACK,QUALITY,CONCERNS}.md`.

### Step 2 — re-invoke `/gsd-new-project`

Same description as before:

```
/gsd-new-project SWG client build modernisation. Strategy: Framing B
(CMake + VS 2022, copy swg-main patterns where applicable). Phase 1
target is MVB-0 (compile only). Background already in CLAUDE.md,
docs/research/, docs/recon/, and now .planning/codebase/.
```

The skill will see `.planning/codebase/` exists, skip the map gate,
proceed to deep questioning → PROJECT.md → workflow config →
research → REQUIREMENTS.md → roadmap.

### Step 3 — proposed phase 1 scope (already pre-decided)

When the workflow asks about phase scope, the answer is already
prepared from `docs/recon/07-swg-source-diff-findings.md`:

- **Goal:** `cmake --build build --config Release` produces `SwgClient_r.exe`.
- **In scope:** CMake authoring for client tree + find-modules for
  vendored SDKs (DX9, Miles, Bink, Vivox, libMozilla, DPVS, lcdui,
  STLPort) + flag tuning per swg-main pattern + relax `WarnAsError`
  initially.
- **Out of scope:** running the client; modifying C++; client.cfg;
  libMozilla stubbing; TRE assets; server-side anything.
- **Pass/fail:** link.exe produces an exe.
- **Estimated effort:** 2–4 weeks.

## Recovery — if memory is fragmentary

If something feels missing, the chain to walk is:

1. `CLAUDE.md` (root of repo) — top-level orientation
2. `docs/research.html` — index of all deep dives
3. `docs/research/swgclient-build.md` — what compiles
4. `docs/research/runtime-middleware.md` — what's contained vs pervasive
5. `docs/recon/05-client-boot-sequence.md` — what runs in what order
6. **This file** — why we're stopped here

Memory items in `~/.claude/projects/C--Code-whitengold/memory/`:

- `MEMORY.md` — index
- `swg_source_reference.md` — `C:\Code\swg-main` location and submodule layout
- `project_swg_client_build.md` — minimal-change strategy

## Things deliberately *not* done yet

- Did not start a gsd project (waiting on diff).
- Did not modify any source file in `whitengold\src\`.
- Did not customise `client.cfg`.
- Did not pick a target toolchain.
- Did not pick a stub strategy for libMozilla / Vivox / Bink.
- Did not write Phase 1 plan.

All of these are explicitly the next session's responsibility, in the
order above, with the diff result informing each call.

## Health check before moving on

Sanity questions to confirm context is intact:

1. Does the user still want minimal-change-first? (yes per memory)
2. Will server testing still happen via the SWG-Source VM? (yes)
3. Is the user OK with VS 2005 if that's the lowest-friction path? (unconfirmed — ask before commit)
4. Has the user changed their MVB target from MVB-0 (compile only)? (unconfirmed — ask)
