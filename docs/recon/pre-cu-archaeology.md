# Pre-CU Archaeology — SWG Client Source

*Surveyed: 2026-05-07*
*Scope: `src/engine/`, `src/game/`, spot-checks in `src/game/server/database/`*

---

## Summary

The NGE-era codebase (~2010 final build) is largely "pure NGE" — Pre-CU code paths are not guarded by runtime flags or `#ifdef` switches but were either fully removed or disabled in-place with `#if 0`. The transition artifacts that remain fall into four categories: (1) a single dated comment explicitly marking a Combat Upgrade deprecation; (2) ~40+ `#if 0` blocks across both game code and tools, with varying explanation quality; (3) three orphaned "Old" header stubs for a superseded resource-extraction UI; and (4) a substantial body of schematic-revocation infrastructure that survived the NGE because it remained needed for GM console commands. The largest single artifact is a 2,514-line SQL schematic conversion table in the database layer, which is historical record, not active code.

---

## 1. Era-Transition Comments

Only one comment in the C++ source explicitly names the Combat Upgrade and carries a date:

| File | Line | Comment |
|------|------|---------|
| `src/engine/shared/library/sharedGame/src/shared/command/CommandTable.cpp` | 151 | `// <-- defaultTime column is deprecated by combat upgrade - 3/16/05 jmatzen` |

Context (lines 150–151):

```cpp
cmd.m_defaultTime = 0.f;
//t.getFloatValue("defaultTime", row); // <-- defaultTime column is deprecated by combat upgrade - 3/16/05 jmatzen
```

The column read is permanently commented out; the struct field is hard-wired to `0.f`. The date (March 16, 2005) places this squarely in the CU development window.

Two comments in the client UI reference the skill-system revocation mechanic that was changed during the CU:

| File | Lines | Comment |
|------|-------|---------|
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiRoadmap.cpp` | 692–693 | `// @note: revoked schematics were removed from the skill system. // there used to be an m_iconPathDraftRevoked and a '-' prefix here to support that.` |

One server-side warning message acknowledges that a code path bypasses the post-CU skill system:

| File | Line | Comment |
|------|-------|---------|
| `src/engine/server/library/serverGame/src/shared/object/PlayerObject.cpp` | 994 | `"... revoking schematic ... due to it being revoked outside the skill system"` |

**Finding:** Era-transition comments are sparse — the code was cleaned aggressively. The CommandTable comment is the only one with an explicit CU/NGE attribution and author/date.

---

## 2. `#if 0` Blocks

Total `#if 0` occurrences across `src/engine/` and `src/game/`:
- `src/engine/`: ~150 files, ~205 occurrences (many in MayaExporter, TerrainEditor tools, animation system, and math utilities — most are unrelated to era transitions)
- `src/game/`: ~20 files, ~29 occurrences

The blocks most likely to be era-related (based on the code they disable):

### High-value blocks (game-logic disabled code)

| File | Lines | What the disabled block does | Inline note |
|------|-------|------------------------------|-------------|
| `src/engine/server/library/serverGame/src/shared/trading/ServerSecureTrade.cpp` | 737–791 | Full `removeItem()` body — checks trade state, removes item from initiator or recipient list, sends `RemoveItemMessage` to other party, calls `unacceptOffer()` | None — function body replaced by a simple `cancelTrade()` call with comment "CS requested that removing an item cancels the trade to prevent scamming" |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserNet.cpp` | 339–351 | Debug `/characters` listing — iterates `ConnectionManager::CharacterMap_t` and prints character name/location/appearance | `// reworking ConnectionManager` |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserNet.cpp` | 358–364 | Debug `/clusters` listing | `// reworking ConnectionManager` |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserNet.cpp` | 370–376 | Debug `/gameservers` listing | `// reworking ConnectionManager` |
| `src/game/server/application/SwgDatabaseServer/src/shared/buffers/IndexedTableBuffer.h` | entire file | Entire header wrapped: `#if 0 //@todo code reorg` | `@todo code reorg` |
| `src/engine/server/library/serverGame/src/shared/core/ServerWorld.cpp` | ~2541–2551 | Disabled pending-object cleanup | None |
| `src/engine/server/library/serverGame/src/shared/core/ServerBuildoutManager.cpp` | two blocks | Unknown — no inline comments | None |
| `src/engine/server/library/serverGame/src/shared/core/PreloadManager.cpp` | one block | Unknown | None |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiIncubator.cpp` | 5 blocks | Creature incubator UI — specific steps of the bio-engineering minigame | None |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiCharacterSheet.cpp` | 1 block | Character sheet — section unclear | None |

### Tool-code blocks (not era-related, noted for completeness)

The MayaExporter, TerrainEditor, AnimationDialog, and Viewer applications account for the majority of `#if 0` instances in `src/engine/`. These are artist-tool-specific disabled experimental features and are unrelated to the Pre-CU→NGE transition.

**Finding:** The `ServerSecureTrade.cpp` block is the clearest example of a CU-era game design decision encoded as a disabled-code swap — original per-item-removal logic replaced by a cancel-on-remove policy. The ConnectionManager debug commands are structural refactor leftovers. Most other blocks have no explanation.

---

## 3. Feature Flags & Dead Switches

**Runtime guards:** No `if (useOldSystem)`, `if (preCuBehavior)`, or similar boolean guards on era-branching code paths were found in `src/engine/` or `src/game/`. The NGE codebase does not use runtime feature flags to select between Pre-CU and NGE behaviors — it is a clean cut.

**`#ifdef` guards:** No `#ifdef OLD_*`, `#ifdef LEGACY_*`, `#ifdef PRE_CU`, or `#ifdef NGE` macros found in C++ source.

**Config-gated features:** No `Config::get...("useLegacy...")` or similar config keys found. The only surviving config-gated feature disable patterns in the client are for the systems already guarded and now targeted by Phase 7 cleanup (Vivox voice chat, G15 LCD, Bink video) — those are not era-related.

**Finding:** The transition was implemented as a hard code replacement, not feature-flagged branching. There are no active runtime branches selecting Pre-CU vs NGE behavior.

---

## 4. Era-Suggestive Naming

### Header stubs with "Old" suffix (orphaned, no implementations)

Three public header files exist in `swgClientUserInterface/include/public/` with `Old` in their names. Each is a one-line forwarding include to a source-relative path that **does not exist on disk**:

| Header | Content | Target (missing) |
|--------|---------|-----------------|
| `SwgCuiResourceExtractionOld.h` | `#include "../../src/shared/page/SwgCuiResourceExtractionOld.h"` | `src/shared/page/SwgCuiResourceExtractionOld.h` — does not exist |
| `SwgCuiResourceExtractionOld_Hopper.h` | same pattern | `SwgCuiResourceExtractionOld_Hopper.h` — does not exist |
| `SwgCuiResourceExtractionOld_Quantity.h` | same pattern | `SwgCuiResourceExtractionOld_Quantity.h` — does not exist |

The `Old` suffix names a pre-NGE resource extraction UI that was replaced. The source implementations were deleted; the public forwarding headers were left behind, pointing at nothing. Including these headers would immediately produce a compiler error ("file not found"). They are safe to delete.

### Other naming searches

No `*Legacy*.cpp`, `*Legacy*.h`, `*Deprecated*.cpp`, `*Deprecated*.h`, `*V1*.cpp`, or `*V1*.h` files found in `src/engine/` or `src/game/`.

No class declarations matching `class.*Old`, `class.*Legacy`, or `class.*Deprecated` found in game source headers.

**Finding:** The three `SwgCuiResourceExtractionOld*.h` stubs are the only era-suggestive named artifacts. They are dead weight — broken forwarding headers with no implementations.

---

## 5. Dead Command Handlers & Schematic Revocation Infrastructure

### Schematic revocation (active code, aware of CU bypass)

The schematic-revocation system is fully implemented and reachable via GM console commands. It predates the CU but survived because GM tools continued to use it after the skill system changed. The `fromSkill=false` call path (from console) explicitly acknowledges it bypasses post-CU logic:

| File | Lines | Description |
|------|-------|-------------|
| `src/engine/server/library/serverGame/src/shared/console/ConsoleCommandParserSkill.cpp` | 40–62 | `revokeSchematicGroup` and `revokeSchematic` registered as GM console commands |
| `src/engine/server/library/serverGame/src/shared/console/ConsoleCommandParserSkill.cpp` | 383–427 | Parser dispatches to `creature->revokeSchematic(...)` |
| `src/engine/server/library/serverGame/src/shared/object/CreatureObject.cpp` | 3832–3845 | `revokeSchematic()` — delegates to `playerObject->revokeSchematic()` |
| `src/engine/server/library/serverGame/src/shared/object/PlayerObject.cpp` | 975–994 | `revokeSchematic()` — walks schematic list, removes by CRC; emits WARNING if ref count > 1 and `fromSkill=false` |

This is not dead code — it is live, functional GM tooling that happens to predate the CU. The `WARNING` log on line 994 documents developer awareness of the bypass.

### ConnectionManager debug listing commands

The debug console commands `characters`, `clusters`, and `gameservers` in `SwgCuiCommandParserNet.cpp` are structurally present but entirely disabled (`#if 0`). The surrounding code compiles and the commands are registered, but their bodies produce no output (just header strings). These were disabled during a `ConnectionManager` refactor and never re-enabled.

**Finding:** No unreachable UI pages or orphaned SwgCui registrations were found beyond the three broken header stubs above. The revocation commands are reachable GM tools, not dead code — they just carry a comment explaining they bypass a post-CU invariant.

---

## 6. Asset References & Database Transition Records

### Item conversion tables (historical migration artifacts)

These files in `src/game/server/database/item_conversion/` document the live-to-NGE player inventory conversion that ran at NGE launch. They are not active C++ code paths — they are data files consumed by a one-time migration script:

| File | Lines | Purpose |
|------|-------|---------|
| `armor_conversion.txt` | 242 | Tab-delimited: Pre-CU armor template → NGE armor template + new script assignments |
| `weapon_conversion.txt` | 135 | Weapon component stat remaps (damage, accuracy, wound chance ranges) |
| `armor_component_conversion.txt` | — | Armor component-level conversions |
| `food_conversion.txt` | — | Food item remapping |
| `medicine_conversion.txt` | — | Medicine remapping |
| `powerup_conversion.txt` | — | Powerup remapping |
| `spice_conversion.txt` | — | Spice item remapping |
| `saber_conversion.txt` | — | Lightsaber remapping |
| `schematics_map.sql` | **2,514** | SQL: complete schematic template remapping for the NGE conversion |
| `item_conversion.pl` | — | Perl: reads `*.txt` tables, generates SQL conversion rules |
| `item_conversion.sql` / `.sql.template` | — | Generated SQL output + template |
| `hotfix_1.sql`, `3.sql`, `4.sql`, `5.sql`, `7.sql` | — | Post-conversion corrections for known bad conversions |

The `schematics_map.sql` at 2,514 lines is the single largest era-transition artifact in the repository. It maps every Pre-CU/CU schematic template path to its NGE equivalent (or marks it as deprecated/removed).

### Hard-coded TRE names in C++ source

No explicit `.tre` filename strings (e.g., `"terrain.tre"`, `"patch_15.tre"`) were found in `src/engine/` or `src/game/` source that would constitute a missing-asset reference. TRE loading is driven by `searchPath` config lines (in `client.cfg` / `servercommon.cfg`), not hard-coded paths in C++.

**Finding:** No broken asset references in C++. The item conversion directory is a complete historical record of the NGE transition migration — 8 conversion tables, a generation script, a 2,514-line schematic SQL map, and 5 hotfix patches.

---

## Interpretation Notes

**Deliberate vs accidental:**

| Artifact | Assessment |
|----------|------------|
| `CommandTable.cpp` CU comment | Deliberate preservation — developer left attribution and date for the change |
| `ServerSecureTrade.cpp` `#if 0` | Deliberate archive — old item-removal logic preserved in-place for reference; CS policy drove the replacement |
| `SwgCuiResourceExtractionOld*.h` stubs | Accidental — implementations deleted, forwarding headers left pointing at nothing; dead weight |
| `ConnectionManager` `#if 0` blocks | Deliberate disable during a refactor that was never completed |
| `IndexedTableBuffer.h` entire-file `#if 0` | Incomplete refactor (`@todo code reorg`) — entire header disabled, presumably never re-integrated |
| Schematic revocation infrastructure | Maintained code — still needed for GM commands; CU-era origin but active in NGE |
| Item conversion tables | Intentional historical record — one-time migration artifacts kept for audit trail |

**Maintenance pattern:** This codebase used "disable in place" (`#if 0`) rather than `#ifdef ERA_NAME` branching or runtime feature flags for transition artifacts. There are no surviving Pre-CU runtime branches — only disabled blocks and deleted-but-not-cleaned-up headers.

**Relevance to Phase 7 (Dead Code Removal — Track A):** The three `SwgCuiResourceExtractionOld*.h` stubs are safe immediate deletes — they are broken headers with no implementations and no callers. The `#if 0` blocks are cosmetically dead but harmless to compilation and not tracked as Phase 7 targets. The item conversion SQL tables are documentation, not C++ build targets, and should be preserved as historical record. None of these artifacts affect the Phase 7 CLEAN-01 through CLEAN-04 scope.

---

*Survey method: targeted `grep` sweeps across `src/engine/` and `src/game/` for CU/NGE keywords, `#if 0` patterns, and `Old`/`Legacy`/`Deprecated` naming; direct file reads to verify agent-reported findings; line-count checks on conversion tables.*
