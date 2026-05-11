# Phase 10: DPVS Culling Experiment - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-10
**Phase:** 10-dpvs-culling-experiment
**Tree:** v2 (`D:/Code/swg-client-v2/`), branch `koogie-msvc-cpp20-base`
**Areas discussed:** Profiling methodology, Test scenes + sample protocol, Decision threshold, Removal mechanism

---

## Profiling methodology

### GPU-side timing

| Option | Description | Selected |
|--------|-------------|----------|
| D3D9 timestamp queries in-engine | IDirect3DQuery9 with D3DQUERYTYPE_TIMESTAMP/TIMESTAMPDISJOINT around DPVS/scene-render block. In-tree, reproducible, scripted. | ✓ |
| Intel GPA + PIX Legacy external capture | Frame-capture analysis tools. Heavy workflow but full bottom-up GPU breakdown. | |
| Both — in-engine + external one-time deep dive | Use in-engine for A/B numbers; one external capture per condition for qualitative read on draw-call delta. | |

**User's choice:** D3D9 timestamp queries in-engine
**Notes:** D3D9 makes external GPU profilers awkward (RenderDoc dropped D3D9 in 1.20). Recommended path.

### CPU-side timing

| Option | Description | Selected |
|--------|-------------|----------|
| Existing ProfilerBlock + new QPC timer pair | Use ms_dpvsQueryProfilerBlock for engine signal; add QueryPerformanceCounter pair flanking resolveVisibility() for raw µs. Both write to log. | ✓ |
| ProfilerBlock alone, just enable its existing output | Just turn on the existing Profiler dump. Less invasive; risks Profiler accumulation logic obscuring per-frame distribution. | |
| New QPC timer pair only | Skip Profiler entirely; one new timer pair. Cleanest data; loses Profiler's frame-time context. | |

**User's choice:** Existing ProfilerBlock + new QPC timer pair

### Output channels

| Option | Description | Selected |
|--------|-------------|----------|
| Log file + on-screen overlay | CSV per-frame log + overlay showing flag state and capture indicator. Best for real-time confirmation and offline analysis. | ✓ |
| Log file only, no overlay | Simpler. Risk of misaligned samples if camera wanders out of test scene. | |
| On-screen running stats only | Live medians/p95 displayed; screenshot to capture. Loses ability to re-aggregate. | |

**User's choice:** Log file + on-screen overlay

### Statistics

| Option | Description | Selected |
|--------|-------------|----------|
| Per-frame raw + distributional summary | Keep raw CSV; compute median, p95, p99, max, stdev. Distribution shape matters. | ✓ |
| Distributional summary only | Compute on-the-fly, drop raw. Smaller artifacts; loses re-bin/correlation. | |
| Mean + stdev only | Traditional. Insufficient for long-tail-dominated frame data. | |

**User's choice:** Per-frame raw + distributional summary

---

## Test scenes + sample protocol

### Test scenes

| Option | Description | Selected |
|--------|-------------|----------|
| Single scene: Mos Eisley cantina plaza | REQUIREMENTS canonical pick. One clean A/B. | ✓ |
| Two scenes: Mos Eisley + Coronet starport portal | Adds portal-traversal-heavy scene. ~2x capture time. | |
| Three scenes: + Theed open plaza | Triangulates decision space. ~3x capture time; multi-week risk. | |

**User's choice:** Single scene: Mos Eisley cantina plaza

### A/B protocol (originally asked; rejected, reformulated)

| Option | Description | Selected |
|--------|-------------|----------|
| Fixed camera transform + same time-of-day + same NPC seed | Recommended at first ask. | (Rejected — user asked who drives capture) |
| Static screenshot scene | Strongest fairness, least realistic. | |
| Best-effort same-position (no scripting) | Cheapest, most variance. | |

**User's choice:** None — user asked "Are you going to drive the capture settings or am I?" which reframed the question. See "Automation level" below.
**Notes:** Reframe: Claude can't run the client. Kenny drives. The fairness question becomes a protocol-doc concern; the implementation question is how much to automate.

### Automation level (reformulated question)

| Option | Description | Selected |
|--------|-------------|----------|
| Keybind-toggle capture, manual everything else | F10 toggle capture; F11 toggle OCC flag; console-set run-label. User picks position/moment/duration. | ✓ |
| Keybind-toggle + auto-stop after N frames | F10 starts capture; auto-stops after fixed frame count. Removes "how long" from protocol. | |
| Scripted camera path replay | Record-and-playback camera path. Strongest fairness; ~1-2 days of engine work. | |

**User's choice:** Keybind-toggle capture, manual everything else

### Runtime A/B toggle

| Option | Description | Selected |
|--------|-------------|----------|
| New F11 keybind → setDisableOcclusionCulling() | Setter exists at RenderWorld.cpp:1151. Instant toggle, no relog. | ✓ |
| Console command /toggleocclusion | Same effect via CommandParser. More discoverable, more files to touch. | |
| client.cfg restart-required toggle | Use existing config key + restart. Zero new code; severely worsens fairness. | |

**User's choice:** New F11 keybind → setDisableOcclusionCulling()

### Sample count

| Option | Description | Selected |
|--------|-------------|----------|
| 3 passes per condition, ~10s each | 6 captures total. Absorbs short-term variance; ~2 min of in-game capture. | ✓ |
| 1 pass per condition, ~30s each | 2 captures total. Cheaper; risks single outlier. | |
| 5 passes per condition, ~10s each | 10 captures total. Strong noise rejection; more friction. | |

**User's choice:** 3 passes per condition, ~10s each

---

## Decision threshold

### Primary metric

| Option | Description | Selected |
|--------|-------------|----------|
| Total frame time, median + p95 | What "fps regression" means in the ROADMAP. CPU/GPU components recorded but verdict on total. | ✓ |
| CPU frame time only | Only CPU headroom matters. Risky: ignores GPU stalls DPVS avoids. | |
| GPU frame time only | Symmetric to above. Less defensible. | |

**User's choice:** Total frame time, median + p95

### Threshold rule

| Option | Description | Selected |
|--------|-------------|----------|
| DPVS-off median+p95 both ≤ DPVS-on by any margin → remove | Strictest "no regression (or gain)". Sidesteps noise-floor band. | ✓ |
| Tolerance band: off worse by <0.5ms median AND <1.0ms p95 still removes | More aggressive about removal; requires band size pick. | |
| Eyeball it after seeing the numbers | Defer until data is in hand. Risks moving goalposts. | |

**User's choice:** DPVS-off median+p95 both ≤ DPVS-on by any margin → remove

### Inconclusive outcome

| Option | Description | Selected |
|--------|-------------|----------|
| Default to keep | Mixed/within-noise results → status quo wins ties. | ✓ |
| Capture more zones | Expand to Coronet/Theed before deciding. Longer experiment. | |
| Split outcome (off-by-default config, no code removal) | Flip default to true without source-delete. Pollutes config with untested toggle. | |

**User's choice:** Default to keep

### Revisit trigger

| Option | Description | Selected |
|--------|-------------|----------|
| Decision is for D3D9 only — Phase 11 re-runs the experiment | Verdict carries a "valid for D3D9" caveat; Phase 11 must re-measure. | ✓ |
| Decision is final — D3D11 inherits whatever Phase 10 decided | Lower friction; risks D3D9-specific decision under D3D11. | |
| Decision is final + DPVS library deletion in v3 | Premature; portal traversal still needs DPVS types. | |

**User's choice:** Decision is for D3D9 only — Phase 11 re-runs the experiment

---

## Removal mechanism (if 'remove')

### Removal shape

| Option | Description | Selected |
|--------|-------------|----------|
| Source-edit: drop OCCLUSION_CULLING from cullingParameters bitmask in RenderWorld.cpp:906 | Single-line source edit. resolveVisibility() still runs (VFC + portals); occlusion queries don't. Matches roadmap intent precisely. | ✓ |
| Same + flip disableOcclusionCulling config default to true | Same effect plus a config breadcrumb. Slightly noisier. | |
| Config-only: flip default to true, no source edit | Zero code change. Bit-flag plumbing stays in hot path forever. | |

**User's choice:** Source-edit drop OCCLUSION_CULLING from cullingParameters bitmask in RenderWorld.cpp:906

### Config-key fate

| Option | Description | Selected |
|--------|-------------|----------|
| Delete disableOcclusionCulling from ConfigClientGraphics | Once OCC is unconditionally off, the key is misleading dead toggle. Clean closure. | ✓ |
| Keep as inverted debug knob ("enableOcclusionCullingExperiment") | Adds a feature-flag with no production use. Bloat. | |
| Keep untouched | Silent dead config. Worst of all worlds. | |

**User's choice:** Delete disableOcclusionCulling from ConfigClientGraphics

### Instrumentation fate

| Option | Description | Selected |
|--------|-------------|----------|
| Remove after verdict, behind a single commit | Phase 10 scaffolding; verdict closes the question. Don't leave dead profiling toggles. | ✓ |
| Keep as permanent perf-instrumentation feature | Gate behind #ifdef. Modest maintenance debt; reusable in v3+. | |
| Keep CSV + timestamp queries only; drop toggle/overlay | Compromise: keep measurement primitives; drop experiment UI. | |

**User's choice:** Remove after verdict, behind a single commit

### Verdict doc location

| Option | Description | Selected |
|--------|-------------|----------|
| docs/recon/10-dpvs-profiling.md | Path named in ROADMAP success criterion #2. Sibling format to existing recon docs. | ✓ |
| Phase artifact only: 10-SUMMARY.md | Tighter GSD integration; diverges from named success-criterion path. | |
| Both: SUMMARY.md links out to docs/recon/10-dpvs-profiling.md | Two writes; canonical location stays per ROADMAP. | |

**User's choice:** docs/recon/10-dpvs-profiling.md (canonical per ROADMAP)

---

## Claude's Discretion

User did not invoke "you decide" explicitly. Implementation details left to researcher/planner are enumerated in CONTEXT.md `<decisions>` § "Claude's Discretion":
- Exact CSV file path on disk
- Overlay rendering surface (SwgCuiHud / debug overlay / DebugPrimitive text)
- Console command grammar for `/setrunlabel`
- Where F10/F11 keybindings wire (clientDirectInput / SwgCuiHud)
- Whether to capture DPVS visible-object counts as diagnostic
- Microsecond vs millisecond unit choice in CSV

## Deferred Ideas

- Multi-zone study (Coronet starport, Theed plaza) — rejected in favor of single-zone scope (D-05); v3+ revisit if single-zone is inconclusive
- Scripted camera-path record/replay — rejected as out of proportion for a profiling experiment
- Permanent profiling instrumentation post-verdict — rejected per D-15; don't leave dead profiling toggles
- DPVS library deletion — pinned out-of-scope by REQUIREMENTS for v2; v3+ candidate
- Re-running Phase 10 under D3D11 — owned by Phase 11 success criteria, not v3+
- Draw-call-count diagnostic column — optional; researcher discretion

## Procedural reframe

The originally-presented "A/B protocol" question (camera transform, time-of-day, NPC seed) was rejected by the user with the question "Are you going to drive the capture settings or am I?" This reframed the question from a code-fairness concern to a procedural-protocol concern, since Claude cannot drive the client. The replacement question — automation level — is what got answered. The protocol doc (`docs/recon/10-dpvs-profiling.md` per D-16) will document Kenny's capture procedure, not the code; the code-side response is just the keybind-toggle + console-label pattern of D-06/D-07.
