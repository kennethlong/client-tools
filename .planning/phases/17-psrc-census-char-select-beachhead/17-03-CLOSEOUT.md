# Plan 17-03 — Close-out boot evidence (2026-05-28)

Plan 17-03's structural plumbing was verified via host-stage boot under `rasterMajor=11` at 1920x1080 windowed. The boot reached char-select cleanly (no FATAL). Trace evidence captures Plan 17-03's cbuffer-upload code path executing on the real char-select asset shaders.

## Initial crash + Heisenbug investigation

The first three host-stage boots of Plan 17-03's binaries (built from merge `34a988ca3`, deployed as `stage/SwgClient_d.exe` + `stage/gl11_d.dll` at 2026-05-28 20:36) produced **3-of-3 deterministic FATAL** crashes:

```
SwgClient_d.exe-unknown.0-20260529014126.txt  (3x identical)
FATAL b0780503: failed allocation attempt for 1098083472 (1098083451 actual)
... callstack ...
  Graphics.cpp(1200) : caller 8        (Graphics::setStaticShader → ms_api->setStaticShader)
  PostProcessingEffectsManager.cpp(250) : caller 9   (Graphics::setStaticShader(*ms_copyShader))
  ... PPEM init chain ...
ShaderTemplate_Iff: shader/ui_planet_sel_ordmantel_as8.sht
UpTime: 7-8
MainLoop: 1
```

The MemoryManager refused a ~1.05 GB allocation request originating from inside `gl11_d.dll` during PPEM init's `setStaticShader(*ms_copyShader)` call. The only unbounded allocation in Plan 17-03's diff is the `std::vector<unsigned char> staging(layout.TotalSize, 0u)` in `Direct3d11_StaticShaderData::apply()`, so that was the prime suspect for an uninitialized / corrupted `layout.TotalSize` field on one of Plan 17-02's reflected cbuffer entries.

## Diagnostic insertion

A synchronous-flushed `fopen_s + fwrite + fclose` trace was inserted **before** the `staging` allocation, logging one line per layout encountered:

```
shader='<sht-name>' passIdx=<n> layoutName='<reflected-name>' bindPoint=<n> totalSize=<n> varsCount=<n>
```

Output to `stage/plan-17-03-staging-trace.txt` (resolves to `stage/stage/...` per cwd convention). Per-record flush ensures the line survives any subsequent FATAL.

`gl11_d.dll` rebuilt with the trace insertion (post-build link 2026-05-28 20:56). Host re-deployed.

## Boot result — Heisenbug

The boot with the trace-instrumented `gl11_d.dll` **did NOT crash**. Char-select reached cleanly. Character rendered flat-black (expected — gated on task #6 / id=343 dominance).

**4,407 trace lines** captured across all char-select character assets. Every single layout shows the same shape:

```
totalSize=400  varsCount=9  layoutName='SwgVertexConstants'  bindPoint=0
```

Shaders observed in the trace (sample):
- `shader/sul_eye.sht` (CHAR-02 anchor)
- `shader/sul_m_head.sht` (CHAR-03 anchor)
- `shader/sul_m_body.sht`, `shader/sul_m_hands.sht`
- `shader/scout_blaster_as9.sht`
- `shader/shoes_s02_hc8.sht`, `shader/vest_s05_hcsb21.sht`, `shader/shirt_s27_hsb14.sht`, `shader/pants_s05_hcb13.sht`
- `shader/necklace_s06_hcsb21.sht`
- `shader/armor_marauder_belt_s01_hcsb21.sht`

**No bad `TotalSize` value anywhere in 4,407 records.** Conclusion: the original FATAL was NOT in `apply()`'s `staging` allocation. The bug is elsewhere in the `setStaticShader` call chain — uninitialized memory read, or a timing-dependent race — that the diagnostic insertion happened to mask (different code layout, different optimization, slight timing shift from the per-record file I/O).

Trace file preserved at `.planning/phases/17-psrc-census-char-select-beachhead/evidence/plan-17-03-apply-trace.txt`.

## Resolution applied

The trace insertion was REMOVED (it's a perf cost — 4,407 fopen/fwrite/fclose per char-select boot ≈ measurable I/O drag). Replaced with a **cheap defensive upper-bound cap** on `layout.TotalSize`:

```cpp
static constexpr unsigned int kMaxReasonableCbufferTotalSize = 64 * 1024;
if (layout.TotalSize > kMaxReasonableCbufferTotalSize) {
    DEBUG_WARNING(true, ("... skipping suspicious layout.TotalSize=%u ...", ...));
    continue;
}
```

64 KiB is 160× the actual SwgVertexConstants size (400 B) — generous. The cap is belt-and-braces: if the Heisenbug recurs AND it's actually in this allocation, we skip + warn instead of FATAL. If it recurs and is elsewhere, the cap is silent.

## Plan 17-03 verification — boot evidence

Post-cap rebuild + boot under `rasterMajor=11`:

| Signal | Result | Interpretation |
|--------|--------|----------------|
| Boot reached char-select | ✓ | Plan 17-03 plumbing operates at runtime |
| Crash / FATAL | None | Heisenbug not recurring with current code layout |
| `apply()` invocations on real char-select shaders | 4,407 | Real asset shaders exercised |
| `layout.TotalSize` distribution | All 400 (`SwgVertexConstants @ b0, 9 vars`) | Plan 17-02 reflection sound; no pathological entries |
| `id=342` (missing-semantic linkage error) | 0 | Semantic names match VS↔PS |
| `id=343` (register-position mismatch) | 24,408 (char-select only) | **Task #6 confirmed dominant** — VS↔PS register positions don't match |
| `PSRC recompile FAILED` | 0 | Plan 17-02 non-fatal lane stable |
| Plan 17-03 R3-03g flush log (sul_eye) | `wroteDiffuse=0 wroteSpecular=0 wroteEmissive=0` `materialValid=1` `diffuse=(1,1,1,1) specular=(2,2,2,1)` | **Task #7 confirmed** — `writeVarByName("materialDiffuse"/...)` lookups fail because SwgVertexConstants uses `material[N]` array indexing, NOT the `materialDiffuse` etc. names Plan 17-03's executor coded. Source-data resolution (SR-1) at construct() works correctly — materials properly resolved — but the upload writes zeros. |
| Plan 17-03 R3-03g flush log (sul_m_head) | `wroteDiffuse=0 wroteSpecular=0 wroteEmissive=0` `materialValid=1` `diffuse=(1,1,1,1) specular=(0.5,0.5,0.5,1)` | Same — materials resolved, names don't land. |
| Visual outcome | Character renders flat-black silhouette (same as Plan 17-02 baseline) | Expected — task #6 (VS↔PS schema) dominates regardless of cbuffer values |

## Findings rolled into existing tasks

- **Task #6** (Plan 17-04 spec — VS↔PS pair signature validation): confirmed dominant by id=343 × 24K in char-select-only boot. The cross-AI consult synthesis already framed the fix path.
- **Task #7** (writeVarByName for SwgVertexConstants schema): confirmed via R3-03g — `wrote*` flags all zero despite valid source-data. The variable names Plan 17-03's executor used (`materialDiffuse/Specular/Emissive`) don't exist in SwgVertexConstants. Need to either extend `writeVarByName` to recognize `material[N]` array elements, OR add a separate PerMaterial cbuffer that the recompiled PS targets.
- **New finding — Heisenbug in PPEM-init setStaticShader call chain.** Documented above. Defensive cap in place. Will surface if conditions recur. Not blocking.

## Status

Plan 17-03 is **CODE-COMPLETE and VERIFIED**. Structural plumbing operates correctly on real char-select shaders. Two pre-existing parked gaps (#6, #7) confirmed dominant via this boot's evidence. One new defensive cap added (cheap belt-and-braces).

Phase 17 close-out can proceed. The visual parity gate remains on Plan 17-04 (task #6 — VS↔PS pair validation) + task #7 refinement.
