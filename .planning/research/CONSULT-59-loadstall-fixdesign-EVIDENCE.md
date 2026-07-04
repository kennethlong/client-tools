# CONSULT-59 — load-stall fix design — SHARED EVIDENCE (treat as GIVEN, do not re-derive)

Client: SWG (SWG-Source fork), MSVC v145, Win32 Release primary + x64. D3D11 renderer (gl11)
plus three D3D9 renderers share the engine libraries. A frame-stall watchdog (whole-process
minidump sampler, 100ms threshold, 2 samples per stall) ran during 4 organic play sessions
2026-07-04. All stacks below are cdb-symbolized against matching PDBs. These are MEASURED
ground truth.

## Stall class 1 — world-entry mega-stall, 3.1–4.5 s, reproduced 4/4 sessions

Both time-samples (at ~100ms and ~500ms into the stall) of every occurrence land inside the
same synchronous chain on the main thread (sample at +500ms shown):

```
ntdll!NtCreateFile
KERNELBASE!CreateFileA
SwgClient_r!OsFile::open
SwgClient_r!FileStreamer::open
SwgClient_r!TreeFile::SearchPathA::open        <- probing a LOOSE search path
SwgClient_r!TreeFile::open
SwgClient_r!Iff::open
SwgClient_r!ShaderTemplateList::fetch
SwgClient_r!ClientProceduralTerrainAppearanceTemplate::PreloadManager::PreloadManager
SwgClient_r!ClientProceduralTerrainAppearanceTemplate::preloadAssets
SwgClient_r!ClientProceduralTerrainAppearanceTemplate::create
SwgClient_r!AppearanceTemplateListNamespace::create
SwgClient_r!AppearanceTemplateList::fetch
SwgClient_r!AppearanceTemplateList::createAppearance
SwgClient_r!GroundScene::load
SwgClient_r!GroundScene::init
SwgClient_r!GroundScene::GroundScene
SwgClient_r!GameNamespace::MultiPlayerSceneCreator::create
SwgClient_r!Game::_startScene
SwgClient_r!Game::_setScene
SwgClient_r!GameNetwork::startScene
SwgClient_r!GameNetwork::receiveCmdStartScene
```

The other sample was in the same PreloadManager frame but down in
`Texture::load -> gl11 createTextureData`. A third session's sample was in the same
GroundScene::load chain down in `ShaderEffectList::fetch -> ShaderImplementation::load ->
ShaderImplementationPassPixelShaderProgram -> Direct3d11_PixelShaderProgramData ctor`
(i.e. D3DCompile of a pixel shader) — that sample was momentarily inside a stale one-shot
diagnostic that appends PSRC source text to a dump file with fflush
(`Direct3d11_PixelShaderProgramData.cpp:1744-1781`, marked REMOVE; being deleted regardless —
NOT the main cost).

Source: `src/engine/client/library/clientTerrain/src/shared/appearance/ClientProceduralTerrainAppearanceTemplate.cpp`
- `PreloadManager::PreloadManager` (lines 53-108): synchronously `ShaderTemplateList::fetch`es
  EVERY shader-group family child, `AppearanceTemplateList::fetch`es + recursively
  `preloadAssets()`es EVERY flora-group appearance, and fetches every radial-group shader for
  the whole planet's TerrainGenerator.
- Triggered ungated from `ClientProceduralTerrainAppearanceTemplate::create` (line 156) via
  `preloadAssets` (lines 189-195, `m_preloadManager = new PreloadManager(this)`).
- `PreloadManager` dtor releases everything it fetched (ref-count pairing).
- This runs while the loading screen is up; the process is alive but the main loop does not
  tick for 3-4.5s (music/audio pump starves; watchdog fires).

## Stall class 2 — post-entry burst, ~10 stalls of 100–800 ms over the next ~10 s

```
SwgClient_r!memcpy / Texture::loadSurface / gl11 createTextureData   <- varies per sample
SwgClient_r!Texture::load
SwgClient_r!TextureList::fetch
SwgClient_r!StaticShaderTemplate::load_texture / load
SwgClient_r!ShaderTemplateList::fetch
SwgClient_r!ShaderPrimitiveSetTemplate::load_sps / load
SwgClient_r!MeshAppearanceTemplate::load_0005 / load
SwgClient_r!MeshAppearanceTemplate::asynchronousLoadCallback
SwgClient_r!AsynchronousLoader::processCallbacks
SwgClient_r!Game::runGameLoopOnce
```

Source: `src/engine/shared/library/sharedFile/src/shared/AsynchronousLoader.cpp`
- `processCallbacks()` (line 571): drains ms_completedRequests; per request it inserts the
  pre-read file bytes into the TreeFile cache (`TreeFile::addCachedFile`), runs the callback
  (full IFF parse + GPU resource creation on the MAIN thread), then `TreeFile::clearCachedFiles()`.
- A count budget ALREADY EXISTS: `ConfigSharedFile::getAsynchronousLoaderCallbacksPerFrame()`
  (line 589, consumed at line 686 `if (callbacksAllowed && --callbacksAllowed == 0) break;`).
  Config default is **0 = unlimited** (`ConfigSharedFile.cpp:43 KEY_INT(asynchronousLoaderCallbacksPerFrame, 0)`).
- Note a single callback can cost 100s of ms (texture-heavy mesh), so a pure COUNT budget has
  a worst-case floor of one-callback-per-frame cost.

## Stall class 3 — character-select avatar bring-in, 371 ms (one specimen, softer)

Main thread mid-draw of the charselect 3D avatar:
`CuiWidget3dObjectListViewer::Render -> ObjectWatcherListCamera::endScene ->
ShaderPrimitiveSorter -> SoftwareBlendSkeletalShaderPrimitive::draw ->
gl11 StateCache::applyPreDrawState -> getOrCreateBS (first-use state-object hash/create)`.
First-use resource/state creation during the avatar's first render.

## Stall class 4 — mid-play isolated stalls, 110–620 ms, ~1 per few minutes

Specimen (620 ms, on target change):
```
SwgClient_r!FileStreamer::File::read          <- synchronous disk read
SwgClient_r!LocalizedString::load_0001
SwgClient_r!LocalizedStringTable::load
SwgClient_r!LocalizationManager::fetchStringTable
SwgClient_r!LocalizationManager::getLocalizedStringValue
SwgClient_r!ClientObject::updateLocalizedName
SwgClient_r!ClientObject::getLocalizedName
SwgClient_r!SwgCuiStatusGround::updateTargetName
SwgClient_r!SwgCuiStatusGround::update / setTarget
SwgClient_r!SwgCuiAllTargets::addStatus / update / updateOnRender
SwgClient_r!UIManager::Render -> CuiManager::render -> Game::runGameLoopOnce
```
Localization library lives at `src/external/ours/library/localization/` (LocalizationManager,
LocalizedStringTable).

## Cross-cutting measured facts

- In EVERY stall dump the Miles (audio) threads are idle in WaitForSingleObject — NOT blocked
  on TreeFile or any engine lock. Audio glitches during stalls = main-thread starvation (the
  audio service pumps from the main loop), not lock contention.
- Settled-play frame census (18,432 frames, same morning): median 10.1 ms, p99 17.0 ms,
  0 frames > 80 ms. Steady-state is healthy; this is purely a load-path problem.
- A prior per-frame resource-creation census showed the stalled frames themselves have ~0
  D3D create calls; the cost is file-open/parse/IO on the main thread, with creates trailing
  as assets arrive. Off-thread D3D creation was already ruled OUT as the lever.
- TreeFile recently gained thread-safety fixes (SearchCache leaf mutex; ms_searchNodes
  snapshot-under-lock; Texture refcount serialized under TextureList CS). Any design must not
  regress these (regression signature: zone-in hang).

## Constraints (project rules)

- Client must stay bootable to character select (boot gate). Minimal diff preferred.
- Engine libraries are shared by 4 renderer plugins (gl05/06/07 D3D9 + gl11 D3D11) — changes
  in shared code must not assume D3D11. Shared-HEADER struct changes cascade an ABI rebuild
  of all plugins (avoid if possible).
- Existing precedent patterns in this codebase: `maxInteriorCreatesPerFrame` throttle
  (ClientInteriorLayoutManager), `minFrameRate` elapsed-time clamp, disk VS-bytecode cache.
- Total wall-clock load time is allowed to stay the same or grow slightly; the goal is to
  keep the main loop TICKING (audio alive, no multi-second freezes) and kill the 100ms+
  hitches after entry.
