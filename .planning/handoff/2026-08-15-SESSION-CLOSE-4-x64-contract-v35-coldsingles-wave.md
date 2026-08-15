# 2026-08-15 — SESSION CLOSE 4: x64 contract shipped (v33→v35), cold-singles perf wave, JUCE approved

**READ FIRST (latest session-close; same day as CLOSE-3 — evening).** Three arcs advanced in one
session: the toolkit x64 contract went from "planned" to **SHIPPED at v35/165 through two
consumer change-request rounds**, the Sais branch gained c22 + the ONE-GIANT-PR mandate, and the
perf arc landed its **cold-singles wave** with a live A/B armed. Direction decision: **JUCE audio
adoption APPROVED** (after perf; needs fixes first). Trackers:
[SAIS-PR-QUEUE.md](../SAIS-PR-QUEUE.md) · [SAIS-KNOBS.md](../SAIS-KNOBS.md) · perf/JUCE state in
auto-memory topic files.

## State

- **Our repo `master`: ELEVEN commits LOCAL, UNPUSHED** (on top of pushed `185755a6f`):
  `372e7aa42` x64 advertise port · `e292a3478` tools/hookpoints-probe · `971805d5c` v34 wave ·
  `56388031b` docs · `d2819bfa3` v35 no-detour overlay · `af06afb86` docs · `27bc59c3a`
  searchPath file manifest + A/B counters · `1a0ed9496` Texture one-read · (+3 doc commits
  earlier in the day incl. this file's set). **Push when Kenny says.** Tree clean.
- **Staged x64 = a MATCHED SET** (v35 changed Gl_api — exe 15:27 + gl DLLs 14:00-14:17 are
  ABI-consistent; nothing since touched plugin code; the Gl_api size gate verifies at boot).
  ⚠ Never mix these with older staged DLLs.
- **Sais branch `strict-data-defaults` @ `5f68728be`, 22 commits, pushed.** stage-B-x64
  hand-restaged (exe `D37ADE49…`, gl11 `C2539CF3…` — the c22 build).
- **⚠ TEMP A/B KEY IS SET**: `stage-x64/client.cfg` `[SharedFile] searchPathFileManifest=false`
  (commented TEMP, session 1 of 2). Kenny was ABOUT TO LAUNCH the baseline session when context
  reset was called. **REMOVE the key after session 1, before session 2.** Win32 stage/ cfg
  deliberately untouched (temp key, x64-only, avoid parity drift).

## Arc 1 — Sais collaboration

- **ONE-GIANT-PR mandate (Kenny)**: PR #1 stays CLOSED while the branch grows; NO milestone
  reopens; Kenny explicitly calls the reopen; keep the closed PR's body current (it now covers
  all 22 commits + the SAIS-KNOBS restore table + flag-to-Sais items).
- **c22 `5f68728be`**: WorldSnapshot wrong-class narrowing ported to his tree
  (asSharedObjectTemplate at fetch + asClientObject in instantiateObject; the b47718cbc 6.1
  hunks; engine_ws* shim hunks NOT ported — no advertise surface in his tree — they ride with
  PR #2). Built clean, restaged, pushed, PR body updated.
- **PR #2 = the toolkit advertise surface into his x64-first tree** — now MOSTLY MECHANICAL
  since the surface went dual-arch (below). Scope notes in SAIS-PR-QUEUE.md "PR #2" section.

## Arc 2 — the x64 hookpoint contract (v33 → v35, all shipped + probe-verified)

- **Port `372e7aa42`** (21 files): every `#if !defined(_WIN64)` guard removed (advertise TU,
  GroundScene/Os/ChatWindow/EffectMgr/Creature/PCC/CILM/WorldSnapshot×3, ClientMain gate,
  vcxproj condition). THE one real ABI item: Win32 `__fastcall(pThis, dummy-EDX, …)` __thiscall
  emulation must carry NO dummy on x64 (single convention, this in RCX) — **`ENGINE_THIS` macro
  seam** (engine_advertise.cpp + engine_groundScene_forward.h + mirrored block in
  GroundScene.cpp); GroundScene.h/Os.h **guarded friend decls were the first-build breaker** —
  now explicit per-platform friends (ABI-neutral). pmfToVoid/pmfRealEntry port as-is (x64 MI-PMF
  = pfn@0 + adjustor@8). EngineWsNodeInfo 80-byte layout bit-identical (static_asserts kept).
  Export = dllexport-by-name; **x64 ordinal coincidentally 82 — never rely on ordinals**; Win32
  ord-82 @ 0x00701420 unchanged (zero drift).
- **`tools/hookpoints-probe/Probe-HookPoints.ps1` `e292a3478`** — boot-free contract gate:
  loads a staged exe as an image (DONT_RESOLVE) + calls GetEngineHookPoints on the pre-CRT path;
  checks version vs .h, count+name-set vs .inc (comment-stripped parse!), nulls, dups; arch
  auto-redispatch; negative-tested. **Run after ANY advertise/catalog change.**
- **v34 `971805d5c`** answers consumer CHANGE-REQUEST round 1 (all 6 asks; handback
  `2026-08-15-PROVIDER-HANDBACK-x64-hookpoint-contract.md`, mirrored both repos):
  +`client::advertisedArchBits` (their fail-closed arch assert — no legacy x64 client exists) +
  `object::setScale` (their D-09 nullptr seed). 162 names.
- **v35 `d2819bfa3`** answers round 2 (their find: **DetourXS is x86-only BY CONSTRUCTION** —
  zero REX handling in ADE32, 32-bit API, 5-byte relative JMPs): the NO-DETOUR OVERLAY —
  `graphics::registerFrameCallback` (render thread, AFTER the BCG/gamma pass, BEFORE Present —
  byte-for-byte where their DXGI vtable patch drew; before drainInfoQueue so their validation
  logs same-frame) · `graphics::registerResizeCallback` (**TWO-PHASE by necessity**: phase 0
  pre-ResizeBuffers = consumer MUST release back-buffer views or ResizeBuffers fails; phase 1
  post-rebuild; both carry new size; one snapshot serves both phases) ·
  `game::registerTickCallback` (TOP of runGameLoopOnce, outside any render chain — their
  deferred scene-swap drain; game::mainLoop stays advertised). Plumbing: **2 new Gl_api TAIL
  slots** (the full plugin-rebuild cascade — hence the matched-set warning); D3D9 plugins
  accept-and-ignore with a Release line; single-slot, null clears, invocation sites snapshot
  the pointer. Round-2 handback answers asks 4–5 too (EngineDx11HookPoints by-value 3-field
  struct stays, separate gl11 export; the six DETOURED rows are NOT theirs — their advertised
  detour surface is now ZERO, MinHook = legacy-SWGEmu path only). 165 names.
- Probe green at every step; final: **v35 / 165 / 0 nulls / 0 dups, BOTH exes.**
- ⚠ NOT yet exercised: no live x64 injection, no callback registration ever invoked (acceptance
  ladder in the round-2 handback: archBits assert → tick log-only → clear-color frame draw →
  resize phases → full ImGui). Consumer sequenced their x64 work as their NEXT milestone.
- v35 boot smoke pending — **Kenny's next launch is it** (rides the A/B session below).

## Arc 3 — perf: the cold-singles wave (approved; 873ms NV wait DISPOSED)

- 873ms NV wait re-examined: ONE sample ever, pre-world, inside nvd3dum on gl05, behind
  loading — the bucket Kenny already deprioritized. **No dedicated work; sampler stays armed.**
- Fresh convictions from the MORNING's live watchdog log (`stage-x64/stall-watchdog.log`):
  loop 5569 = 643ms space-preload `TreeFile::exists` GetFileAttributes storm (FIRST-TOUCH
  probes — the 07-06 negative cache covers only re-probes); loop 22162 = 330ms in-world
  first-use texture cluster (Gate::wait file reads + loose CreateFileA probes inside
  Texture::load); ⚠ loop 31844's scary 2548ms = **window-chrome SendMessage/KiUserCallback**,
  NOT game content — do not chase.
- **Fix 1 `27bc59c3a` — loose-searchPath FILE MANIFEST**: per-SearchPath lazy directory
  enumeration (normalized to fixUpFileName convention); absent-name probes = zero syscalls,
  first-touch included; negative cache stays as fallback + stale-positive self-heal;
  `forgetMissing()` INSERTS into a built manifest (Wave-3 wsSaveSnapshot flow preserved;
  broadcast false-positives are safe-by-design). Kill switch `[SharedFile]
  searchPathFileManifest` (default ON). **Release-visible `[treefile.probe]` line per loose
  node at clean shutdown**: realProbes / manifestFiles / manifestSkips / negCacheSkips.
- **Fix 2 `1a0ed9496` — Texture::load one-read buffering**: .dds paid a Gate::wait I/O-thread
  round-trip PER MIP (per ROW on pitch mismatch); MemoryFile collapse, the
  TreeFileFactory/CONSULT-68 idiom, with buffering-failure fallback. No kill switch
  (correctness-shaped) — active in BOTH A/B sessions; its delta shows vs the morning's samples.
- **THE A/B (Kenny's probe-before-fix ask, armed and waiting)**: same binary, one variable.
  Session 1 = the key currently SET in stage-x64/client.cfg (baseline, counters live) — play a
  normal route, EXIT CLEANLY (probe lines print at shutdown). Then remove the key, session 2
  same kind of route. Compare `[treefile.probe]` realProbes per path, `TEXCREATE` ms
  (`[ClientGraphics] logTextureCreates`), and stall-watchdog profiles.
- **RECON BANKED (Explore agent full map in this file's git commit context + perf topic
  memory)**: the cold-single ROOT mechanism = `AsynchronousLoader::add()` MANIFEST-MISS
  SYNCHRONOUS FALLBACK (AsynchronousLoader.cpp:363-386 — non-manifest roots run the whole load
  inline on the caller's thread; only 2 add() sites). NEXT STEP IF A/B SHOWS RESIDUAL:
  synthetic single-file record on miss → worker pre-reads root bytes → transient-cache
  delivery. Also banked: SearchCache = persistent never-evicted RAM cache at top priority whose
  hits bypass Gate::wait (fill = TreeFile::cacheFile, itself main-thread-blocking); texture
  names materialize with ~zero lead time (TXM → TextureList::fetch same expression — dependency
  prefetch needs a raw-IFF TXM extractor honoring placeholder/ENVM + usesTexture gate);
  TextureList::ms_criticalSection held across the whole blocking Texture ctor.

## Direction decision — JUCE audio (memory: project_juce_audio_adoption_arc)

Kenny APPROVED adopting Sais's JUCE backend (his build default; retires vendored Miles +
unblocks the upstream-offering licensing check) — **sequenced AFTER the perf wave, and it needs
fixes first: audio dropouts + crackling.** Suspects (Kenny's, code-corroborated): (1) full WAV
files loaded into memory, no streaming — our Miles side has NO reusable feed (AIL_open_stream
does its own I/O) but the **stream-vs-cache classifier** (Audio.cpp ~720:
`sampleSize > getMaxCached2dSampleSize()` → SampleStream) is the reusable layer his port likely
flattened; JUCE fix = AudioFormatReaderSource + BufferingAudioSource over a juce::InputStream
adapter on TreeFile (which beats Miles: streams from TREs); (2) no mixer voice cap → deadline
misses under load; fix = hard cap + priority voice stealing; (3) audit the audio callback for
alloc/locks. Transferable hardening: streamBufferBytes sizing, the CONSULT-62 open-holds-mixer-
mutex lesson, EOS-by-polling semantics.

## Open board (next session, in order)

1. **Kenny's A/B session 1** (key is SET) → read `[treefile.probe]` + v35 boot smoke + fresh
   watchdog → **remove the temp key** → session 2 → compare. Then decide: async-loader
   manifest-miss fallback (design banked) or done.
2. **Push** the 11 local commits when Kenny says.
3. Consumer's x64 milestone runs next on their side — expect change-requests/handbacks in
   `D:/Code/SWG-Toolkit/.planning/handoff/` (rounds 1+2 answered; nothing owed).
4. PR #2 assembly (advertise surface → Sais tree; c22-skipped engine_ws hunks ride along) —
   after the consumer milestone proves the surface, or when Kenny calls it.
5. JUCE adoption arc (scope above) after perf settles.
6. Sais PR #1: grows quietly; Kenny calls the one-giant-PR reopen.
