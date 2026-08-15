# CONSULT-74 ADVERSARIAL CLOSE — corrections to the synthesis

A fresh Fable re-derived the synthesis against both trees with the B-as-base political constraint
treated as fixed. [CONSULT-74-SYNTHESIS.md](CONSULT-74-SYNTHESIS.md) is the DRAFT; **this document
supersedes it wherever they conflict.** Errors are the draft author's.

Verification base: A = `f5f14c3f6`; B = `sais/x64-dx11-vanilla` = `4ce0386af`; ancestor `949451032`.

---

## 1. Corrections to the Q2 findings

**The device-loss comparison was half-checked and is wrong.** B's `checkForDeviceRemoved`
(`Direct3d11_Device.cpp:120-136`) is *also* an unconditional FATAL — "DX11 has no Reset()…
continuing produces a cascade of failures." B independently ratified A's design decision D-13. The
only real delta is that B fires the engine's lost/restored registries on **resize**; A releases and
recreates views directly, and A's resize path is live-verified (embed-resize, alt-tab).
**Strike this from the B-advantage column.**

**Colour write mask — defect real, severity overstated.** It is an **R↔B swap**, not a reversal:
G and A coincide. Masks symmetric under R↔B are unaffected, and those are the common ones — `0xF`,
`0x7` (RGB-only), `0x8` (alpha-only, including A's own `& ~ALPHA` path, coincidentally correct).
Only a mask containing exactly one of R or B breaks. **Severity LOW.** Fix is a 4-entry remap plus
deleting the false comment at `Direct3d11_StateCache.cpp:2328-2343`.

**`TextureFormatInfo` — provenance corrected.** Byte-identical in the common ancestor: a stock
upstream defect A inherited, not A-authored. It belongs on the "stock defects B fixed" ledger.
Both zero rows carry `supported=false`, so exposure is bytes-per-pixel = 0 in size math if a
renderer ever enables the float formats.

**Transform throttle — context wrong, adoption still right.** Both trees inherit
`frameRateLimit = 144.f` from upstream (`ClientMain.cpp:216`, identical at the ancestor), so the
"180–240 fps / 4–5 ms" framing does not apply. At 144 fps the send interval is ~7 ms and whether the
server tolerance trips is **unmeasured**. Downgrade to SUSPECTED; adopt the throttle regardless.

**`setDynamicIndexBufferSize` — wrong verb.** Zero engine callers: a dead API slot, not a live call
through null. Latent trap only.

## 2. Corrections to the merge arithmetic

The decomposition double-counted (131 + 45 + 12 + 83 = 271 ≠ 259). Correct: **130 vcxproj-only +
45 Direct3d11 (including its own vcxproj) + 84 others**; 3 of the 12 modify/deletes are vcxproj
already counted. Genuine hand-merge ≈ **75 files**, of which ~15 are convergent mechanical x64 fixes
to identical sites. **The genuinely hard set is ~10:** `WorldSnapshot.cpp`, `Game.cpp`,
`GroundScene.cpp`, `Graphics.cpp`, `MemoryManager.cpp`, `Archive.h`/`AutoDeltaMap.h`,
`DebugHelp.*`/`CallStack.*`, `CuiMediator.cpp`, `ShaderImplementation.{h,cpp}`, `WinMain.cpp`.

"Merging A's engine fixes into B is close to a clean apply" **overstates it**: 11 of ~14 sites apply
clean, but `CuiCombatManager.cpp`, `CuiMediator.cpp` and `TreeFile_SearchNode.cpp` conflict — and
the `CuiMediator` one is philosophical (A's re-entrancy guard vs B's FATAL-softening), not textual.

## 3. Renderer verdict, restated

**Not "B is architecturally better."** The supported claim is: **B is better factored and
documented; A is better proven; the two are behaviourally unmeasured against each other.** The
strongest B-advantage row in the architecture consult turned out half-false (§1), and a
well-documented tree read by a consultant will systematically score above a battle-scarred one —
a reading artifact that must be named. Every *measured* behavioural datum favours A (RenderDoc
capture-diff parity across a recorded bug list including space content; live-verified perf
catalogue). One B parity find favours B:

**New B→A candidate, confirmed.** D3D9 hardware clamped COLOR-semantic interpolants to [0,1];
D3D10+ does not, and the shipped corpus relies on it (`a_2blend_dirt.psh` ends `mul r0.rgb, r0, v0`
on unclamped `oD0`). B saturates centrally in the signature wrapper (`62708f7e7`). A saturates
per-stage FFP combiner results but has **no central COLOR-interpolant clamp**. Spec-correct parity,
cheap. Same family: B's half-pixel 2D offset restoration.

## 4. Conflict list, corrected

1. **MemoryManager** — take A's root-cause. Widen the scope: it is a **three-part memory-policy
   divergence** — pool bypass, soft-budget rewrite (`WinMain.cpp` 1536 MB cap → 75% RAM ≤ 12 GB +
   env override), and flipped instrumentation defaults.
2. **FreeChaseCamera** — A's work is the base; adopt B's optional-`INZM` read (A's mandatory `INZM`
   is a genuine fragility against NGE-retail assets); gate the `DEFH` heuristic and ground clamp off.
3. **RETITLE: the interior-binding cluster. A's position is STRONGER than the draft conceded.**
   B's in-code rationale for flipping `cellLoaded` to `true` is **contradicted by stock code**:
   `Object::addToWorld` runs the property loop before the attached-objects loop, and
   `PortalProperty::addToWorld` ends in `Container::addToWorld()` (`Container.cpp:484-492`), which
   walks container **contents** and adds every exposed cell regardless of child status — the design
   is documented at `Object.cpp:1099-1100`. `false` is what retail shipped for two decades of
   multiplayer. B's `true` additionally makes network-owned `CellObject`s parent-destructor-deleted
   and parent-alter-driven — a **double-ownership hazard**.
   But the flip is one member of a coordinated rework: `CellObject::endBaselines` FATAL → on-demand
   buildout load; WorldSnapshot POB-CRC mismatch now *proceeds* by default (**inverting the effective
   default of `worldSnapshotIgnorePobChanges`**); missing `.pob` → building silently non-portalized;
   GroundScene per-frame `setParentCell(worldCell)` eviction + 45/90 s force-finish loading.
   **Resolution:** A's `false` + A's dPVS set as the visibility base; evaluate B's on-demand buildout
   load as a separate keepable fix; reject the CRC-tolerance and cell-eviction defaults; re-run A's
   portal repro after merge with a second scenario — server-streamed **and** client-cached buildout.
4. **cbuffer convention** — stands, and **extends into the data layer**: A's `//hlsl` PSRC overrides
   and the `stage/override` payload are authored against A's convention. Whichever renderer loses,
   its override corpus is discarded or re-authored, not merged.
5. **NEW — the wire/archive cluster.** The draft filed this under "zero-risk adoptions"; that is
   wrong. Both sides rewrote count discipline differently (A: `uint32_t` members, with the 8-byte
   overloads deliberately *hidden* so a raw `size_t` fails to compile; B: `int` counts +
   ReadException guards + **centralized 8-byte overloads in `Archive.h:59-70`**). Taking B's
   `Archive.h` wholesale silently arms every *future* raw-`size_t` call site with an 8-byte wire
   write — precisely the footgun A's hidden-overload design breaks at compile time. **Land a designed
   union as one commit:** B's guards + A's member pinning + B's packed-map `int32_t` + an explicit
   ruling on overload visibility.
6. **NEW — `CuiMediator.cpp`**: A's re-entrancy guard vs B's FATAL-softening — same functions,
   opposite failure philosophies.

## 5. Risk register for the B-as-base world

**Controlling fact: nearly all of B's behavioural defaults ride one squashed commit,
`29c641b13` "Add self-contained x64 DX9 client builds"** — innocuously titled, and it will sail into
a base adoption unread. Ranked by how hard each is to reverse once it is the base default:

1. **The fail-silent data/network layer — hardest to reverse (ecosystem lock-in).** Not ~50 sites
   but a system: `NetworkHandler::dispatch` **swallows `Archive::ReadException`** rather than
   rethrowing — combined with B's own throwing guards, every malformed or misaligned message becomes
   silent loss and unbounded client/server state divergence; `DataTable` accessors return
   `0`/`0.0f`/`""` on missing columns; the `DataTableManager` empty sentinel; 63 removed `FATAL(`
   lines; `CustomizationData::saveToByteVector` silently truncates the **persisted appearance
   payload**; four more FATAL→continue sites (a missing `water_values.iff` disables water types
   process-wide). *Why most irreversible:* within months, server data packs exist that only boot
   under leniency, and restoring strictness then breaks other people's users — politically
   impossible. **Wave-1 mitigation: one `strictData` master config, strict as the upstream default.**
2. **Protocol/wire drift** — `entertainerCaptchaPercent` removed (deleting the very feature the
   shared fork-point commit merged); `ClientPermissionsMessage` unpack changed to a residual-size
   heuristic; one-way `InputMap` TAG_0007 keymap migration.
3. **Gameplay-ruleset defaults presented as fixes** — `canUse() → true` (pre-CU by declared intent),
   intended-target command fallback, chat bubbles silently disabled on a missing UI asset, 45/90 s
   force-finish loading, per-alter forced-wearable re-apply with non-terminating retry.
4. **MemoryManager bypass + budget rewrite + instrumentation off** — every x64 heap defect in the
   merged tree investigated blind, in a tree whose history shows that is its specialty bug class.
5. **⚠ THE SLEEPER: B's port plan deletes gl05/06/07 outright** (`docs/dx11-port-plan.md:507,513,645`).
   The draft framed the D3DX question as "B retains the legacy SDK"; the actual endgame is **no D3D9
   renderer at all**. That destroys the fork's parity oracle — RenderDoc D3D9-vs-D3D11 capture-diff
   is THE diagnostic — and the dual-renderer validation bar. **Extract an explicit "gl05 stays,
   ported off D3DX" ruling before his plan's P5 executes.**
6. **Data-set workarounds as engine truth** — DEFH heuristic, 0.3/0.85 lighting floors, byte-pattern
   shader patching, TAG_0006 "close enough" TRE parsing, and a **skinned-mesh vertex-colour rewrite
   (packed ARGB == 0 → 0xffffffff, opaque white, every skinned mesh)** which silently corrupts any
   render-parity measurement taken on a B build.
7. **A's compile-time discipline** (`/we4311 /we4312`, layout `static_assert`s, hidden-overload
   design) — trivially re-landable, lowest risk.

### Hard to cherry-pick from A (as opposed to merely large)

- **The WorldSnapshot line** — the hardest single file: A's phased `loadStep` + create/delete drains
  + `[ws.drain]` probes vs B's CRC-tolerance and force-finish rework, opposite philosophies. Note B
  *needs* A's delete drain before shipping the wicked branch's detail-slider range increase
  (`1060d2ccb`), which multiplies the leak.
- **The engine-advertise contract** — 1,565-line `engine_advertise.cpp` + in-TU private-method thunks
  inside engine classes (required by C2248), version-locked v33/160 to the SWG-Toolkit consumer, and
  some advertised names point at A-only systems. Dependency order: the perf-catalogue subset must
  land **first**, or the contract has to be re-cut downward.
- **The diagnosability layer** — `Game.cpp` + `sharedDebug`, both conflicted with legitimate B
  rewrites. Land it **early anyway**: every A performance claim is unmeasurable in a B base until
  the probes exist.
- **The decruft** — textually easy (deletions), politically hard.
- **The data/override payload** — renderer-locked, per conflict 4.
- **NOT hard: the engine crash-fix set** — ~14 surgical sites, 11 apply clean. It is the *systems*,
  not the fixes, that are entangled.

### Ordering — fastest-decaying first

1. **The strictness ruling** — policy, not code; decays fastest, before any B-based release creates
   data-pack dependents.
2. **Wire unification** as one designed commit (conflict 5 + packed-map `int32_t` + captcha
   restoration + `ClientPermissions` revisit).
3. **The decruft ruling** — before a B base ships x64-fixed versions of dead subsystems and "why
   delete what now works" wins the argument.
4. **MemoryManager root-cause** before allocator-shape dependencies accrete on CRT malloc.
5. **The gl05-survival ruling** before port-plan P5.
6. **Renderer decision by measurement.**

Deferrable: JUCE-vs-Miles (already a build flag), Qt3 tools, SDL3 / MIDI / cascades, editor contract.

### Concede without a fight

Build infrastructure wholesale (`Directory.Build.props/.targets`, matrix scripts, PE machine-type
verification, prerequisite manifest) · `RtlCaptureContext` all-bitness, keeping A's OOM reserve on
top · B's `Archive.h` guards and packed-map `int32_t` as wire-authoritative · the asm2hlsl corpus as
an artifact regardless of renderer outcome · headless `gl00` · SDL3 · entertainer MIDI · the
God-client Qt3 pipeline · `Gl_api` load-time layout verification · the transform throttle · the
COLOR-interpolant clamp and half-pixel-2D findings · `.clang-format` scoped to new files.

### The five hills — do not concede

**Fail-fast defaults · the memory pool · `false` in `cellLoaded` · gl05's existence · the strict-data
posture.**

## 6. The one decisive experiment

**Same machine, same TRE set, same server, both clients built by their own scripts** (A HEAD;
B `x64-dx11-vanilla`), plus a gl05 reference. RenderDoc capture-diff + frame timing across five
scenes: Mos Eisley starport **interior** (COLOR clamp, ambient floors, portal/cell binding — doubles
as the conflict-3 repro), Theed exterior (terrain blends), character select (exposes B's
vertex-colour rewrite), **JTL space + nebula + additive UI** (content B's renderer has plausibly
never drawn), and one timed zone-in end-to-end. Half a day, no code.

It closes the round's three biggest unknowns simultaneously: whether B's renderer *renders* A's
hardest content, whether A's renderer carries C35-class latent parity debt, and whose performance
story survives contact. It is the only thing that converts the renderer question from structural
reading into measured ground truth.

## 7. Residual uncertainty

CONFIRMED throughout except: transform-throttle severity at 144 fps (unmeasured); the COLOR-clamp
gap in A (suspected, not proven visible); "B has no space/JTL content" (a claim about his TRE set,
unverifiable from the repo — soften from categorical); and B's true motivation for the `cellLoaded`
flip (his stated rationale is code-contradicted, but his observed symptom was presumably real in his
buildout flow — insufficient evidence on which of his three interior changes actually cured it).
