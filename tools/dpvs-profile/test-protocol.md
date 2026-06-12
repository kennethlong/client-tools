# DPVS Profiling — Capture Protocol

> **Read this top-to-bottom at capture time.** The capture session is the
> validation of DPVS-01 (Phase 23, D3D11 re-measure) and — historically — the
> Phase 10 goal per `.planning/phases/10-dpvs-culling-experiment/10-VALIDATION.md` §Manual-Only Verifications.
>
> **Phase 23 update (2026-06-12):** This protocol now drives the **D3D11**
> re-measure. Read the *Phase 23 Delta* section immediately below FIRST — it
> overrides the Phase 10 specifics (renderer, F11 target, scene set) wherever
> they conflict. The original Phase 10 body is retained verbatim below the
> delta as the historical decision trail and the baseline-table reference.

---

## Phase 23 Delta — D3D11 re-measure (READ FIRST, overrides Phase 10 below)

This is the live capture for **DPVS-01**: re-run the occlusion-vs-no-occlusion
A/B under the D3D11 renderer and record a keep/remove verdict that confirms or
revises the Phase 10 Option α decision (remove outdoor occlusion, keep indoor
portals). What changes from the Phase 10 body:

1. **Renderer = `rasterMajor=11` (gl11), Debug build.**
   Launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe`. The **Debug** exe reads
   **`stage/client_d.cfg`** (NOT `client.cfg` — that is the Release exe; memory
   `feedback_debug_exe_reads_client_d_cfg`). Set `rasterMajor=11` in
   `stage/client_d.cfg`. Debug is intentional: the F10/F11 toggle, the occlusion
   re-gate, and the visible-object count are all `_DEBUG`-only, and the verdict
   turns on the **relative** ON-vs-OFF delta (which Debug preserves), not absolute
   fps (23-RESEARCH Open Question 1 / Pitfall 3). Note the Debug caveat in the
   verdict doc.

2. **F11 calls `RenderWorld::toggleForceDisableOcclusionCulling()`** — the
   surviving-flag accessor introduced in Plan 23-01. (The Phase 10 D-07 target
   `RenderWorld::setDisableOcclusionCulling()` was DELETED by Option α and no
   longer exists; do not look for it.) F11 flips the surviving
   `ms_forceDisableOcclusionCulling` DebugFlag; the `_DEBUG` `cullingParameters`
   branch ORs `DPVS::Camera::OCCLUSION_CULLING` gated on it. The shipped
   Release Option α `#else` branch is untouched.

3. **Scene set: at minimum one OUTDOOR (Mos Eisley plaza) AND one INDOOR
   (cantina interior).** Option α is fundamentally an outdoor-vs-indoor
   tradeoff, so a single scene cannot confirm or revise it (23-RESEARCH Open
   Question 3). All four Phase 10 scenes (plaza, starport, walking, cantina) are
   ideal; outdoor-plaza + indoor-cantina is the defensible minimum.

4. **≥3 passes per condition, alternating ON-OFF-ON-OFF-ON-OFF**, retained from
   D-08. Label each pass `/setrunlabel <scene>-pass<i>-on` /
   `<scene>-pass<i>-off`. The **`-on` / `-off` suffix is what `analysis.py`
   groups on** — keep it exact. Use a distinct `<scene>` prefix per scene
   (e.g. `mosEisley`, `cantina`) so the two scenes' CSVs don't pool together.

5. **`gpu_us` is expected to be 0 in every row** — DPVS is CPU-only software
   occlusion (Umbra issues no draw calls; 23-RESEARCH Pitfall 1). An all-zero
   `gpu_us` column is the CORRECT answer, not a measurement failure. The DPVS
   cost lives in `cpu_qpc_us` and `total_frame_ms`. Additionally, the
   **`dpvs_occlusion_flag` CSV column is authoritative over `run_label`**: if the
   inverted-default trap (Pitfall 4 — client launches with occlusion OFF by
   default, persisted in `local_machine_options.iff`) mislabels a pass, trust
   the flag column, not the typed label.

6. **gl11 noise guidance (Pitfall 5):** the D3D11 path has known intermittent
   furniture LOD-thrash flicker and async-asset load races that inject
   single-frame transients. Hold the camera still ~10 s per pass and rely on
   **median + p95** (single-frame outliers don't move them). **Re-capture a side
   if the median across same-side passes differs by >30%.**

7. **Optional fresh same-session D3D9 baseline (23-RESEARCH Open Question 2):**
   for a clean same-hardware A/B/A you may also capture a `rasterMajor=5` (gl05)
   pass per scene. If skipped, the Phase 10 baseline is the **summary tables** in
   `docs/recon/10-dpvs-profiling.md` §Summary Statistics (the raw Phase 10 CSVs
   were gitignored and are gone).

**Post-capture (Phase 23):** run `analysis.py` against `stage/dpvs-profile/` as
below, then write the verdict to **`docs/recon/23-dpvs-d3d11-profiling.md`**
(NEW doc, sibling to the Phase 10 doc), stating whether Option α is confirmed or
revised under D3D11.

---

## Phase 10 historical protocol (baseline reference)

> The section below is the original Phase 10 D3D9 protocol, retained verbatim as
> the decision trail and the source of the baseline summary tables. Where it
> says "Mos Eisley plaza only", "F11 → setDisableOcclusionCulling", or
> "rasterMajor=5/D3D9", the **Phase 23 Delta above takes precedence**.
>
> Decision-ID trail: this protocol implements **D-05** (scene = Mos Eisley
> plaza), **D-06** (keybind-toggle capture, no scripted replay), **D-07**
> (F11 calls `RenderWorld::setDisableOcclusionCulling()`), **D-08** (3 passes
> per condition, ~10 s each, alternating ON-OFF-ON-OFF-ON-OFF).

---

## Pre-Session Checklist

Tick each line before launching the client. Any miss → fix first, then start.

- [ ] **SWGSource VM running at 192.168.1.200:44453.** Per memory
  `project_vm_server_setup.md`: VM up, manual IP assigned, Oracle started,
  stationchat running, `ant` build current in the VM, login server +
  central + planet servers visible.
- [ ] **Client binary current.** `D:/Code/swg-client-v2/stage/SwgClient_d.exe`
  built from current `koogie-msvc-cpp20-base` HEAD with Wave 1+ DPVS
  instrumentation applied. (Wave 0 ships only scaffolding; the F10/F11
  bindings + CSV writer + overlay land in Wave 1, plan 10-02.)
- [ ] **Stage dir prepared.** `D:/Code/swg-client-v2/stage/dpvs-profile/`
  exists and is writable. **Empty it before the session** so you have a clean
  set of 6 CSVs after capture (no leftovers from prior attempts).
- [ ] **Config knob set.** `client_d.cfg` has
  `[ClientGraphics/Dpvs] reportInstrumentation=true` (turns on the overlay
  and the CSV writer per RESEARCH.md Q2).
- [ ] **DebugMonitor visible.** The child overlay window is showing — Wave 1
  smoke verifies this works; if it doesn't render after build, see
  Troubleshooting.

---

## Launch Sequence

1. Launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe`. Wait for the loading
   spinner to clear; client should sit at the SOE/SOE-replacement login UI.
2. Log in with your test account. Pick a **Tatooine character**
   (Phase 9 closeout boot path; known-good zone).
3. Once on Tatooine, travel on foot to the **Mos Eisley cantina plaza**
   (the open paved area immediately outside the cantina entrance, just past
   the loading-zone door).
4. Pick a fixed landmark. Suggested anchor: **face the cantina door, camera
   locked at the same coords across all 6 passes**. Type `/loc` and record
   the values somewhere (notepad / index card) so you can resume the exact
   spot if the client crashes mid-session.
5. Wait for NPC spawns to populate the plaza. Visual check before starting:
   **at least 6–8 NPCs in the camera view** (mix of patrol stormtroopers,
   civilians, droids if present). NPC density is the primary driver of the
   DPVS-on/off cost delta; capturing into an empty plaza wastes the session.
6. Confirm the overlay shows: `DPVS:ON`, current `run_label`, and the
   capture indicator (`...` when idle, `REC` when F10-capture is on).

---

## Capture — 6 passes alternating ON-OFF-ON-OFF-ON-OFF (per D-08)

Each pass is **~10 seconds** of held-still camera = ~600 frames at 60 fps.

**Total session: 6 passes (3 per condition), ~1–2 minutes of capture wall
time + setup overhead. The alternating order absorbs time-of-day drift
(NPC spawner phase, weather, lighting cycle) so both conditions see the
same long-term population profile.**

For each pass index `i` in `{1, 2, 3}`:

### Pass `<i>` — DPVS:ON side

1. Verify overlay shows `DPVS:ON` (occlusion culling **enabled** — i.e.
   `disableOcclusionCulling=false`). If not, press F11 once to flip it.
2. Type into chat: `/setrunlabel mosEisley-pass<i>-on` then ENTER.
   - Overlay updates to show the new run-label.
3. Press **F10**. Overlay flips from `...` to `REC`.
4. **Hold camera still ~10 seconds.** Resist the urge to pan. NPC motion is
   the variable we want to capture; camera motion is noise.
5. Press **F10**. Overlay returns to `...`. Pass `<i>`-on is done.

### Pass `<i>` — DPVS:OFF side

6. Press **F11**. Overlay flips to `DPVS:OFF` (occlusion culling
   **disabled** — view-frustum culling and portal traversal **still active**;
   only the DPVS occlusion query is bypassed).
7. Type: `/setrunlabel mosEisley-pass<i>-off` then ENTER.
8. Press **F10**. `REC` overlay on.
9. **Hold camera still ~10 seconds.** Same anchor as on-side.
10. Press **F10**. `REC` off. Pass `<i>`-off done.
11. Press **F11** again. Overlay flips back to `DPVS:ON`, ready for the
    next pass.

### After pass 3

You should have **6 CSV files** under
`D:/Code/swg-client-v2/stage/dpvs-profile/`, named per the
DpvsProfileInstrumentation writer convention:

```
mosEisley-pass1-on-<frameStart>.csv
mosEisley-pass1-off-<frameStart>.csv
mosEisley-pass2-on-<frameStart>.csv
mosEisley-pass2-off-<frameStart>.csv
mosEisley-pass3-on-<frameStart>.csv
mosEisley-pass3-off-<frameStart>.csv
```

(Each file has 10 columns; row count ≈ 600 per file at 60 fps.)

---

## Post-Capture

1. Quit the client cleanly: `/quit` or Alt-F4. Avoid taskkill — it
   sometimes truncates the last CSV's final rows.
2. From the v2 tree run the aggregator:
   ```
   python D:/Code/swg-client-v2/tools/dpvs-profile/analysis.py \
     --csv-dir D:/Code/swg-client-v2/stage/dpvs-profile/
   ```
   (Add `--scene mosEisley` only if you changed the scene tag.)
3. Copy the printed markdown table into
   `docs/recon/10-dpvs-profiling.md` §Summary Statistics.
4. Read the **final stdout line** (`verdict = remove` or `verdict = keep`)
   and fill in §Verdict and §Rationale in the verdict doc per D-10 / D-11
   reasoning.
5. Per D-11, if the analysis warns about mixed signals / partial data, the
   verdict defaults to `keep`. Read any `WARNING:` lines on stderr before
   pasting numbers into the doc.
6. Commit the populated `docs/recon/10-dpvs-profiling.md` (Wave 3 closeout).

---

## Troubleshooting

- **No CSV file appears after F10.**
  Check `client_d.cfg` for `[ClientGraphics/Dpvs] reportInstrumentation=true`.
  Verify the `dpvs-profile/` subfolder exists under `stage/` and is writable.
  Verify the build actually included the Wave 1 instrumentation (`dumpbin
  /exports SwgClient_d.exe | findstr DpvsProfile` if the writer is a
  loose symbol; otherwise grep build output for `DpvsProfileInstrumentation`).

- **`gpu_us` column is empty in many rows.**
  D3D9 `GetData` returned `S_FALSE` (query not ready) per RESEARCH.md
  Pitfall 1. `analysis.py` tolerates blanks. Acceptable up to ~10 % of rows;
  if higher, bump the query-pool depth in `Direct3d9.cpp` from N=3 to N=5
  and rebuild.

- **Overlay does not show / DebugMonitor not visible.**
  Per RESEARCH.md Q2: confirm the DebugMonitor child window is enabled
  (check `client_d.cfg` for the `[DebugMonitor]` section; some Koogie
  branches default it off). Without overlay you can still capture (CSV
  writer is independent), but you lose the eyes-on confirmation of
  current DPVS state — risk of mis-labelling a pass.

- **Median µs values across the 3 same-side passes differ by >30 %.**
  Camera-position drift per RESEARCH.md Pitfall 6. Re-capture. Anchor
  camera more rigidly: lock to the `/loc` coords you noted in step 4 of
  Launch Sequence, and zoom level matters too (mouse-wheel one notch
  changes draw-call count materially).

- **F11 appears to do nothing.**
  Conflict with an existing binding. Check `client.cfg` `[ClientInput]`
  section for an F11 mapping; rebind the conflict or pick a different
  free key in the Wave 1 instrumentation.

- **Client FATALs partway through capture.**
  Save what you have (any complete CSVs in `stage/dpvs-profile/` are
  fine to keep), restart, and resume from the next pass index. The
  analysis script aggregates all CSVs in the directory; missing
  passes degrade statistical power but the verdict still computes.
  If the same pass keeps crashing, log as a Wave 1 deferred-items entry.

---

## Decision-ID Trail

This protocol implements:

- **D-05** — Scene = Mos Eisley cantina plaza (single zone; multi-zone
  expansion deferred to v3+ per CONTEXT.md §Deferred Ideas).
- **D-06** — Keybind-toggle capture; manual everything else (no scripted
  camera path replay).
- **D-07** — F11 calls `RenderWorld::setDisableOcclusionCulling()` at
  `clientGraphics/RenderWorld.cpp:1151`.
- **D-08** — 3 passes per condition, ~10 s each, alternating
  ON-OFF-ON-OFF-ON-OFF to balance time-of-day drift across the two pools.
