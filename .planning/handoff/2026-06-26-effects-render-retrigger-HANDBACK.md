# Provider HANDBACK — Bucket B (Effects editor live preview): render rows + particle retrigger

**Status:** DONE, build-gated, **not yet maintainer-smoked**. Provider side complete + pushed.
**Wave:** Utinni Phase 24 §2.B (the [outstanding-editor-unlock ledger](2026-06-26-utinni-provider-outstanding-editor-unlock.md), Bucket B — top priority).
**Contract:** `ENGINE_HOOKPOINTS_VERSION` **6 → 7**, **104 names** (was 99).
**Commit:** `db3ca5895` (origin/master). 32-bit-only advertise TU unchanged; SWGEmu D3D9 path untouched.

---

## Re-sync (Utinni side) — copy byte-identical + sha256-verify

Copy the two shared contract files **byte-identical** into `D:/Code/Utinni/UtinniCore/swg/` and verify.
sha256 of the canonical **LF** working-copy bytes (this is the form to copy — git may CRLF-normalize on
checkout, so copy the working tree, not a fresh `git show`):

```
engine_hookpoints.h    5f0e48acc711554eff7a55180ba083d35c2e8317d8076edd8edf0ee149a47ae9
engine_hookpoints.inc  79c733976faadd2e565f6b029bed7634e93a3879b435e3f313afc035f193b558
```

Then bind the 5 new names in the consumer and remove the `if (!advertised)` gate on the rows that now
resolve (see per-row notes). `utinni_verifyNoNullNoDup()` covers the grown 104-name required set.

---

## NEW ROWS (5)

| Name | Engine symbol | Mechanism | Consumer use |
|------|---------------|-----------|--------------|
| `particlePreview::retrigger` | `utinni_retriggerClientEffect(const char*)` (friend free fn, `ClientEffectManager.cpp`) | constant `&fn`, `__cdecl(const char*)` | **the live-preview value** — drop in behind `utinni::ParticlePreview::retriggerLiveEffectInstances`; `isRetriggerAvailable()` returns true once the name resolves |
| `renderWorld::addObjectNotifications` | `RenderWorld::addObjectNotifications(Object&)` | static `&fn` | appearance-in-world render path |
| `bloom::preSceneRender` | `Bloom::preSceneRender()` | static `&fn` | post-processing preview |
| `bloom::postSceneRender` | `Bloom::postSceneRender()` | static `&fn` | post-processing preview |
| `skeletalAppearance::getDisplayLodSkeleton` | `SkeletalAppearance2::getDisplayLodSkeleton() const` | `bit_cast` PMF (const overload; dyn[] row) | skeletal LOD read |

### `particlePreview::retrigger` — the cooperative retrigger (§2.B-ii)

Provider deliverable: a **friend free function** `utinni_retriggerClientEffect(char const* logicalName)`
(friend of `ClientEffectManager`, ABI-neutral) that:
1. `AppearanceTemplateList::fetch(logicalName, found)` (the SAFE overload — `found=false` instead of
   FATAL if the name isn't a known appearance template) to refresh the (timed-)template cache; balanced
   `release` after the walk (live instances keep their own refs — never dangled).
2. Walks the **private** `ClientEffectManager::m_particleSystems` (no public enumerate surface existed),
   and for each live particle `Object` whose `Appearance::getAppearanceTemplateName()` matches
   `logicalName` (case-insensitive), calls `ParticleEffectAppearance::restart()` (re-fires from `m_age=0`,
   rewinds all emitter groups).

**Threading/lifetime contract (honored):** game-thread-only; once per save/reload (never per frame);
**allocation-free** on any per-frame path — read-only walk of the existing vector, no growth, no per-call
heap (the `project_rh_snapshot_no_heap_alloc` lesson). Consumer seam unchanged: `utinni::ParticlePreview`
(`particle_preview.h`) drops in behind this exact `__cdecl(const char*)` signature.

**Scope note (flag):** matches by **particle appearance (.prt) name**. `.cef`-level re-play is out of
scope — `ManagedParticleSystem` stores no `.cef`→instance link; re-play a client effect via the public
`ClientEffectManager::playClientEffect` path if the editor needs `.cef` retrigger. Also: whether a
restart visibly reflects the *edit* depends on the template being refreshed in memory (timed-template
in-place reload, or the editor mutating the cached template) — restart re-fires the instance with its
current template; the fetch warms/refreshes the cache.

---

## OMIT / SKIP ledger (handoff §2.B-i requested, NOT advertisable here)

A wrong `&` is worse than a missing row; a skipped virtual is consumer-vtable-resolved (handoff §2.C),
not a missing required row. Each accounted for, alternatives offered:

| Requested | Disposition | Why / alternative |
|-----------|-------------|-------------------|
| `particleEffectAppearance::render` | **SKIP (virtual)** | `ParticleEffectAppearance.h:77 virtual void render() const` → consumer vtable-resolves; `&fn` is a vtable stub. |
| `skeletalAppearance::render` | **SKIP (virtual)** | `SkeletalAppearance2.h:120 virtual void render() const` → vtable-resolve. |
| `particleEffectAppearance::ctor` | **OMIT (ctor)** | Cannot take `&Class::Class`; **no single construction funnel** (only inline `new ParticleEffectAppearance(tmpl)`) — a placement-new thunk is detour-dead. Same disposition as `cuiChatWindow::ctor`. The retrigger supplies the live-preview value instead. If you specifically need construction tracking, RVA-resolve on SWGEmu / degrade on advertised, or tell us a concrete funnel to advertise. |
| `skeletalAppearance::addShaderPrimitives` | **OMIT (absent)** | Not a member of `SkeletalAppearance2`. The name lives on `Skeleton::addShaderPrimitives(const SkeletalAppearance2&)` / `CompositeMesh` — different class + signature. Name the one you want and we'll advertise it. |
| `renderWorld::render` | **OMIT (absent)** | No `render` on the all-static `RenderWorld`. Nearest = `RenderWorld::drawScene(const RenderWorldCamera&)` [`RenderWorld.h:97`], static. Say the word and we bind `renderWorld::drawScene`. |
| `shaderPrimitiveSorter::*` | **OMIT (wildcard)** | No concrete method named. Canonical submission funnel = `ShaderPrimitiveSorter::add(const ShaderPrimitive&)` [`ShaderPrimitiveSorter.h:82`] (static, overloaded). Name the concrete contract slot (e.g. `shaderPrimitiveSorter::add`) and we'll bind it with the disambiguating cast. |
| render globals (static-shader `0x1922F8C`, transform/scale `0x1945AD4`/`0x194596C`, extent `0x1945A0C`) | **COORDINATE** | Took the handoff's **preferred** shape: drive the draw via the already-advertised `graphics::*` statics; **no raw private-global rows** (§8 #3: never take a private-member address). If you truly need raw `(void*)&g`, send the concrete accessor/name and we'll add accessor-style rows. |

---

## Maintainer smoke steps (the ground-truth gate)

1. Re-inject Utinni against the staged `SwgClient_r.exe` → `utinni_init` resolves the grown 104-name set
   (no new nulls/dups; the 99→104 delta binds).
2. Load a world in-world (TJT / `game::loadScene`).
3. Open the **Effects** editor; confirm basic edit/save still works (was already green).
4. Edit + save a `.prt` (or a client effect that spawns one) that has **live instances in the scene**.
5. **Expected:** the editor's "Preview in client" button + reload-candor badge **light up**
   (`isRetriggerAvailable()` true), and on trigger the **live scene instances restart** (re-fire from the
   start; the edit is visible if the template refreshed in memory).
6. Confirm no crash / no per-frame hitch (the retrigger is one-shot, game-thread, alloc-free).
7. Regression: SWGEmu D3D9 Pre-CU live-smoke still passes (advertise path is additive + gated).

If anything in the OMIT ledger turns out to be needed, name the concrete symbol/slot and it's a quick
follow-up wave (each is a one-row add or a documented alternative already located).

---

## Next buckets (per handoff §4, one wave per smoke)

A (per-editor real-entry rows: chat ctor, free-cam, world-pick/HUD, sys-msg/input) → C (virtual vtable,
consumer-side) → D (mid-function joint toggles) → E (WS-5 scene-ready callback) → F (crash-log setter).
