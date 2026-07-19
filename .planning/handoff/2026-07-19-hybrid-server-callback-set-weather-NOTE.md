# 2026-07-19 — NOTE: hybrid-session server-callback set (weather) — consumer-guarded, provider facts recorded

**Status:** No provider action needed now. Consumer (Utinni/SWG-Toolkit) put guards in place;
this note records the incident + the provider-side facts for when "that set of callbacks" gets
real handling (likely a future freeze request).

## Incident

2026-07-19 ~16:00:41 local (`SwgClient_r.exe-unknown.0-20260719210041.{mdmp,txt}`): c0000005
during Kenny's scene-save-path testing on the PRE-v20 exe (Friday 07-18 16:24 build). Session
shape: **hybrid** — server login (`Cluster: swg`) + editor scene via `game::loadScene`
(`Terrain: terrain/naboo.trn`, player at 0.00/20.25/0.00 = origin spawn), tatooine→naboo scene
switch in the session (crash-txt shows tatooine object Alter-Killing), uptime 579s,
`wsSelfTestSaveOnLoad=1` armed. Root cause per Kenny: a **weather-change callback the consumer
did not handle**. Consumer guarded; crash closed their side.

⚠️ The dump's matching PDB was overwritten by the same-day v20 rebuild (16:14) — deep
symbolization of THIS dump needs a HEAD~1 (pre-`7c8cf81c7`) rebuild if ever wanted. Probably moot.

## Provider-side facts (verified in source 2026-07-19)

The engine's own weather path is null-safe in the hybrid case — the crash class is
consumer-side message modeling, not an engine defect:

- `GroundScene::receiveMessage`, `ServerWeatherMessage` branch [GroundScene.cpp:2874-2887]:
  guarded on `TerrainObject::getInstance()`; pushes to `WeatherManager::
  setNormalizedWindVelocity_w` + `GroundEnvironment::getInstance().setWeatherIndex`.
- `GroundEnvironment::getInstance()` [GroundEnvironment.cpp:1940] **lazily creates** the
  singleton (no null deref); `setWeatherIndex` is a plain member write.
- `WeatherManager` [clientTerrain/weather/WeatherManager.cpp]: static fn-pointer list; sole
  registrant is `ParticleEffectAppearance::setGlobalWind` (GroundScene ctor :697, dtor :1133 —
  symmetric, static fn, no dangling-object exposure). `weatherChanged()` caches `end()` but no
  registered callback mutates the list.
- Sibling branches in the same receiveMessage set that a hybrid session KEEPS receiving from
  the live server while an editor GroundScene is current: `ServerTimeMessage` (:2855, also
  terrain-guarded), `NewbieTutorialRequest` (:2891), and the rest of the server-driven set —
  the natural checklist for the consumer's eventual "handle that set of callbacks" work.

## If/when real handling is designed (consumer freeze request expected)

Provider options on the table (NOT committed): advertise a copy-out weather read
(`WeatherManager::getScaledWindVelocity_w` is a static; `GroundEnvironment` weather index has
a getter), or a provider-side suppression/routing toggle for server-driven scene messages
while an editor scene is active. Decide against the actual consumer design, not speculatively.
