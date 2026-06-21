# CONSULT-46 SYNTHESIS — should we throttle ClientInteriorLayoutManager? (risk/value/design)

4 consultants, non-overlapping. A real Sonnet-vs-Cursor divergence on VALUE, resolved by direct code
check. Verdict: **DO it, via Opus's safe design — medium win, risk solvable.**

## Q1 RISK — solvable (Codex + Sonnet found the danger; Opus's design avoids it)
A NAIVE throttle holding raw `CellProperty*`/`TangibleObject*` across frames is genuinely unsafe:
- `TangibleObject::removeFromWorld()` deletes the whole interior-object list (`TangibleObject.cpp:495-506`).
- `CellProperty::removeFromWorld()` resets `m_appliedInteriorLayout` (`CellProperty.cpp:563-580`).
- Raw `CellProperty*` has no generation → ABA address reuse; fast in-out makes mid-creation teardown
  realistic → AV / props into freed cell / wrong building / silently-missing props, on the path that
  loads EVERY interior. (Codex + Sonnet converge.)
- **Opus's design neutralizes it:** store the resume CURSOR as a count ON `CellProperty` (manager holds
  ZERO cross-frame pointers; re-derives from the live visible-cell list each frame), and reset that
  count in the SAME `removeFromWorld` that already resets the applied flag + deletes the objects →
  no-dangle, no-dup, ABA-safe by construction (a reused address gets a fresh count from the ctor). No
  new teardown hook.

## Q2 VALUE — MEDIUM win (Cursor verified; Sonnet's "low ROI" was based on a wrong assumption)
- Burst size (Cursor, from the real `.ilf`): main `cantina` cell = **84 objects / 15 unique templates**
  in ONE frame; whole building 260/47. No per-frame cap today; `runtimeDisableAsynchronousLoader=true`
  → all synchronous on the main thread.
- **The divergence:** Sonnet assumed texture upload happens at first DRAW (decoupled) → throttle won't
  help. **VERIFIED FALSE:** `Texture::Texture()` calls `load()` in the ctor (`Texture.cpp:195`), and
  the texture is built DURING each prop's appearance construction inside the create loop → the GPU
  upload is ON the creation critical path. Throttling creation spreads it. (Also: managed-pool draws
  are gated by creation, so they spread too.)
- ROI: MEDIUM. Spreads ~15 unique first-load GPU touches + 84 per-object instantiations over N frames.
  Real, but not the ~44-compile cliff that #1 already removed. First-session-only.

## Q3 DESIGN — Opus's safe-by-construction throttle
- Count-on-CellProperty resume cursor (above); fix the `:84` bug (latch "applied" AFTER the loop
  completes, resume at `created(C)`); visibility flicker does NOT reset (only real unload does) → no-miss.
- Budget = config knob `maxInteriorCreatesPerFrame`, **default 0 = unlimited = byte-identical old
  behavior** → land off, enable to test. Recommend **8–12** (Cursor: 5–15 is the right order to spread
  the 84-object cantina cell). Per-frame refresh with ≥1 → monotonic progress, no starvation.
- Diff: ~1 header line + accessors on CellProperty.h; ~3 lines in CellProperty.cpp (init + reset beside
  the existing resets); rewritten loop + config read in the manager; one cfg line. **Cost: adding an
  int to CellProperty.h is a shared-header ABI change → rebuild all 4 plugins (gl05/06/07/11)** — the
  known cascade trap, manageable.

## RECOMMENDATION
DO #2, via Opus's design, default-off knob, budget 8–12. It's a real medium win on the first-session
stutter, the risk is fully mitigated by construction, and the off-by-default knob means we land it with
zero behavior change and flip it on to measure. Caveat: medium (not transformative) + a 4-plugin ABI
rebuild. If we'd rather not touch the all-interiors path at all for a medium win, the safe fallback is
"do nothing more" — #1 + #4 already took the big cliff and bounded the lurch.
