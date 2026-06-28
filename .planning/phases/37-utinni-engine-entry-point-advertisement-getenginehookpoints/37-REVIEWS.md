---
phase: 37
reviewers: [codex, cursor, sonnet, opus]
reviewed_at: 2026-06-21
plans_reviewed: [37-01-PLAN.md, 37-02-PLAN.md, 37-03-PLAN.md]
note: claude (self) skipped — running inside Claude Code; gemini/opencode/qwen not installed. Codex + Cursor via CLI, Sonnet + Opus via fresh agents (the repo's documented 4-AI consult crew), each on a non-overlapping focus angle.
---

# Cross-AI Plan Review — Phase 37 (Utinni `GetEngineHookPoints`)

Four independent reviewers on non-overlapping angles:
- **Codex** — repo tracer: verify the engine-symbol claims against actual source.
- **Cursor** — C++ mechanism detail: PMF→`void*` legality, the coverage `static_assert` bridge, undecorated-export-from-EXE.
- **Sonnet** — lateral: sequencing, scope, executability, blind spots.
- **Opus** — contract/spec soundness: ABI, version field, EPA-02 thunk, the count≠correctness gap, verifiability boundary.

---

## Codex Review

### Summary
The 3-plan set has the right high-level shape: additive exe export, source-bound symbol table, Win32 validation, and explicit handling for virtuals/ctors/private members. But it is too optimistic about coverage and symbol correctness. The biggest issues are that the "zero-missing" gate is only a count check, several MVP/full-set symbol names are wrong or unresolved in this repo, and the build wiring as written will likely affect x64 despite Phase 37 being 32-bit-only.

### Strengths
- Mirrors the shipped `gl11` pattern accurately: `Direct3d11.cpp` exports `GetHookPoints` with `extern "C" __declspec(dllexport) ... __cdecl` at the cited block.
- Correctly identifies `config::loadOverrideConfig` as a zero-arg thunk around `ClientMainNamespace::installConfigFileOverride()`, not `ConfigFile::loadFromBuffer`.
- Correctly flags the main C++ landmines: virtual PMFs, constructors, private members, overloaded functions.
- The `/FORCE` link-warning trap is handled explicitly with an unresolved-symbol grep.
- The plan's "omit rather than guess" rule is the right policy for this kind of detour table.

### Concerns
- **HIGH: coverage gate does not prove zero-missing.** The proposed `static_assert(table_count == inc_count)` only proves equal counts. It can pass with `game::install` missing and a duplicate/wrong name added elsewhere. The runtime no-null/no-dup scan also does not verify that every `.inc` required name exists in the table. This directly undercuts EPA-04.
- **HIGH: Unconditioned `ClCompile` contradicts 32-bit-only scope.** 37-01 Task 2 says add `utinni_advertise.cpp` with no condition and "Do NOT add an x64 config." In this `.vcxproj`, an unconditional source item applies to all platforms, including existing x64 configs. That can accidentally export `GetEngineHookPoints` from x64 or fail x64 builds due to PMF assumptions. Use a Win32 condition or `#if !defined(_WIN64)` guard.
- **HIGH: shared header with `__declspec(dllexport)` is risky if copied to Utinni.** 37-01 says `utinni_engine_hookpoints.h` is shared verbatim with `D:/Code/Utinni` and contains the dllexport declaration. A consumer repo should not include a provider-side `dllexport` declaration. Split the provider export declaration from the shared struct/name header, or macro-gate it.
- **HIGH: `ClientMain` shim can create name ambiguity.** `ClientMain.cpp` defines `ClientMainNamespace::installConfigFileOverride()` and then `using namespace ClientMainNamespace;`. Adding a same-signature global forwarding function named `installConfigFileOverride()` may make existing unqualified calls ambiguous or alter lookup. Prefer renaming the external shim, or promote/qualify deliberately and update call sites explicitly.
- **HIGH: several MVP UI rows are not source names.** In `SwgCuiChatWindow.h`, `enableTextInput` is actually `acceptTextInput`; `writeToAllTabs` is `appendToAllTabs` (line 172); `writeToCurrentTab` is `appendTextToCurrentTab` (line 174); `chatEnterHandler` appears to map to `performEnterKey()` (line 214), not a handler by that name. `CuiConsoleHelper` has `processInput`, not `sendInput`.
- **HIGH: 37-03 `extent::intersect -> &Extent::intersect` is wrong as written.** `Extent` itself has no `intersect` member (`Extent.h:24`). The non-virtual overloads live on `BaseExtent` (`BaseExtent.h:45-47`); `Extent` has virtual `realIntersect` (`Extent.h:90`). The advertised symbol should resolve to `BaseExtent::intersect` overloads or be skipped/triaged, not `Extent::intersect`.
- **MEDIUM: PMF-to-void* is MSVC-pragmatic, not well-defined C++.** The union type-pun plus `sizeof(PMF)==sizeof(void*)` is not standard-defined, and it does not prove the value is an implementation entry address. Probably workable for MSVC Win32 single-inheritance non-virtual PMFs, but the plan should explicitly validate representative PMF rows by disassembly/dump or a small probe. The static assert will not catch virtuals or semantic thunk targets.
- **MEDIUM: confirmed symbol spot-checks are mixed.** Correct: `Game::install` static (`Game.h:94`); `CommandParser::addSubCommand` public non-virtual (`CommandParser.h:149`); `Graphics::install/present/screenShot/setHardwareMouseCursorEnabled` exist (`Graphics.h:70`). But `CommandParser::createDelegateCommands` is protected (line 180), not addressable without a friend/thunk.
- **MEDIUM: Object virtual classification is overstated/imprecise.** `Object::addToWorld`/`removeFromWorld` are virtual (`Object.h:120`). But `getObjectType()` is non-virtual at line 152 (not `getType()`), and `move_p`/`move_o` are non-virtual at lines 251-252 (not `move()`). The full-set plan should not carry "Object::getType/move verified virtual" forward.
- **MEDIUM: GroundScene claims mostly hold, but ctor/init must be handled carefully.** `GroundScene::draw`/`processEvent` virtual (`GroundScene.h:203`); `reloadTerrain`/`setView`/`getCurrentCamera` public non-virtual (207-215); `init/update/handleInputMap*` private. `GroundScene` inherits `NetworkScene`, so PMF size/representation should be validated before relying on raw extraction.
- **LOW: export claim is plausible but still needs the 37-01 dumpbin gate.** The shipped DLL analog supports the claim. Don't add a `.def` preemptively, but treat EXE export verification as a real gate, not a proven fact.

### Suggestions
- Replace count-only coverage with a real required-name lookup: generate `s_requiredNames[]` from the `.inc`, then runtime-check every required name is present exactly once and non-null. Keep the count assert only as a cheap drift smoke.
- Make `utinni_advertise.cpp` Win32-only in the project or source. Do not accidentally compile/export it in x64.
- Split shared contract from provider export decoration: shared header = structs/version/name list; SwgClient-only TU declares/defines `extern "C" __declspec(dllexport)`.
- Add an explicit "verified mapping table" before 37-02 implementation for every MVP row: contract name, actual source symbol, signature, static/member/virtual/private, action.
- Treat all UI/chat rows as unresolved until remapped to the actual names above.
- For PMF rows, add a small MSVC Win32 probe or documented dumpbin/disassembly check for representative `CommandParser`, `GroundScene`, `WorldSnapshot` rows.

### Risk Assessment
**Overall: MEDIUM-HIGH.** The core export idea is sound, and the spike can likely prove it quickly. The catalog plans are the risky part: several claimed symbols are wrong, the coverage gate can falsely pass, and the x64/build-header details could create collateral damage. Tightening those before implementation would materially reduce the chance of shipping a table that builds but advertises wrong or incomplete hook points.

---

## Cursor Review

### Summary
The three-plan wave (37-01 spike → 37-02 MVP → 37-03 full catalog) is sensibly sequenced, mirrors the shipped `gl11_r.dll!GetHookPoints` pattern, and explicitly documents the real C++ landmines (virtual/ctor/private/overload/inline). The spike-first approach is the right de-risk. Material gaps: the PMF `union` helper is MSVC-pragmatic but not standards-safe and its `static_assert` does **not** exclude virtual or semantic mismatches; the coverage bridge has a **documented off-by-one split** between 37-01 and 37-PATTERNS/37-RESEARCH; and several symbol-resolution claims in 37-RESEARCH/37-03 (especially `object::getType`/`object::move`, `extent::intersect`) do not match this repo's headers. Export via `extern "C" __declspec(dllexport) __cdecl` from a 32-bit EXE should yield undecorated `GetEngineHookPoints` (not `_GetEngineHookPoints@0`, which would imply `__stdcall`), but that remains unproven until 37-01 Task 3 dumpbin runs — the handoff spec is correctly more cautious than the plans.

### Strengths
- **Spike-before-catalog sequencing** isolates the four hard mechanisms (undecorated export, PMF conversion, `installConfigFileOverride` exposure, coverage scaffold) before ~230 rows.
- **EPA-02 correction is correct and verified**: zero-arg `config::loadOverrideConfig` → thunk around `installConfigFileOverride()` (`ClientMain.cpp:98-109`), not `ConfigFile::loadFromBuffer` (`ConfigFile.h:136`).
- **Confirmed spike symbols check out**: `Game::install` static (`Game.h:94`); `CommandParser::addSubCommand` public non-virtual (`CommandParser.h:149`).
- **Landmine rules are stated honestly**: virtual `&fn` = vtable thunk, count `static_assert` won't catch it (37-02 Task 2, 37-PATTERNS:108-109).
- **Exe-local header** avoids the shared-header ABI cascade (AGENTS.md).
- **`/FORCE` link-grep gate** is the right mitigation for typo'd `&Class::method` rows.
- **Out-of-repo acceptance is explicitly deferred** (37-VALIDATION.md, 37-03 Task 3) — honest about what build-time checks cannot prove.

### Concerns
**C++ mechanism (assigned focus):**
- **HIGH — PMF `union` is not well-defined in ISO C++**, even with the size guard. Reading `u.p` after writing `u.m` is type-punning through a non-common initial member; MSVC tolerates it for hooking but it is not portable/strictly defined. Prefer `std::bit_cast<void*>` (C++20, already enabled) or `memcpy`, keeping the size guard.
- **HIGH — `static_assert(sizeof(PMF)==sizeof(void*))` does not rule out the unsafe cases it claims to.** On 32-bit MSVC, single-inheritance virtual PMFs are still 4 bytes — the assert passes, but `&Object::addToWorld` (virtual, `Object.h:120`) yields a vtable dispatch stub, not the impl. RESEARCH also wrongly lists `Object::getType`/`move` as virtual at `Object.h:120-121` — those lines are `addToWorld`/`removeFromWorld` only.
- **HIGH — Coverage `static_assert` is internally inconsistent.** 37-01 Task 2 uses `(sizeof table / sizeof[0]) == UTINNI_REQUIRED_COUNT` with **no sentinel**; 37-PATTERNS:176-177 uses **`- 1`**, matching 37-RESEARCH:433-437 which adds `{ nullptr, nullptr }`. An executor mixing docs gets a compile failure or a silent off-by-one in `count`. 37-01's no-sentinel version is correct; align PATTERNS/RESEARCH.
- **MEDIUM — Count bridge catches only `.inc`↔table row drift, not correctness.** Won't catch: wrong function at non-null address, virtual vtable thunks, accessor-as-global semantic mismatch (`game::g_mainLoopCounter` → `&Game::getLoopCount`), or inline methods with no unique address (`Object::move_o` inline at `Object.h:1216`).
- **MEDIUM — Confirmed: virtual-advertised-as-`&fn` will NOT fail the coverage gate.** Only greps/landmine discipline catch it. One missed grep in 37-03's ~160 new rows ships a wrong detour target with a green build.

**Undecorated export from EXE:**
- **LOW (mechanism) / MEDIUM (evidence)** — `extern "C" __declspec(dllexport) __cdecl` should produce undecorated `GetEngineHookPoints` on 32-bit MSVC (internal `_GetEngineHookPoints`; `@0` would mean `__stdcall`). No `ModuleDefinitionFile` in the vcxproj. But the analog is proven for `gl11_r.dll`, not yet for `SwgClient_*.exe`; handoff §5 is correctly more cautious. `/EXPORT` fallback is reasonable insurance, not proof the primary path fails.
- **LOW** — export decl in both header and `.cpp`; harmless if identical (ODR).

**Symbol-resolution risk (spot-check):**
- **HIGH** — `object::getType`/`object::move` virtual at `Object.h:120-121` is wrong lines + wrong symbols. `getType` doesn't exist (non-virtual `Tag getObjectType() const` at `Object.h:152`); `move` doesn't exist (non-virtual `move_p`/`move_o` at 251-252, `move_o` inline at 1216). Advertising `&Object::getType` won't compile; skipping as virtual omits required names.
- **HIGH** — `extent::intersect` → `&Extent::intersect` doesn't exist. Non-virtual `BaseExtent::intersect` overloads (`BaseExtent.h:45-47`); `Extent::realIntersect` protected virtual (`Extent.h:90`). Needs overload resolution on `BaseExtent::intersect` via `pmfToVoid` + `static_cast`.
- **MEDIUM** — `commandParser::createDelegateCommands` is `protected` (`CommandParser.h:180`).
- **MEDIUM** — `game::g_mainLoopCounter`: `ms_loops`/`ms_done` private (`Game.h:276-277`); `getLoopCount()` accessor inline (`Game.h:190`/398). Utinni may expect a readable global address, not a callable accessor.
- **MEDIUM** — `graphics::present` no-arg cast: overloads at `Graphics.h:161-162`; cast approach right, must match the UtinniCore typedef exactly.
- **LOW** — MISMATCH corrections (`loadFile`, `run`, `getPlayerCreature`, `screenShot`, `setHardwareMouseCursorEnabled`, `setView`, `getIoWin`) verified against the cited headers.

**Sequencing/scope/verifiability:**
- **MEDIUM** — 37-03 SKIP policy vs shared `.inc`: virtual endpoints excluded from `.inc` required-set, but Utinni's contract names (~230) still include them. EPA-04 "zero-missing" = zero-missing on the trimmed required-set, not Utinni's full hook list — needs explicit Utinni-side coordination.
- **MEDIUM** — Ctor thunk calling convention: 32-bit MSVC member ctors are typically `__thiscall`; plan shows generic `CtorThunk(...)` without mandating it — ABI mismatch passes link-grep, fails at first detour.
- **MEDIUM** — `utinni_verifyNoNullNoDup()` scaffolded but not wired into boot until optionally `#if !PRODUCTION`; "verify by inspection" is weak for ~230 rows.
- **LOW** — 5-target build is heavy but correct per AGENTS.md.
- **LOW** — `ClientMain.cpp` `using namespace` at line 112; shim-at-file-scope is fine but must not break internal callers.

### Suggestions
1. Pick one coverage idiom (standardize on 37-01's no-sentinel); fix 37-PATTERNS:176 and 37-RESEARCH:429-437.
2. Replace PMF `union` with `std::bit_cast<void*>(pmf)` under the existing `static_assert`; comment that the guard catches MI/VI inflation only, not virtual dispatch stubs.
3. Add a "symbol kind" checklist per row in 37-02/03 (`{static | pmf-nonvirtual | thunk | accessor | skip-virtual | omit}`) verified against the header before writing the row.
4. Fix RESEARCH/37-03 object bucket: `getType`→`getObjectType` (non-virtual PMF); `move`→resolve against `move_o`/`move_p` (force emission if inline, or OMIT); remove the false "virtual at Object.h:120-121" citation.
5. Fix `extent::intersect`: target `BaseExtent::intersect` with explicit overload `static_cast`.
6. 37-01 Task 3: treat dumpbin on `SwgClient_d.exe` as blocking; apply `/EXPORT` pragma if decorated, per handoff §5.
7. Wire `utinni_verifyNoNullNoDup()` into Debug boot before declaring EPA-04 green.
8. Document accessor-vs-global rows with "call-not-read" semantics.
9. Ctor thunks: mandate `__thiscall` (or match the UtinniCore typedef exactly) in 37-PATTERNS Pattern 4.

### Risk Assessment
**Overall: MEDIUM.** The architecture is sound and the easy static rows are low risk. Risk rises to medium because: (1) PMF conversion + virtual/inline landmines can ship plausible-but-wrong addresses with all in-repo gates green; (2) the coverage `-1` sentinel doc inconsistency can break the spike or corrupt `count`; (3) RESEARCH symbol errors on `getType`/`move`/`extent::intersect` will burn time or produce wrong rows if copied verbatim; (4) true acceptance remains out-of-repo and the in-repo gates are mostly structural parity, not behavioral proof. The export mechanism is likely LOW once dumpbin confirms; the PMF/count/correctness cluster is where the plans are most vulnerable.

---

## Sonnet Review

### Summary
The three-plan set is well-structured and unusually self-aware for autonomous work: it correctly identifies the hardest risks (PMF→void* under MSVC, the `/FORCE` trap, the export-decoration question, the virtual/ctor/private landmines), proposes per-row taxonomy discipline rather than bulk `&fn`, and backs most symbol claims against the actual headers. The spike-first ordering and coverage-gate design are sound in principle. However, there are concrete correctness gaps that could silently ship wrong rows despite the gate passing: a latent count off-by-one between the `static_assert` formulas in RESEARCH vs 37-01; a misidentification of `game::getCamera` as overloaded (it is not); `game::g_mainLoopCounter`'s accessor (`getLoopCount`) is inline and its address may not be ODR-emitted; and the `installConfigFileOverride` namespace-exposure step is underspecified. 37-03 at ~160 endpoints is ambitious as a single autonomous plan with no intermediate build checkpoint before Task 3.

### Strengths
- Spike-first sequencing (37-01) proves the hardest mechanism risks with ~3 rows before any catalog.
- The `/FORCE` mask-trap is correctly documented and gated (`grep "unresolved external symbol" == 0`) across all three plans.
- Virtual/ctor/private landmines are per-row, not blanket; acceptance greps for `&CuiIoWin::processEvent`, `&GroundScene::GroundScene`, `&Class::Class` catch the most dangerous mistakes.
- The EPA-02 crash-fixer mismatch is correctly resolved (zero-arg thunk, not the two-arg inner call).
- The undecorated-export claim is properly evidenced by the shipped gl11 twin; fallback documented.
- Renderer ABI protection is sound (header kept exe-local).
- Boot gate preserved at every wave (rasterMajor=5, later =11).
- Out-of-repo acceptance honestly documented as Utinni-side, not phase gaps.
- The eight known MISMATCH corrections are baked into the plans.

### Concerns
- **HIGH — Static_assert count formula inconsistent across documents; the 37-01 action is wrong.** RESEARCH:437 and PATTERNS:176 use `(sizeof/sizeof) - 1` (sentinel row); 37-01 Task 2:192 omits `- 1`. Two conflicting designs ("sentinel in array" vs "no sentinel, exact count"). An autonomous executor following 37-01's action produces an assert that disagrees with the RESEARCH/PATTERNS examples — build breaks immediately unless the executor notices.
- **HIGH — `game::getCamera` is NOT overloaded.** `Game.h:173` has one `getCamera()`; the const version is the separately-named `getConstCamera()` (line 174). 37-02-PLAN:82 marks it "OVERLOADED w/ const: cast needed" and prescribes a `static_cast`. The cast compiles and still yields the right address, but an executor "correctly" applying a cast where no ambiguity exists may cast to the wrong signature and silently produce a valid address for the wrong function type.
- **HIGH — `game::g_mainLoopCounter` accessor (`Game::getLoopCount`) is inline-defined (`Game.h:398`).** RESEARCH Pitfall 2 (inline may not be ODR-emitted) applies to this exact row but isn't flagged. `&Game::getLoopCount` may produce a non-emitted/TU-local address (link-grep catches the unresolved case but NOT the duplicate-copy case). A forced non-inline emission would be needed if this row is required.
- **HIGH — `installConfigFileOverride` external-linkage exposure underspecified, with name-collision risk.** With `using namespace ClientMainNamespace;` at `ClientMain.cpp:112`, adding a file-scope `void installConfigFileOverride()` that forwards into the namespaced one can make the name ambiguous/redefined at the definition site. The promote-out-of-namespace option requires verifying no internal callers rely on the unqualified name. Pin one approach on the EPA-02-critical row.
- **MEDIUM — The coverage `static_assert` does NOT catch virtual-advertised-as-PMF, and there's no acceptance test for it.** A `pmfToVoid(&GroundScene::draw)` (virtual) passes the sizeof assert, link-grep, count assert, and no-null/no-dup. Only the per-row taxonomy check + a grep guard catch it, and the 37-03 bucket is large enough to miss one. No structural mechanism prevents it.
- **MEDIUM — 37-03 covers ~160 endpoints across 8+ subsystems with no intermediate build checkpoint.** Tasks 1+2 author ~160 rows before the first compile in Task 3. Under `/FORCE` and with the count gate depending on `.inc`↔table synchrony, drift across two tasks is likely and only surfaces in Task 3, requiring an 80+-row re-trace.
- **MEDIUM — `.inc` → Utinni drift declared but the dual-placement coordination is untasked.** No step copies/syncs the `.inc` into `D:/Code/Utinni`. The coverage gate only enforces the client table against its own `.inc`, not against Utinni's consumer-side expected names.
- **MEDIUM — `commandParser::createDelegateCommands` is `protected` (not private), but no thunk/friend strategy is specified.** `protected` is accessible from a derived class; an executor might add a local subclass (a new class in the tree). The plan leaves resolution to discretion.
- **LOW — Spec §5 implies `.def`/`/EXPORT` was used for the gl11 twin, but the source uses `dllexport` alone.** An executor re-reading §5 without the RESEARCH correction might add an unnecessary `/EXPORT`. The dumpbin grep catches decoration, so it's caught.
- **LOW — Optimized SAFESEH warnings (LNK2026/LNK4039) are not "unresolved external symbol", so the grep gate is correct and won't false-positive.** Flagged for completeness.
- **LOW — SKIP/OMIT/DEFER must NOT be in the `.inc`** or the static_assert demands rows for them. The rule should be more prominent in 37-03 task actions (SKIP comments belong in the `.cpp` only).

### Suggestions
1. Resolve the static_assert count formula definitively in 37-01 Task 2 (pick sentinel-or-not, strike the other from all docs, add a code comment).
2. Correct the `game::getCamera` overload claim in 37-02 (no cast needed; plain `(void*)&Game::getCamera`).
3. Flag `Game::getLoopCount` as inline in the `game::g_mainLoopCounter` row; provide a non-inline-shim fallback.
4. Specify the `installConfigFileOverride` exposure precisely (move-out-of-namespace + grep callers, or a differently-named file-scope wrapper).
5. Add an intermediate build+link-grep checkpoint to 37-03 between Task 1 and Task 2.
6. Add an explicit task step to copy the updated `.inc` to `D:/Code/Utinni`.
7. Make 37-03 Task 2's virtual-skip greps comprehensive (`&Appearance::render`, `&Appearance::collide`, `&GroundScene::draw`, `&renderWorld::render`).
8. Add a note that SKIP/OMIT comments go in the `.cpp` only, never the `.inc`.

### Risk Assessment
**Overall: MEDIUM.** The mechanism is sound and the core risks are identified and mitigated. The 37-01 spike is the right design. But the static_assert count inconsistency (HIGH) breaks the spike if the executor follows the action as written; the `game::getCamera` overload error (HIGH) can propagate a wrong cast; the `installConfigFileOverride` underspecification (HIGH) targets the single most important row; and 37-03's single-wave 160-row plan with no intermediate build (MEDIUM) is the main executability risk. The phase does NOT risk breaking the boot gate or plugin ABI; the risks are all "ship a wrong row silently past the gates" — exactly what spec §0 warns against. Fixing the three HIGH items before execution would drop this to LOW.

---

## Opus Review

### Summary
The three-plan set is unusually disciplined for a binary-interop task: it correctly identifies that the hard parts are the undecorated export, the PMF→`void*` conversion, the EPA-02 crash-fixer mapping, and the coverage gate — and de-risks all four in a P0 spike before scaling. The C++ mechanism is sound for the cases actually used, and symbol spot-checks confirm the RESEARCH mismatch corrections are real. However, the contract has two soundness gaps the plans under-weight: (a) the spec mandates a `version` bump on every name add/remove but **the plans never bump `UTINNI_HOOKPOINTS_VERSION` past 1** across three additive waves; and (b) the coverage `static_assert` proves **count parity, not name/symbol correctness** — it is structurally blind to the exact failure mode (a wrong `&`) the spec calls "worse than a missing row." Combined with the only true acceptance being out-of-repo, the phase can ship a table that passes every in-repo gate yet detours the wrong functions. Acceptable as a phase boundary only if named as residual risk — the plans flag it, but overstate what the by-construction gates prove.

### Strengths
- Spike-first sequencing is correct; the coverage harness is genuinely inherited, not re-derived.
- The EPA-02 correction is well-reasoned and verified: Utinni's typedef is zero-arg (config.cpp:39); `ConfigFile::loadFromBuffer(char*,int)` is two-arg and provably cannot be the hooked symbol. The thunk over the zero-arg `installConfigFileOverride()` is the right target — the single most important row, gotten right.
- Landmine taxonomy is real and source-confirmed (`CommandParser` polymorphic but `addSubCommand` public/non-virtual; `Game::install` static; `getPlayerCreature`/`ms_loops`-private exist).
- The `/FORCE` link-grep is correctly identified as the real linker gate; "wrong `&` worse than missing → OMIT" is propagated.
- Header kept exe-local — avoids the stale-plugin ABI cascade.

### Concerns
- **HIGH — The `version` field is never bumped; the spec's add/remove semantics are silently violated.** Spec §0/§4: "`version` is bumped whenever a name is added or removed." 37-01 sets it to 1; 37-02 adds ~67 names; 37-03 adds ~160 more. No task bumps it. Utinni's consumer does `if (version != UTINNI_HOOKPOINTS_VERSION) log soft mismatch`. In lockstep it's probably benign, but the plans never reason about it, never instruct a per-wave bump, and never coordinate with the Utinni copy — producing spurious mismatch warnings or silent-by-luck behavior. A direct, checkable contract-field violation, invisible to every in-repo gate.
- **HIGH — The coverage `static_assert` proves count parity, not name/symbol correctness — and the plans conflate the two.** `static_assert(rowcount == .inc_count)` only proves two integers are equal. It cannot detect: a row whose `name` string differs from its `.inc` spelling (the table body is hand-written; the macro never generates the name string, so a typo'd `"game::instal"` passes), a row bound to the wrong `&symbol` with the right name, or two `.inc` entries collapsed into one row plus a duplicate elsewhere. The runtime `verifyNoNullNoDup` narrows it (nulls + dup names) but nothing cross-checks that the table's name set equals the `.inc` name set — the only thing that would prove "zero-missing." EPA-04 claims more than the gate delivers.
- **HIGH — Off-by-one inconsistency in the count bridge is unresolved and will fail the build or defeat the gate.** RESEARCH:437 and PATTERNS:176 write the table with a `{nullptr,nullptr}` terminator and `count = (sizeof/sizeof) - 1`; 37-01 Task 2:184-192 writes it without a terminator and `count = (sizeof/sizeof)`. Mutually inconsistent specs to the same executor. The spec even says "NUL-name terminated optional," so the terminator's presence is genuinely undecided. Pin to one canonical form before execution.
- **MEDIUM — The `union` PMF→`void*` is well-defined enough for the chosen cases but the guard doesn't guard the case that bites.** Reading a non-active union member is technically UB but a reliable MSVC idiom for same-size members (fine for single-inheritance non-virtual PMFs under v145/32-bit). The gap: `sizeof(PMF)==sizeof(void*)` guards MI/VI inflation but NOT the virtual-method-thunk problem (a `&Class::virtualFn` PMF is still pointer-sized, so the assert passes while the address is a vcall thunk). The plans know this and handle it by a manual skip rule — meaning the most dangerous PMF error class has zero automated backstop across ~230 rows.
- **MEDIUM — `pmfToVoid` of a non-static member gives Utinni a `__thiscall` entry that it hardcodes as a plain-function RVA on SWGEmu; the convention is assumed to match, never asserted.** The advertised `void*` is just an address; the calling convention is carried entirely by Utinni's typedef. For a non-static member MSVC uses `__thiscall` (this in ECX). The plans never state the per-row convention contract beyond "match the signature-source." The one explicit case (`client::wndProc` `__stdcall`) is mentioned only in passing. A convention mismatch links clean, passes coverage, and crashes live.
- **MEDIUM — `installConfigFileOverride` exposure is left to executor discretion in a way that can change behavior.** 37-01 Task 1 offers a forwarding shim vs promoting the definition. The additive forwarding shim is fine; moving the definition out of the namespace risks the qualified-caller case. Pin the forwarding-shim option on the row whose correctness is the entire EPA-02 deliverable.
- **LOW — The thunk returning `0` unconditionally is fine** (answering the posed question): Utinni detours *this thunk* as the "original," so the trampoline runs `installConfigFileOverride()` then returns 0; Utinni's typedef return is `int` and it doesn't inspect the value (fire-and-load orchestrator). There is no "real original" being shadowed. Sound.
- **LOW — "All 3 flavors export undecorated" is asserted but `_o` is optional in 37-01/02 and only mandatory in 37-03.** If `_o` first builds in 37-03 and surfaces an Optimized-only issue (pre-existing SAFESEH LNK), it lands late. Flavor coverage is back-loaded.

### Suggestions
- Add a per-wave `version` bump instruction (and re-place the `.inc`/`.h` copy into `D:/Code/Utinni`), or add one sentence reasoning that lockstep-by-shared-header makes the bump cosmetic — so the spec field isn't silently ignored.
- Close the name-correctness gap cheaply: emit a parallel `s_requiredNames[]` from the same X-macro (`#define UTINNI_HOOKPOINT(g,n) #g "::" #n`), then have `verifyNoNullNoDup` assert the table's name set equals `s_requiredNames`. Converts the count-only gate into an actual name-coverage gate. The hand-written part stays only the `&symbol`.
- Pin the terminator + count form to one canonical choice (recommend: no terminator, `count = sizeof/sizeof`, assert without `-1`); fix RESEARCH/PATTERNS to match.
- Add a per-row calling-convention note for every non-`__cdecl` row (wndProc `__stdcall`, `__thiscall` member PMFs); state the contract that the advertised address + Utinni's typedef convention must match MSVC's emitted convention.
- Pin the `installConfigFileOverride` exposure to the additive forwarding-shim option.
- Down-scope the EPA-04 "done" language from "zero-missing proven" to "count-parity + no-null + no-dup + name-set-equality proven; correct-`&` is by-construction/human-reviewed, live-verified Utinni-side."

### Risk Assessment
**Overall: MEDIUM.** The export mechanism, EPA-02 mapping, and landmine taxonomy are sound and source-verified — the genuinely hard, crash-on-first-detour parts are correct, and the phase will almost certainly build, export, and boot as planned. Risk is not LOW because three contract-soundness gaps survive every in-repo gate: the unbumped `version` field (a flat spec violation), a coverage gate that structurally cannot prove the property EPA-04 claims, and a calling-convention assumption that is load-bearing across the boundary yet asserted nowhere. All three are invisible until live Utinni injection (out of repo), so the phase can declare victory on by-construction checks while shipping a table that misbehaves at runtime. Acceptable as a boundary, but the by-construction gates are presented as stronger proof than they are. Drops to LOW with the name-set-equality check + version-bump + pinned terminator form — none requiring new infrastructure.

---

## Consensus Summary

### Agreed Strengths (2+ reviewers)
- **Spike-first sequencing (37-01 → 02 → 03)** is the right de-risk — *all four*.
- **The EPA-02 thunk correction is correct and source-verified** (zero-arg `installConfigFileOverride()` thunk, not `ConfigFile::loadFromBuffer`) — *all four*.
- **Landmine taxonomy is honest and per-row, not bulk `&fn`** — *all four*.
- **The `/FORCE` link-grep gate** is the correct mitigation for typo'd rows — *all four*.
- **Exe-local header avoids the plugin-ABI cascade** — *Cursor, Sonnet, (Codex implied)*.
- **Out-of-repo acceptance is honestly deferred** to Utinni-side — *Cursor, Sonnet, Opus*.
- **Confirmed spike symbols + the 8 MISMATCH corrections check out** against source — *all four*.

### Agreed Concerns (2+ reviewers — highest priority)

1. **[HIGH — ALL FOUR] The coverage `static_assert` is count-only; it does NOT prove zero-missing or correctness.** It cannot catch a wrong-`&` at a non-null address, a name-string typo, a virtual vtable thunk, or a collapsed/duplicated pair. EPA-04 over-claims. **Converged fix (Codex + Opus, independently):** generate `s_requiredNames[]` from the same X-macro and assert at runtime that the table's name set equals the required set; keep the count assert only as a cheap drift smoke.

2. **[HIGH — Cursor, Sonnet, Opus] The coverage formula is internally inconsistent (terminator off-by-one).** 37-01 Task 2 uses no sentinel + raw `sizeof/sizeof`; 37-PATTERNS:176 and 37-RESEARCH:433-437 use a `{nullptr,nullptr}` sentinel + `- 1`. An executor mixing docs breaks the spike or corrupts `count`. **Pin one canonical form** (consensus: no sentinel) and align PATTERNS/RESEARCH.

3. **[HIGH/MED — ALL FOUR] A virtual advertised as `&fn`/`pmfToVoid` passes every automated gate** (the `sizeof(PMF)==sizeof(void*)` guard does not exclude single-inheritance virtual PMFs — still 4 bytes — yet yields a vtable stub). Only human grep discipline catches it, across ~230 rows. Add an explicit per-row symbol-kind checklist and broaden the virtual-skip greps in 37-03.

4. **[HIGH/MED — Cursor, Codex, Opus] The PMF `union` is not standards-defined.** Use `std::bit_cast<void*>` (C++20, already enabled) or `memcpy` under the same size guard. (Opus notes it's a reliable MSVC idiom in practice — see Divergent Views.)

5. **[HIGH/MED — Cursor, Codex, Sonnet] Concrete wrong symbols in RESEARCH/37-03 that won't compile or will be mis-skipped:**
   - `Object::getType` → does not exist; non-virtual `getObjectType()` at `Object.h:152`. `Object::move` → non-virtual `move_p`/`move_o` at 251-252 (`move_o` **inline** at 1216). The "virtual at Object.h:120-121" citation is wrong (those lines are `addToWorld`/`removeFromWorld`).
   - `extent::intersect` → `Extent::intersect` does not exist; non-virtual `BaseExtent::intersect` overloads (`BaseExtent.h:45-47`); `Extent::realIntersect` is protected-virtual.
   - `commandParser::createDelegateCommands` is **protected** (`CommandParser.h:180`).

6. **[HIGH/MED — Codex, Sonnet, Opus] The `installConfigFileOverride` exposure is underspecified and risks a `using namespace ClientMainNamespace` collision/ambiguity** on the single most important (EPA-02) row. Pin one approach (additive differently-named shim, or deliberate promote-out-of-namespace + verified callers).

7. **[MEDIUM — Cursor, Opus] The `__thiscall` calling convention for member-PMF rows (and `__stdcall` for `wndProc`) is never asserted per-row** — a convention mismatch links clean and crashes at the first live detour.

### Unique high-value findings (single reviewer — worth elevating)
- **[Codex HIGH] The unconditioned `<ClCompile>` applies to the existing x64 configs**, breaching the 32-bit-only scope (could export from x64 or fail x64 builds). Add a `Platform=Win32` condition or an `#if !defined(_WIN64)` guard. *(Most actionable build-graph fix; no other reviewer caught it.)*
- **[Codex HIGH] The shared header carries the provider-side `__declspec(dllexport)` into the Utinni consumer repo.** Split the export declaration from the shared struct/name header (or macro-gate it) so Utinni doesn't import a `dllexport`.
- **[Codex HIGH] Specific wrong UI/chat symbol names:** `enableTextInput`→`acceptTextInput`, `writeToAllTabs`→`appendToAllTabs`, `writeToCurrentTab`→`appendTextToCurrentTab`, `chatEnterHandler`→`performEnterKey`, `consoleHelper::sendInput`→`processInput`.
- **[Opus HIGH] `UTINNI_HOOKPOINTS_VERSION` is never bumped** across three additive waves — a flat spec §0/§4 violation invisible to every in-repo gate.
- **[Sonnet HIGH] `game::getCamera` is NOT overloaded** (RESEARCH/37-02 are wrong — the const variant is the separately-named `getConstCamera`); the prescribed `static_cast` is spurious and could be cast to the wrong signature.
- **[Sonnet HIGH] `game::g_mainLoopCounter`'s accessor `getLoopCount` is inline** (`Game.h:398`) → address may not be ODR-emitted (Pitfall 2 applies to this exact row but isn't flagged).
- **[Sonnet MED] 37-03 authors ~160 rows across Tasks 1-2 with no build until Task 3** → `.inc`↔table drift accumulates and surfaces late. Add an intermediate build+link-grep checkpoint.
- **[Sonnet MED] The `.inc`→`D:/Code/Utinni` dual-placement is declared but never tasked** → the two repos can silently diverge.

### Divergent Views (worth investigating)
- **Overall risk:** three reviewers say **MEDIUM** (Cursor, Sonnet, Opus); Codex says **MEDIUM-HIGH**, driven by the symbol errors plus the x64-leak. The x64-scope and shared-header-dllexport findings are Codex-only and tip its rating — worth weighing since no one refuted them.
- **PMF `union` severity:** Cursor and Codex call it not-well-defined and want `std::bit_cast`; Opus agrees `bit_cast` is safer but characterizes the union as a "reliable MSVC idiom, fine in practice." Agreement on the fix, divergence on alarm level.
- **Skipped-virtuals vs the shared `.inc` (Cursor):** excluding virtuals from the `.inc` required-set means EPA-04 "zero-missing" is measured against a *trimmed* set, not Utinni's full ~230-name hook list. Whether that's a coordination gap or intended depends on Utinni already resolving those off the live vtable — **needs explicit Utinni-side confirmation**, which no in-repo artifact provides.

### Top remediations before execution (do these first)
1. **Pin the terminator/count form** (no sentinel) and align 37-01 / PATTERNS / RESEARCH — prevents an immediate spike build-break. *(consensus #2)*
2. **Upgrade the coverage gate to name-set equality** via an X-macro-generated `s_requiredNames[]` — closes the count≠correctness gap. *(consensus #1)*
3. **Guard `utinni_advertise.cpp` to Win32-only** (vcxproj condition or `#if !_WIN64`) — honors 32-bit-only scope. *(Codex unique HIGH)*
4. **Pin the `installConfigFileOverride` exposure** to one collision-free approach. *(consensus #6)*
5. **Fix the wrong symbols** (`getObjectType`/`move_*`/`BaseExtent::intersect`/`createDelegateCommands` protected/UI-chat names) and remove the false "virtual" citations in RESEARCH/37-03. *(consensus #5 + Codex unique)*
6. **Replace the PMF union with `std::bit_cast`** and add the per-row symbol-kind checklist + broadened virtual greps. *(consensus #3, #4)*
7. **Add the `version` bump + `.inc`→Utinni sync as explicit tasks**, and down-scope the EPA-04 "done" language to what the gates actually prove. *(Opus + Sonnet unique)*

**To incorporate feedback into planning:** `/gsd-plan-phase 37 --reviews`
