---
phase: 14-voice-chat-vivox-source-removal
reviewed: 2026-05-26T19:23:06Z
depth: standard
files_reviewed: 18
files_reviewed_list:
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp
  - src/engine/client/library/clientUserInterface/src/shared/core/CuiMenuInfoTypes.h
  - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
  - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj
  - src/engine/client/application/AnimationEditor/build/win32/AnimationEditor.vcxproj
  - src/engine/client/application/ClientEffectEditor/build/win32/ClientEffectEditor.vcxproj
  - src/engine/client/application/LightningEditor/build/win32/LightningEditor.vcxproj
  - src/engine/client/application/NpcEditor/build/win32/NpcEditor.vcxproj
  - src/engine/client/application/ParticleEditor/build/win32/ParticleEditor.vcxproj
  - src/engine/client/application/SwooshEditor/build/win32/SwooshEditor.vcxproj
  - src/engine/client/application/Viewer/build/win32/Viewer.vcxproj
  - src/engine/client/library/clientGame/build/win32/includePaths.rsp
  - src/engine/client/library/clientUserInterface/build/win32/includePaths.rsp
  - src/game/client/application/SwgClient/build/win32/libraries_d.rsp
  - src/game/client/application/SwgClient/build/win32/libraries_o.rsp
  - src/game/client/application/SwgClient/build/win32/libraries_r.rsp
  - src/game/client/application/SwgClient/build/win32/libraryPaths.rsp
  - src/external/3rd/library/soePlatform/copy-libs.bat
findings:
  critical: 1
  warning: 1
  info: 2
  total: 4
status: issues_found
---

# Phase 14: Code Review Report

**Reviewed:** 2026-05-26T19:23:06Z
**Depth:** standard
**Files Reviewed:** 18
**Status:** issues_found

## Summary

Phase 14 removes the Vivox voice-chat subsystem: voice call sites, voice preference
keys/accessors, three voice radial-menu enum values, and vivox/VChat link + include
tokens from build files. The build-file edits (9 `.vcxproj`/`.rsp`/`.bat` files) are
clean — every voice token was removed on deletion lines only, no addition line
reintroduces a voice token, XML structure in the `<AdditionalDependencies>` /
`<AdditionalLibraryDirectories>` blocks is preserved, and non-voice neighbors
(`Base.lib`, `ChatAPI.lib`, `dpvs.lib`) were correctly kept. `CuiPreferences.cpp/.h`
removed the six voice option pairs and their registrations in matched pairs with no
surviving caller anywhere in `src` (verified by repo-wide grep → 0 matches for
`CuiVoiceChat|SwgCuiVoice|VOICE_INVITE|VOICE_KICK|ms_voice|...`).

However, the change to `CuiMenuInfoTypes.h` introduces a **BLOCKER**: deleting three
enum values from the *middle* of `Cui::MenuInfoTypes::Type` shifts the ordinals of
every subsequent client menu item (`ITEM_EQUIP_APPEARANCE`, `ITEM_UNEQUIP_APPEARANCE`,
`OPEN_STORYTELLER_RECIPE`, `GOD_TELEPORT`) down by 3. Those ordinals are used as
**row-index keys** into the retail-TRE `radial_menu.iff` datatable, which is NOT
modified by this phase. The enum and datatable are now misaligned — surviving menu
items will resolve the wrong command / range rows. This class of defect is invisible
to both the link-grep gate (no unresolved symbols) and a casual boot gate (only
manifests when a player opens a radial on an appearance-tab item / storyteller recipe,
or an admin uses GOD_TELEPORT). The phase planning docs (14-01-PLAN.md:141) explicitly
considered and dismissed this as "client-internal," but that analysis is inverted — see
CR-01.

## Critical Issues

### CR-01: Enum-ordinal shift in CuiMenuInfoTypes::Type desyncs surviving menu items from the retail radial_menu datatable

**File:** `src/engine/client/library/clientUserInterface/src/shared/core/CuiMenuInfoTypes.h:161-167`

**Issue:**
The diff deletes three enum values (`VOICE_SHORTLIST_REMOVE`, `VOICE_INVITE`,
`VOICE_KICK`) from the middle of the `enum Type`, immediately after
`GROUP_USE_PICKUP_POINT_NOCAMP`. Because these are unvalued enumerators, removing
them shifts the ordinal of every following enumerator down by 3:

| Enumerator | Ordinal BEFORE | Ordinal AFTER |
|---|---|---|
| `GROUP_USE_PICKUP_POINT_NOCAMP` | 102 | 102 |
| `ITEM_EQUIP_APPEARANCE` | 106 | **103** |
| `ITEM_UNEQUIP_APPEARANCE` | 107 | **104** |
| `OPEN_STORYTELLER_RECIPE` | 108 | **105** |
| `GOD_TELEPORT` | 109 | **106** |
| `CLIENT_MENU_LAST` | 110 | **107** |

These ordinals are consumed as **datatable row-index keys**, not as opaque symbols.
`RadialMenuManager::install()` (`src/engine/shared/library/sharedGame/src/shared/core/RadialMenuManager.cpp:65-76`)
loads the retail TRE file `datatables/player/radial_menu.iff` and builds
`s_ranges[rowIndex] = {range, commandName, useRadialTarget}` keyed by the **row index `i`**.
The client then calls `RadialMenuManager::getCommandForMenuType(menuType)` /
`getRangeForMenuType(menuType)` (`RadialMenuManager.cpp:83-118`) passing the **enum
ordinal** as `menuType`. The enum ordinal and the datatable row index MUST be equal for
the lookup to return the correct row.

There is **no in-repo `radial_menu.tab`/`.iff` source** (verified — the datatable is
retail-TRE-only and is unchanged by this phase). The datatable still has (per
`.planning/phases/09-stlport-msvc-stl/baseline/before-datatable-round-trip.txt`):

```
103  caption=VOICE_SHORTLIST_REMOVE  command=(empty)         range=16384.0
104  caption=VOICE_INVITE            command=voiceInvite     range=16384.0
105  caption=VOICE_KICK              command=voiceKick       range=16384.0
106  caption=ITEM_EQUIP_APPEARANCE   command=equipAppearance range=6.0
107  caption=ITEM_UNEQUIP_APPEARANCE command=unequipAppearance range=6.0
108  caption=OPEN_STORYTELLER_RECIPE command=openStorytellerRecipe range=6.0
```

After the shift, the live menu-build calls in `CuiRadialMenuManager.cpp` resolve to the
WRONG datatable rows:

- `addRootMenu(ITEM_EQUIP_APPEARANCE, ...)` (`CuiRadialMenuManager.cpp:1420,1452`) now
  passes ordinal 103 → `getCommandForMenuType(103)` returns row 103 (`VOICE_SHORTLIST_REMOVE`,
  **empty command** → clicking "Equip Appearance" does nothing) and `getRangeForMenuType(103)`
  returns **16384.0** instead of 6.0.
- `addRootMenu(ITEM_UNEQUIP_APPEARANCE, ...)` (`CuiRadialMenuManager.cpp:1410,1448`) now
  passes ordinal 104 → resolves to row 104 (`voiceInvite` command).
- `OPEN_STORYTELLER_RECIPE` (ordinal 105) → resolves to row 105 (`voiceKick` command).
- `addRootMenu(GOD_TELEPORT, ...)` (`CuiRadialMenuManager.cpp:1800`) shifts 109→106 →
  resolves to row 106 (`equipAppearance`).

`getLocalizedLabel()` is unaffected (it keys the `typeStringIds` map by the *symbolic*
`TypePair(MenuInfoTypes::a, ...)` token in `CuiMenuInfoTypes.cpp`, so labels still
display correctly) — which makes this even more insidious: the menu *renders the right
text* but *executes the wrong (or no) command* and uses the wrong activation range.

The planning rationale (14-01-PLAN.md:141, "orphaned retail-TRE radial rows referencing
the old type ids are ignored by the menu builder") is the inverse of the real failure
mode. The problem is not orphaned rows being ignored — it is *surviving* client menu
items now indexing the wrong (voice) rows. This is a functional regression in
ground-game radial menus that the D-09 link-grep gate and a quick boot gate cannot
detect.

**Fix:**
Do not renumber the surviving menu items. Replace the deleted enumerators with
explicitly-valued placeholders so the ordinals of everything after them are preserved
and stay aligned with the unchanged retail datatable rows 103-105:

```cpp
        GROUP_CREATE_PICKUP_POINT, // 100
        GROUP_USE_PICKUP_POINT,
        GROUP_USE_PICKUP_POINT_NOCAMP,
        OBSOLETE_RADIAL_103,   // was VOICE_SHORTLIST_REMOVE (retired; row 103 reserved)
        OBSOLETE_RADIAL_104,   // was VOICE_INVITE          (retired; row 104 reserved)

        OBSOLETE_RADIAL_105,   // was VOICE_KICK, // 105    (retired; row 105 reserved)
        ITEM_EQUIP_APPEARANCE, // 106 — must remain 106 to match radial_menu row 106
        ITEM_UNEQUIP_APPEARANCE,
        OPEN_STORYTELLER_RECIPE,
        GOD_TELEPORT,

        CLIENT_MENU_LAST,
```

Alternatively, anchor the values explicitly (e.g. `ITEM_EQUIP_APPEARANCE = 106,`) so the
intent is self-documenting and robust to future edits. Either way, leave the
`MAKE_ID(VOICE_*, ...)` lines out of `CuiMenuInfoTypes.cpp` (already done) — the
placeholder enumerators simply must not have `MAKE_ID` entries, which is fine because
nothing references them. After the fix, re-verify `ITEM_EQUIP_APPEARANCE == 106` and
re-run the radial-menu interaction path in a boot test (equip/unequip an appearance-tab
item; trigger an Open Storyteller Recipe; admin GOD_TELEPORT on a waypoint).

Also note: the `SERVER_MENU* = CLIENT_MENU_LAST + N` offsets (header lines 175-178)
depend on `CLIENT_MENU_LAST`. With the current (broken) change `CLIENT_MENU_LAST` shifts
110→107, moving all `SERVER_*` ids by -3 as well; restoring the ordinals per the fix
above also restores these.

## Warnings

### WR-01: Stale `voiceChatEnabled` config key left in stage/client_d.cfg after its option registration was removed

**File:** `stage/client_d.cfg:91-93`

**Issue:**
`CuiPreferences.cpp` removed `REGISTER_OPTION_USER(voiceChatEnabled)` and the backing
`ms_voiceChatEnabled` variable, but `stage/client_d.cfg` still carries:

```
[ClientUserInterface]
	# Disable voice chat: Vivox is a dead service ...
	voiceChatEnabled=false
```

With the option no longer registered in `CurrentUserOptionManager`/`LocalMachineOptionManager`,
this key is now dead — it is silently ignored at load (the option system tolerates
unknown keys, so this is not a crash). It is stale configuration that no longer has any
effect and misleadingly implies a runtime voice toggle still exists. (`stage/client_d.cfg`
was not part of this phase's commit diff, so this is residue the removal left behind
rather than a newly-introduced line.) The `[ClientUserInterface] disableG15Lcd`-style
comments suggest this file is curated; leaving a no-op voice key invites future
confusion.

**Fix:**
Remove lines 91-93 from `stage/client_d.cfg`. (Per project memory
"stage/client_d.cfg deferred cleanup," accumulated test settings are scheduled for a
cleanup pass — fold this into that, or delete now since it is reversible.)

## Info

### IN-01: Dead speakerVolume/micVolume preference pair survives with no remaining consumer

**File:** `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp:397-398, 841-842, 3460-3484`

**Issue:**
`ms_speakerVolume` / `ms_micVolume`, their `REGISTER_OPTION(...)` registrations, and the
`getSpeakerVolume/setSpeakerVolume/getMicVolume/setMicVolume` accessors
(`CuiPreferences.h:553-557`) were retained, but their only callers were the deleted
voice UI (`SwgCuiOptVoice`, voice fly-bar). A repo-wide grep shows the only remaining
references are the definition + registration inside `CuiPreferences` itself — these are
now dead code. They are harmless (registered as user options, never read), so this is
informational, not a defect. Whether they are voice-coupled or generically "audio
device volume" is a judgement call; if the latter, they could be intentionally kept for
a future audio-options screen.

**Fix:**
If confirmed voice-only, remove the two variables, their two `REGISTER_OPTION` lines, and
the four accessor definitions/declarations for completeness of the voice removal.
Otherwise, leave a comment noting they are retained for future non-voice audio options.

### IN-02: Vestigial .rsp link/path files edited alongside inline vcxproj

**File:** `src/game/client/application/SwgClient/build/win32/libraries_d.rsp`, `libraries_o.rsp`, `libraries_r.rsp`, `libraryPaths.rsp`

**Issue:**
Per project memory ("Decruft removal build-graph + invocation gotchas: .rsp vestigial —
edit inline vcxproj"), the `*.rsp` files are not consumed by the active MSBuild graph;
the live link inputs are the inline `<AdditionalDependencies>` /
`<AdditionalLibraryDirectories>` in the `.vcxproj`. The voice-token deletions from these
`.rsp` files (e.g. `vivoxSharedWrapper_debug.lib`, `vivoxSharedWrapper/lib`) are correct
and consistent, but they have no build effect. This is noted only so the next reviewer
does not assume these files gate the build — the `.vcxproj` edits (verified clean) are
what matters.

**Fix:** None required. Consistent cleanup; no action.

---

_Reviewed: 2026-05-26T19:23:06Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
</content>
</invoke>
