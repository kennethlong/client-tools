# CODEX Consult — Plan 11-09 Iter-1 Friend-Additions Set

**Date:** 2026-05-20
**Context:** Phase 11 / Plan 11-09 Iter-1 (minimum-VS-bind iteration). CODEX previously reviewed this plan and said *"Friend additions: 2 lines on ShaderImplementation.h matches precedent."* Want to verify that's the FULL friend set, or whether additional friend declarations in other engine headers are also needed.

---

## What I'm building

`Direct3d11_StaticShaderData::construct(StaticShader const &shader)` needs to walk `ShaderImplementation::m_pass[]` to extract per-pass `Direct3d11_VertexShaderData*` + `Direct3d11_PixelShaderProgramData*` from the public `ShaderImplementationPass::m_vertexShader` + `m_pixelShader` members (both public, confirmed at `ShaderImplementation.h:263, 265`).

To reach `ShaderImplementation::m_pass[]`, I need a `ShaderImplementation const *` pointer. The input is `StaticShader const &shader`. The traversal chain is:

```
shader.getStaticShaderTemplate()                  // public, StaticShader.h:56
    .m_effect                                     // PRIVATE on StaticShaderTemplate
    ->getActiveShaderImplementation()             // public, ShaderEffect.h:40
    ->m_pass                                      // PRIVATE on ShaderImplementation
```

(D3D9's canonical version at `Direct3d9_StaticShaderData.cpp:988-992` reaches `m_implementation` directly via `effect->m_implementation`, but `ShaderEffect::getActiveShaderImplementation()` is a public getter for the same field, so the friend on `ShaderEffect` can be skipped if we use the getter.)

## Verified evidence

| File | Line | Evidence |
|---|---|---|
| `clientGraphics/src/shared/StaticShader.h` | 32-35 | Existing friends: `Direct3d8`, `Direct3d8_StaticShaderData`, `Direct3d9`, `StaticShaderTemplate`. **No `Direct3d9_StaticShaderData` friend.** Public getter `getStaticShaderTemplate()` at line 56. |
| `clientGraphics/src/shared/StaticShaderTemplate.h` | 28 | `friend class Direct3d9_StaticShaderData;` — needed because `m_effect` is private (line 144). No public getter for `m_effect`. |
| `clientGraphics/src/shared/ShaderEffect.h` | 24-25 | `friend class Direct3d8_StaticShaderData; friend class Direct3d9_StaticShaderData;` — needed because `m_implementation` is private (line 63). **Public getter `getActiveShaderImplementation()` at line 40 returns `const ShaderImplementation *`.** |
| `clientGraphics/src/shared/ShaderImplementation.h` | 41-44 | `friend class Direct3d8_ShaderImplementationData; friend class Direct3d8_StaticShaderData; friend class Direct3d9_ShaderImplementationData; friend class Direct3d9_StaticShaderData;` — needed because `m_pass` is private (line 129). |
| `clientGraphics/src/shared/ShaderImplementation.h` | 263, 265 | `ShaderImplementationPass::m_vertexShader` + `m_pixelShader` are PUBLIC (the pass class itself uses `//lint -esym(1925)` to suppress the public-member warning — intentional public). |
| `Direct3d9/src/win32/Direct3d9_StaticShaderData.cpp` | 988-992 | D3D9 ctor: `m_implementation(shader.getStaticShaderTemplate().m_effect->m_implementation)` — touches all three private fields (`m_effect`, `m_implementation`, eventually `m_pass` in `construct()`). |

## Plan 11-09 Iter-1 PLAN.md says

> "**Engine-side friend declarations** -- `ShaderImplementation.h:41-44` add 2 lines mirroring D3D8/D3D9 pattern:
> ```cpp
> friend class Direct3d11_ShaderImplementationData;
> friend class Direct3d11_StaticShaderData;
> ```
> Rule-3 deviation per CONTEXT D-02. Precedent: Plan 11-04 Texture.h friend, Plan 11-06 HardwareVB/IB friends. Zero behavioral impact on D3D8/D3D9 plugins (their existing friends preserved)."

**Only `ShaderImplementation.h` is named.** No mention of `StaticShaderTemplate.h` or `ShaderEffect.h` friend additions. The prior CODEX consult artifact summarized this as *"2 lines on ShaderImplementation.h matches precedent."*

## The discrepancy

If I add only 2 friends to `ShaderImplementation.h`, `Direct3d11_StaticShaderData::construct()` still cannot reach `StaticShaderTemplate::m_effect` (private, no public getter). The traversal fails at the first hop. To reach `m_pass` at all, I need ≥1 additional friend.

## Hypothesis space

**Hypothesis A — PLAN is correct AND I'm missing a public path.**
There exists a `StaticShader` → `ShaderImplementation` route that doesn't touch `m_effect`. Candidates I considered and rejected:
- `StaticShader::getShaderImplementation()` — does not exist.
- `StaticShader::getShaderEffect()` — does not exist.
- Some `Graphics` / `ShaderTemplate` factory backpointer — `Direct3d11_ShaderImplementationData` is built by `Direct3d11_ShaderImplementationData::create` from a `ShaderImplementation` reference, so it does have an owning backpointer to a `ShaderImplementation`; but reaching THAT backpointer from a `StaticShader` requires the same `m_effect`/`m_implementation` traversal.

If Hypothesis A is right, please point me at the public-API route I missed.

**Hypothesis B — PLAN is incomplete (off-by-2 on the friend count).**
The actual minimum friend set is 3 lines across 2 engine headers:
- `StaticShaderTemplate.h` → `friend class Direct3d11_StaticShaderData;` (1 line, for `m_effect` access)
- `ShaderImplementation.h` → `friend class Direct3d11_ShaderImplementationData; friend class Direct3d11_StaticShaderData;` (2 lines, for `m_pass` access; PLAN already names these)
- `ShaderEffect.h` → untouched (use public `getActiveShaderImplementation()`)

If Hypothesis B is right, the PLAN should be updated and the iteration log records this as a PLAN-vs-reality delta.

**Hypothesis C — Mirror D3D9 verbatim (4 lines across 3 headers).**
Same as B, plus `friend class Direct3d11_StaticShaderData;` in `ShaderEffect.h:24-25` block. Use `effect->m_implementation` (private) like D3D9 does instead of `effect->getActiveShaderImplementation()` (public). Trades 1 extra friend declaration for line-for-line D3D9 parity.

## Verifiable test

The answer changes which `.h` files Iter-1 touches:
- A → 1 header touched (`ShaderImplementation.h`)
- B → 2 headers touched (`ShaderImplementation.h` + `StaticShaderTemplate.h`)
- C → 3 headers touched (the two above + `ShaderEffect.h`)

The implementation difference is mechanical (1-3 extra lines), but the engine-side surface change matters for D-05 invariant (D3D9 plugin still builds clean — adding friends in engine headers requires recompiling D3D8/D3D9 too, but no behavioral change).

The visible-runtime test is identical across all three hypotheses: post-Iter-1 smoke shows `CreateInputLayout` ≥1, IASet/Draw activity, `ms_skippedDrawsNullVS` stops climbing. The compile-time test differs by which Hypothesis I follow.

## My current bias

Hypothesis B feels right (matches code reality + uses the cleaner public path), but I'd like CODEX confirmation before editing engine headers — adding friends is a Rule-3 deviation and I want the exact line set locked before the commit lands.

## Question to CODEX

> Confirm which Hypothesis (A / B / C) is correct, or surface an option I missed. If B or C, the PLAN's "2 lines on ShaderImplementation.h" guidance should be updated. If A, point at the StaticShader → ShaderImplementation public route I missed.

---

**Resume:** After CODEX answers, I'll record the verdict here under a `## CODEX response` section + execute the resulting friend-additions set, then continue with Iter-1 Tasks 2-7.

---

## CODEX response (2026-05-20)

**Verdict: Hypothesis A. PLAN is correct as written. The public traversal route I missed:**

```cpp
const ShaderImplementation *implementation =
    shader.getStaticShaderTemplate()
        .getShaderEffect()
        .getActiveShaderImplementation();
```

**Evidence (provided by CODEX, verified against codebase):**

| File | Line | Symbol | Visibility |
|---|---|---|---|
| `StaticShader.h` | 56 | `StaticShader::getStaticShaderTemplate()` | public |
| `StaticShaderTemplate.h` | 103 | `StaticShaderTemplate::getShaderEffect() const` | **public** (returns `const ShaderEffect &`; impl at StaticShaderTemplate.cpp:189 is `return *m_effect;`) |
| `ShaderEffect.h` | 40 | `ShaderEffect::getActiveShaderImplementation() const` | public (returns `const ShaderImplementation *`) |
| `ShaderImplementation.h` | 129 | `ShaderImplementation::m_pass` | **private** — friend required here |

**Friend additions confirmed at 2 lines, 1 header:**
```cpp
// ShaderImplementation.h:41-44 (insert after Direct3d9_StaticShaderData friend)
friend class Direct3d11_ShaderImplementationData;
friend class Direct3d11_StaticShaderData;
```

**Do NOT add `Direct3d11_StaticShaderData` to `StaticShaderTemplate.h` or `ShaderEffect.h`** — the public-API traversal makes those friends unnecessary for Iter-1.

**Self-attributed miss:** the original analysis grep on `StaticShaderTemplate.h` did return `103: const ShaderEffect &getShaderEffect() const;`, but the analyzer fixated on `m_effect` being private and didn't connect the public getter as the escape hatch. CODEX's direct citation of `StaticShaderTemplate.cpp:189` (the `return *m_effect;` impl) is the proof that the public getter dereferences through the private field, removing the need for any friend on `StaticShaderTemplate`.

**Iter-1 friend-additions set is now LOCKED at PLAN's original guidance: 2 lines on `ShaderImplementation.h:41-44`.**

Proceeding to Tasks #2-#7.

---

## CODEX response — post-implementation correction (2026-05-20)

**CODEX's Hypothesis A was correct in principle but wrong on plugin DLL linkage.** First MSBuild attempt revealed the constraint:

```
error LNK2019: unresolved external symbol StaticShaderTemplate::getShaderEffect() const
error LNK2019: unresolved external symbol ShaderEffect::getActiveShaderImplementation() const
```

**Root cause:** plugin DLLs (`gl05_d.dll` / `gl11_d.dll`) cannot call non-inline non-DLLEXPORT member functions of clientGraphics classes. Only inline functions (defined in headers), fields-via-friend, and `Gl_api` slot function pointers cross the plugin/engine boundary. D3D9 plugin doesn't hit this because it uses **direct field access** (`m_effect->m_implementation`) via friend, not the public getters.

The `StaticShaderTemplate::getShaderEffect()` and `ShaderEffect::getActiveShaderImplementation()` functions exist (defined in their respective .cpp files) but their symbols are only linked into binaries that include those .obj files at link time — i.e. `SwgClient.exe` directly, not the plugin DLLs.

**Decision:** abandon Hypothesis A; mirror D3D9 verbatim via direct field access. **Hypothesis C is actually correct.**

**Final Iter-1 friend set (5 lines across 4 engine headers):**

| File | Line | Friend added | Purpose |
|---|---|---|---|
| `StaticShader.h` | 35 | `friend class Direct3d11_StateCache;` | `m_graphicsData` cast in `setStaticShader` |
| `StaticShaderTemplate.h` | 29 | `friend class Direct3d11_StaticShaderData;` | `m_effect` access in ctor |
| `ShaderEffect.h` | 26 | `friend class Direct3d11_StaticShaderData;` | `m_implementation` access in ctor |
| `ShaderImplementation.h` | 45-46 | `friend class Direct3d11_ShaderImplementationData;` + `friend class Direct3d11_StaticShaderData;` | `m_pass` access in construct |

Mirrors D3D9 exactly (D3D9 friends in same 4 places, just different friended class names per-plugin).

**Lesson for future plugin work:** the plugin/engine ABI is field-access-via-friend OR Gl_api slots. Public non-inline member functions of engine classes are NOT callable from plugin DLLs. Future CODEX consults should validate "public API path" recommendations against the plugin linkage constraint before endorsing them.

**PLAN.md was incomplete on the friend-additions count.** Recommend updating Plan 11-09 Iter-1 §Action item 1 to enumerate all 5 friend lines / 4 headers explicitly, with the plugin-linkage rationale.

Build outcome: MSBuild EXIT=0 across all 5 targets (Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient) after attempt 4. Full iteration log: `11-09-iteration-log.md` § Iter-1.
