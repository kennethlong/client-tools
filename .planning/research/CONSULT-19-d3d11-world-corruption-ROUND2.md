# Consult ROUND 2: D3D11 world corruption — your Round-1 hypothesis was FALSIFIED

You (CODEX and Cursor independently) converged in Round 1 on a **BC/DXT bottom-of-chain
mip upload bug**: `lock()` pads the BC staging texture up to the 4×4 block boundary, the
engine writes only the logical block-rows, and `unlock()`'s 4-aligned `CopySubresourceRegion`
bakes the unwritten padded texels into the destination mip → garbage in sub-4 mips → distant/LOD
texture corruption. You proposed a decisive test (clamp sampling to mip 0) and a fix (zero the
padded staging / copy only logical rows).

**We implemented and tested it. It is WRONG.** This round we are telling you that, handing you
the new hard data, and asking you to challenge the *new* framing — not to defend the old one.

---

## What we did since Round 1 (all empirical, not hypotheses)

### TEST 6 — Zero the padded BC staging (your proposed fix). NO CHANGE.
After `context->Map()` of the BC staging texture, before the engine writes, we `ZeroMemory`
the entire mapped staging region (both the 2D and 3D/volume BC paths), so the padded block-rows
are deterministic zero instead of uninitialized driver memory. Built, ran, roamed Mos Eisley.
**Corruption identical.** → Uninitialized padded-staging CONTENT is NOT the cause.

### TEST 7 — Retain (leak) EVERY texture + SRV forever. NO CHANGE.
In `~Direct3d11_TextureData` we push every underlying `ID3D11Resource` + `ID3D11ShaderResourceView`
into static retain vectors so their **GPU memory is never freed or recycled**, even when the engine
deletes the wrapper. This kills any use-after-free / freed-and-recycled-memory mechanism dead.
Built, ran, roamed. **Corruption identical.** → Texture resource LIFETIME / UAF is NOT the cause.

### TEST 8 — We independently re-verified "RenderDoc replay-clean" with the RenderDoc API.
Round 1 took "captures replay clean" on faith. This round we opened the actual `.rdc` files
(6 captures taken during a corrupt live roam) through the RenderDoc replay API and **exported the
swapchain backbuffer (the presented image) at the Present event**:
- `frame4072.rdc`: backbuffer = **CLEAN** (correct Mos Eisley buildings/terrain/UI).
- `frame4632.rdc`: backbuffer = **CLEAN**.
- Debug/validation log for the captures: **empty** (no D3D11 debug-layer messages, no
  "initial contents not serialized" warnings, `degraded=false`).

So the corruption is **provably not in the recorded D3D11 command stream, pipeline state,
bindings, constant buffers, or serialized resource contents.** The captured frames are pristine.

### Architecture facts we confirmed this round (grep of the whole D3D11 plugin)
- **No deferred contexts.** Zero `CreateDeferredContext` / `FinishCommandList` / `ExecuteCommandList`.
  Everything goes through the single **immediate context**.
- `D3D11CreateDevice` with `createDeviceFlags = 0` → **default multithread-protected** immediate
  context (NOT `SINGLETHREADED`); the runtime takes its internal lock per context call.
- Texture *creation* asserts main-thread (`DEBUG_FATAL(!Os::isMainThread())`). (We have NOT yet
  audited whether VB/IB locks/uploads or draw submission can occur off the main thread.)

---

## The full elimination set now (trust these, not theory)

| # | Test | Result |
|---|---|---|
| 1 | PS cbuffer b0 — all 25 regs instrumented, 282,404 draws | sane + stable; NOT it |
| 2 | SRV slot bindings (terrain binds exactly 4) | consistent; NOT it |
| 3 | Per-frame `Flush()` + `QUERY_EVENT` spin-wait after Present (GPU finish) | NO CHANGE |
| 4 | Disable async file loader (`runtimeDisableAsynchronousLoader`) | NO CHANGE |
| 5 | RenderDoc capture of corrupt live frame | replays/exports CLEAN |
| 6 | **Zero padded BC staging (your fix)** | **NO CHANGE** |
| 7 | **Leak all textures+SRVs forever (no free/recycle)** | **NO CHANGE** |
| 8 | **Re-verified backbuffer clean via RenderDoc API on 2 of 6 .rdc** | **CLEAN** |

The corruption itself (live screenshots): entire surfaces show the WRONG texture (recognizable
lava/circuit on stone buildings); terrain renders solid blue→magenta→green→yellow, changing
frame-to-frame; white blocks; floating geometry shards. **Near geometry FINE; corruption is
distance/LOD-correlated.** Flickers corrupt↔normal; some surfaces LATCH bad and stay; worse on
occluded geometry. D3D9 (`gl05`) renders the identical scene/assets/server perfectly.

---

## The reframe we now believe (challenge it)

Walk what RenderDoc faithfully reproduces vs. what we see:
- Wrong **content** in the right resource → would replay corrupt (contents serialized). It's clean.
- Wrong **binding** (SRV/VB/slot/cbuffer) → would replay corrupt (calls recorded verbatim). It's clean.
- **UAF / freed-recycled memory** → killed by the leak-everything test. No change.
- **GPU pipeline race** → killed by GPU-finish. No change.

What survives is a **live-only, timing-sensitive phenomenon** — RenderDoc's capture instrumentation
(extra locking + slower submission) perturbs timing enough to mask it; the recorded stream is a
"good" ordering. Classic Heisenbug signature.

But we are explicitly suspicious of *this* conclusion too, because it is the comfortable story and
we have a single immediate context, multithread-protected, with no deferred contexts — which makes a
naive "two threads racing on the context" mechanism hard to construct (the runtime serializes
individual calls).

---

## What we're asking you (be adversarial — assume we're still wrong)

1. **Given single immediate context + default multithread-protection + NO deferred contexts**, what
   concrete D3D11-specific mechanism produces *live-only, distance/LOD-correlated, flickering+latching
   wrong-texture/geometry* that **provably does not appear in a RenderDoc capture of the same frame**?
   Be specific about the code path, not the category.

2. Is "RenderDoc replay-clean" actually consistent with an **intermittent but capturable** bug where
   we simply keep catching clean frames (the corruption flickers)? i.e., is the right next step to
   **catch a LATCHED-corrupt surface** (one that's persistently wrong, not flickering) in a capture and
   check whether *that* replays clean — which would decisively separate "un-capturable timing bug" from
   "intermittent but in-stream"? What other single capture-side test discriminates these two?

3. If it IS timing: what is the **most likely specific racing producer** in an SWG-engine D3D11 port?
   Candidates we can think of: engine worker/job threads issuing `Map`/`UpdateSubresource` on dynamic
   VB/IB or textures concurrently with the render thread's draws; a background terrain/LOD builder
   touching the device; `UpdateSubresource` on a `USAGE_DEFAULT` resource being read by an in-flight
   draw from a prior call. Which would the multithread-protection lock NOT save us from, and why does
   D3D9 tolerate the same engine threading where D3D11 does not?

4. What is the **single decisive next test** — a code experiment (e.g. force every device/context
   call through one global lock; or force the engine fully single-threaded) OR a capture-side test —
   that confirms-or-kills the timing hypothesis the way TEST 6/7 killed the content/lifetime ones?

5. Are we wrong that it's timing? Is there a non-timing mechanism consistent with ALL of tests 1–8 —
   e.g. something in the present/compositing path, a baked persistent render target, depth/Z handling,
   or a resource RenderDoc re-initializes differently than the live driver — that we've under-weighted?

Tell us where this framing is wrong. We have falsified our last two confident conclusions by testing
them; we expect to falsify this one too, and want the test that does it fastest.
