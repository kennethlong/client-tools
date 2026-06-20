# Session handoffs

## Active handoffs

- [2026-06-20-32bit-cantina-leak-NEXT.md](2026-06-20-32bit-cantina-leak-NEXT.md) — **NEXT UP. Get the 32-bit client working in the cantina.** Surfaced while testing the door-snap fix: 32-bit Release in the cantina shows a transient camera-fling-to-sky + crackly audio + lag, NOT in 64-bit. Diagnosis (high confidence): the **known pre-existing Phase-32 D3DCompile per-cell recompile heap-fragmentation leak** → 2 GB pressure → hitches (32-bit-specific ceiling + cantina-specific shader recompile). **NOT the door-snap fix.** Fix candidate: port gl11's compile-once shader bytecode cache to gl05. See the handoff.
- [2026-06-20-cantina-door-snap-FIX-COMPLETE.md](2026-06-20-cantina-door-snap-FIX-COMPLETE.md) — **DONE 2026-06-20, committed `3549c7104`.** The cantina door-snap was **TWO stacked bugs**: (1) avatar floor-collision rubberband — footprint **circle** grazes an uncrossable floor-mesh seam while the **center** walks clean (`PWR_WalkOk`); fixed by suppressing that contact only on a **shallow graze** (`cs_seamGrazeEpsilon`); and (2) chase-camera instant snaps (zoom pull-in + shoulder zero), fixed by **rate-limiting the inward ease** (`cs_cameraPullInSpeed`). The resolver slide was never broken; framerate was a real factor for bug 1 only. Root-caused by runtime probes, **4-AI code-reviewed**, built clean in all 4 configs (Win32/x64 × Debug/Release), user-verified. All `CORNERSNAP-*` probes removed. Remaining: commit + a systemic non-dt-scaled-constant sweep (backlog). Tunables ref: `spikes/door-snap-fix-tunables.md`.
- [2026-06-20-cantina-door-snap-spike-START.md](2026-06-20-cantina-door-snap-spike-START.md) — _historical START handoff (root-cause findings before the fix); superseded by FIX-COMPLETE above._

- [login-music-miles-9.3v-research.md](login-music-miles-9.3v-research.md) — **RESOLVED 2026-06-18 (`5a894d327`).** Login/character-select SCREEN music now plays. The 9.3v lead was a **red herring** — the bug was engine-side, not a Miles version difference: Miles' background IO thread re-opens the stream via our `AIL_set_file_callbacks` hook with an **empty filename** → `TreeFile::open("")` returned NULL → buffers never filled. Fix (`clientAudio/Audio.cpp`, gated `s_titleMusicStreamFix`): remember the real stream name + serve the empty-name re-open from it + track/seek read offset across re-opens + close prior handles (leak guard). Verified: plays start-to-finish, transitions to zone music, handles bounded. (9.3v was later adopted for x64 in `984afc073` for a **separate** in-game 3D-audio mixer bug.) Handoff kept as historical record.
- [phase-32-d3dcompile-port.md](phase-32-d3dcompile-port.md) — **Phase 32 (D3DX → d3dcompiler_47).** 32-01 GATE = PASS + 32-02 (D3DCompile / HlslRewrite / X4016 fix / VS bytecode cache) COMPLETE & committed; renders correctly (user-verified). Hit a **32-bit Debug-client memory issue** — D3DCompile's per-compile heap fragmentation bloats first cantina load past the 2 GB ceiling (~59s); the bytecode cache fixes re-entry recompiles but not first-load misses. **Moving forward anyway: deferred to x64 (NOT reverted) — the x64 port removes the 2 GB ceiling and dissolves the OOM.** Next session = v3.0 x64 milestone (Phase 33 `x64-build-platform`). 2026-06-17.

_(Completed-work handoffs remain in this directory as historical records.)_

## Reference docs

Standalone reference/investigation docs created alongside the work above (not handoffs — kept for
lookup):

### Cantina door-snap fix (2026-06-20)
- [../spikes/door-snap-fix-tunables.md](../spikes/door-snap-fix-tunables.md) — **the new tunable
  config values** (`cs_seamGrazeEpsilon`, `cs_cameraPullInSpeed`): what each does, how to tune, and the
  pre-existing values they interact with.
- [../spikes/MANIFEST.md](../spikes/MANIFEST.md) — spike index + the two-bug outcome summary.
- `../spikes/001..006/README.md` — the spike chain (mechanism probe → resolution probe → faceB-fix →
  camera-zoom-lurch → time-scale → hysteresis).
- `../research/CONSULT-36/37/38/39-doorsnap-*SYNTHESIS.md` — root-cause + fix-selection consult rounds.
- `../research/CONSULT-40-camera-lurch-framerate-EVIDENCE.md` — the framerate angle + systemic
  non-dt-scaled-constant scan.
- [../research/CONSULT-41-REVIEW-SYNTHESIS.md](../research/CONSULT-41-REVIEW-SYNTHESIS.md) +
  `CONSULT-41-doorsnap-fix.diff` — the 4-AI code review of the final fix.