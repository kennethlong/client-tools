# 2026-07-19 SESSION CLOSE — contract v19→v24 in one day, .ilf editing arc contract-complete, UTINNI SUNSET

**READ FIRST after restart.** One Sunday marathon: five contract versions (all consumer-request-driven,
all same-day request→staged-exe), the interior-decoration (.ilf) editing arc designed via CONSULT-69 and
made contract-complete, Utinni development SUNSET. `origin/master` = `796d335ff`, everything pushed, tree
clean except the long-parked CONSULT-5x/66 `.out` files + `stage/override/snapshot/` (Kenny's live-save
output, intentional).

## 1. ⚠️ STRUCTURAL CHANGE: Utinni SUNSET — SWG-Toolkit is the SOLE contract consumer

Kenny shut down Utinni development ("SWG-Toolkit is a much more modern better designed solution; no
double development"). Memory updated (`project_utinni_contract_sync_volatile`). Consequences:
- Handbacks/consults mirror ONLY to `D:/Code/SWG-Toolkit/.planning/handoff/` (their
  `*-CHANGE-REQUEST-*.md` incoming / our `*-PROVIDER-*.md` outgoing convention). Do NOT write new
  material into `D:/Code/Utinni` (phase-24 dir = historical archive; still valuable as gizmo/ImGui
  SOURCE reference for their Slice-0 vendoring).
- One consumer re-sync + one smoke per version bump.
- The toolkit's own consult discipline is GOOD (their CONSULT-70 caught our model-D data-loss; their
  layer probe caught the CONSULT-69 false positive) — treat their adversarial findings as first-class.

## 2. The contract day: v19 → v24 (146 names), all pushed, all handbacked

| Ver | Row(s) | State |
| --- | --- | --- |
| v20 | `clientWorld::collideScreenRay` (copy-out cursor ray: client-pixel → hit NetworkId + world point; terrain hit = 1 with id 0; ancestor walk is `m_childObject`-gated [Object.h:604] so cell-CONTAINED objects report id 0 WITHOUT dissolving into the building — v22-corrected comment) + `cuiRadialMenuManager::clear` | **CONSUMER-VERIFIED live** |
| v21 | `game::getSceneId` (copy-out, wsGetSavePath convention; one-click reload = `wsUnloadSnapshot` + `load(getSceneId())`) | landed; bind pending |
| v22 | `clientWorld::collideScreenRayObject` (borrowed RAW nearest-hit Object*; no walk/no id; shared ray core refactor) — the pure-.ilf pick | landed; bind pending |
| v23 | `worldSnapshot::wsSetNodeTemplateName` (in-place OTNL template re-point; subtree/id/transform/crc untouched; fail-closed: authored non-buildout + TreeFile-resolvable with `forgetMissingFile` first). New PUBLIC `WorldSnapshotReaderWriter::internObjectTemplateName` (extracted from addObject; append-only; no layout change → no ABI cascade) | landed; bind pending |
| v24 | `object::getTransformO2P` (copy-out o2p on borrowed Object*; byte-for-byte the camera O2W 3x4 layout) | landed; bind pending |

**AWAITING (the ONE open consumer item): v24 re-sync (sha256s in the v24 handback §3 — append-only,
covers v21-v24 in one bind) + the model-D END-TO-END persistence smoke:** pick decoration
(`collideScreenRayObject`) → their `resolveRowIndex` → `getTransformO2P` (orig + moved o2p) → edited
`.ilf` + tiny DERIVED template (`@base` + `interiorLayoutFileName` override — a derivable StringParam;
new names resolve loose via TreeFile, no CRC-table) → `wsSetNodeTemplateName` → `wsSaveSnapshot` →
reload. Expected: edited interior visible, authored subtree intact, other instances unchanged.

## 3. The .ilf design record (CONSULT-69 + their CONSULT-70)

- **Three-layer world model** (in `2026-07-19-utinni-hybrid-incell-ANSWERS.md`): snapshot (authored
  ids, client-spawned) / server-streamed / .ilf interior-layout (client-only, NO ids, per-TEMPLATE).
  Data-proved: the Mos Eisley cantina is NOT a tatooine.ws node (only 3 cantina POBs planet-wide, other
  towns); layer oracle = collideScreenRay id + wsGetNodeInfo membership.
- **CONSULT-69 synthesis** (`.planning/research/CONSULT-69-SYNTHESIS.md`, 5-consultant round):
  id-minting REJECTED (id-0 hover uplink is a FIREWALL — m_lookAtTarget auto-deltas to a live server;
  throttle-interleaved spawn order makes minted ids session-unstable; silent first-writer-wins
  collisions; note a client FAKE-id band `>=0x4000000000000000` ALREADY exists for waypoints with
  PARTIAL leak protection). Selection = pointer-keyed. **Experiment result CORRECTED: partial pass**
  (the first "gizmo moves chair" was a server-streamed networked tangible — false positive; pure .ilf
  never reaches cuiHud::getTarget, measured) → v22 row is the pick path.
- **Persistence = model (D), Kenny's design, contract-complete:** per-instance via derived template +
  edited .ilf (pure data, loose dir) + v23 in-place rebind; "change ALL instances" (model A — advertised
  surface over `InteriorLayoutReaderWriter`, writer API exists) NOT built — only if requested.
  Deferred: the Object*→(cellName,rowIndex) resolver (consumer attempting their side; provider shape =
  PURE READ, rank-in-parentCell-group of the building's file-ordered watcher list — no registry).
- SOE archaeology (Sonnet): .ilf was always a BAKE of live objects — `SwgGodClient/ActionsGame.cpp:277`
  `onSaveInteriorLayout` still in-tree.

## 4. Morning thread (perf) — unchanged since the 07-19 AM handoff, still parked

- **VB-lock probe STILL ARMED**: `[Direct3d9] logDynamicBufferLockMs=5` in `stage/client.cfg` — the
  815ms gl05 skeletal first-draw stall did NOT reproduce warm (zero VBLOCK lines ≥5ms, run was smooth);
  needs a COLD-boot repro session (first boot after machine restart, avatar-dense area). Remove the key
  after (cfg-parity hygiene). Kenny also armed `[ClientGame/WorldSnapshot] wsSelfTestSaveOnLoad=1`
  (his save-path testing) — strip when Goal-B-style gates are done.
- Exit "spinning cursor" = normal ~1.5s+ ExitChain teardown (sampled; ~250ms AsynchronousLoader
  wait + destructor cascade). Not a defect; low-value target if it ever bothers.
- The 16:00:41 c0000005 = hybrid-session weather-change callback the consumer didn't handle —
  **consumer-guarded (closed)**; sibling server-message set recorded in
  `2026-07-19-hybrid-server-callback-set-weather-NOTE.md` for eventual real handling. That dump's PDB
  was overwritten by the v20 rebuild (HEAD-of-that-morning rebuild needed if ever symbolized — moot).
- Rest of the perf backlog per `2026-07-19-gl05-shadercache-preload-and-vblock-probe-LANDED.md` §4
  (preloadSomeAssets overshoot, driver-threading soak call, probe strip, Debug-config refresh,
  ilm-extract audit).

## 5. Gotchas learned/relearned this session

- **Measurement beats prediction, again ×2**: Cursor's hud-pick trace predicted .ilf objects reach
  getTarget — consumer measurement refuted it; my "floor-hit" counter-theory was then killed by
  reading `Object::getParent()`'s `m_childObject` gate. Read the inline BODY, not the name.
- **The negative-cache trap generalizes**: any advertised row that validates a loose file the consumer
  JUST wrote must `TreeFile::forgetMissingFile` first (wsSaveSnapshot precedent, now v23 too).
- Contract-day mechanics all held: quote MSBuild `"/t:A;B;C"`, forced relink before link gates, grep
  0 `unresolved external symbol`, exe↔cfg pairing, comment-only edits don't need rebuilds.
- Crash-txt tail (`Terrain:`/`Player:`/`Cluster:` lines) identifies session shape fast (hybrid vs
  editor scene) before any symbolization.

## 6. Where to resume

1. **Consumer v24 bind + model-D end-to-end smoke** (their court; expect either DONE or a small
   follow-up request — e.g. the resolver row or model-A surface).
2. VB-lock cold-boot repro (Kenny's court; probe armed).
3. Whatever new CHANGE-REQUEST lands in `D:/Code/SWG-Toolkit/.planning/handoff/` — the pattern is
   stable: verify engine-side facts → implement fail-closed → gate (0 unresolved, ord-82 export,
   count static_assert, 45s smoke) → handback with sha256s → push → mirror.
