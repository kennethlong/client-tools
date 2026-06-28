# CONSULT-49 — Is JTL space-world non-render a code bug to fix, or a missing SWGSource config/component?

Treat ALL of the following as GIVEN (measured 2026-06-26/27, runtime + code + TRE inventory). Do not
re-derive; reason FROM it. Investigate ONLY your assigned angle.

## The operator's meta-question (the thing to actually answer)
SWGSource advertises shipping Jump-to-Lightspeed. In our client, space LOADS (HUD, targeting, ship
model, starfield all work) but the **3D world is empty — no planets, celestials, or ships, only a
starfield + UI markers.** The main agent traced this to a specific code path (below). **Before we
change engine code, the question is: is that a GENUINE bug we should fix, OR are we MISSING a
config/component/code-path that SWGSource ships (analogous to a prior finding where we were missing
the ILM_visuals.tre wiring) that would make the world render?** Give us confidence + the right action.

## LOCKED ground truth (measured — do not contradict)
- Client repo `D:/Code/swg-client-v2` ("Stella Bellum Testing" build), data `D:/Code/SWGSource Client v3.0`.
- **Real server space entry** (launched a Scyk Light Fighter from a Tatooine station; connected to the
  swg-main VM) — NOT an offline editor harness.
- The empty world is **identical under gl05 (D3D9) AND gl11 (D3D11)** — renderer-agnostic.
- Prior fix (CONSULT-48): wiring `ILM_visuals.tre` fixed the ship MODEL (was a default placeholder).
- **Planet ASSETS are all present + WIRED**: `appearance/planet_tatooine.pln` (+2 moons),
  `shader/cels_star_back.sht`, `shader/starglow_radial.sht`, `terrain/colorramp/stars_tato.tga`. The
  `.pln` PlanetAppearance type IS registered (`SetupClientTerrain -> PlanetAppearanceTemplate::install`).
- The operator notes: **this version has NO visible jump tunnel — just a static cut screen, then in space.**

## The main agent's CODE finding (VERIFY OR REFUTE this — it is the crux)
- `SpaceEnvironment` ctor (`clientTerrain/.../appearance/SpaceEnvironment.cpp`) creates stars +
  celestials + distant-appearance PLANETS. Each distant-appearance planet's parent Object is pushed to
  **`m_objectsToDisableForHyperspace`** (SpaceEnvironment.cpp:246). Stars are NOT in that list.
- The jump runs `HyperspaceIoWin::deactivateSpaceEnvironmentObjects()` (HyperspaceIoWin.cpp:320) ->
  `SpaceEnvironment::disableEnvironmentForHyperspace()` (SpaceEnvironment.cpp:377) -> `object->setActive(false)`
  on every planet (line 389).
- **The main agent grepped the whole hyperspace + space-environment code and found NO reactivation**:
  the only `setActive(true)` calls in HyperspaceIoWin are on the CAMERA (lines 566/671/783/853), never
  the environment objects; the `~HyperspaceIoWin` dtor restores the camera view but not the planets.
- Conclusion drawn: every jump deactivates the planets and never restores them -> invisible; starfield
  survives because it's a separate StarAppearance/skybox not in the disable list. Symptom matches.

## BANNED framings (already FALSIFIED — do not propose)
- "renderer-specific bug" (gl05==gl11). · "offline-harness artifact" (real server entry). · "missing
  planet asset/.pln type" (all present, wired, registered).

## Your job (per assigned angle)
Answer: (1) Is the "deactivate-with-no-reactivation" finding CORRECT, or is there a reactivation path
the main agent missed (scene/environment recreated on zone-arrival? a different method/owner? is the
disable even reached for a station-LAUNCH vs an in-space jump?)? (2) Is this code STOCK SWGSource
client source or a fork modification? (3) Does SWGSource JTL actually render the space world — and if
so, what restores the environment? (4) Bottom line: FIX CODE (add reactivation — where exactly) vs
ADD A MISSING CONFIG/COMPONENT (what)? Give a decisive disambiguating test.
