---
phase: 17
slug: psrc-census-char-select-beachhead
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-30
---

# Phase 17 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Phase 17 is a **local single-player D3D11 graphics-plugin** beachhead — no network,
> auth, or IPC surface. "Inputs" are engine-produced: IFF chunk data from the game's own
> `.tre` archives and D3DCompile-produced reflection metadata. ASVS L1 web categories
> largely do not map; threats are renderer-internal (memory-safety, diagnostics integrity,
> and D3D9-reference-path availability).

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| IFF asset chunk → engine | PSRC/PEXE shader chunks read from the game's own `.tre` archives in `load_0000` | Shader source/bytecode (developer-authored game data, not user input) |
| D3DCompile/D3DReflect → plugin | Reflection metadata consumed at compile + cbuffer-write time | Struct layouts, register offsets (engine-produced) |
| D3D11 plugin ↔ D3D9 reference path | Shared `clientGraphics` code feeds both renderers; D3D9 must never regress | Render state (no trust crossing; availability invariant) |
| (none) external | No network / auth / file-upload / IPC surface introduced this phase | — |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-17-01 | Tampering/DoS | PSRC chunk read (ShaderImplementation.cpp) | mitigate | `getChunkLengthLeft` (no trusted size, :2924) + 1 MiB cap (:2931-2939) + null-terminated `read_string()` (:2954) + free in dtor (:2788-2791) AND reload re-null after delete[] (:2840-2847) | closed |
| T-17-02 | Diagnostics integrity | census output / stale state | accept | Read-only diagnostic behind default-off `KEY_BOOL(psrcCensus,false)` (ConfigClientGraphics.cpp:128); stage/ dev artifact | closed |
| T-17-03 | D3D9 availability | shared load_0000 edit | mitigate | TAG_PEXE block unchanged (:2961-2965); no D3D9 src/include commits; D-06 dual-renderer boot VERIFIED 2026-05-30 (rasterMajor=5 clean) | closed |
| T-17-04 | DoS | PSRC compile in ctor | mitigate | `tryCompilePixelShaderFromHlslNoFatal` false on FAILED hr (:353-385), non-FATAL; null m_d3dPS → magenta tombstone (:1503-1506) | closed |
| T-17-05 | Diagnostics integrity | stale .cso cache | mitigate | `D3D11_REWRITE_VERSION`="24" (:304) + FNV-1a key over compile input (ShaderCache.cpp:46-47) + strlen sourceLen (:1285) | closed |
| T-17-06 | D3D9 availability | m_exe / TAG_PEXE | accept | m_exe never to CreatePixelShader (DXBC/D3D9-asm paths leave m_d3dPS null, :1205-1226); D3D11-plugin-only edit | closed |
| T-17-07 | DoS | reflection consumption at apply() | mitigate | apply() iterates CACHED `layouts` (:807); only D3DReflect is one-shot dump gated by s_dumped* (:1163); zero-size layout skips before staging alloc (:809-810) | closed |
| T-17-08 | Tampering (memory) | PerMaterialCB write via updatePS | mitigate | slot+size range-check (ConstantBuffer.cpp:177-179) + Map(WRITE_DISCARD) (:185) + per-memcpy `StartOffset+sizeof<=TotalSize` (:927-928) + full-cbuffer write (:858) + 16-byte `static_assert` (ConstantBuffer.h:66) | closed |
| T-17-09 | D3D9 availability | StaticShaderData / ConstantBuffer / Direct3d11.cpp | accept | Plugin-local, no shared-code edit; D3D9 byte-for-byte | closed |
| T-17-10 | Memory lifetime/DoS | retained DXBC blob m_psBytecodeBlob | mitigate | `ComPtr<ID3DBlob>` auto-release on dtor (:321,:391); cache load/store prevents per-reload growth | closed |
| T-17-11 | State integrity | shared s_perMaterialShadow | mitigate | Both paths RMW via `getPerMaterialShadow()` (:786,:1256-1258); full-struct flush (:1271); per-pass zero-fill on !m_materialValid (:793-795,:858); single-threaded | closed |
| T-17-05-01 | Audit-trail integrity | 17-VERIFICATION.md replacement | accept | Predecessor sha `a20e828d8` cited in footer; audit trail preserved | closed |
| T-17-05-02 | D3D9 availability | stage/client_d.cfg rasterMajor flips | mitigate | Dual rasterMajor=5 re-boot VERIFIED 2026-05-30; no D3D9 source edits | closed |
| T-17-05-03 | Frame consistency | screenshot framing drift | mitigate | evidence/README.md capture contract pins slot/camera/UI/res/gamma; honored, matched pair captured | closed |
| T-17-05-04 | security_enforcement framing | renderer-internal | accept | ASVS L1 does not map; local single-player | closed |
| T-17-06a-01 | DoS/log-flood | inner-walk recursion | mitigate | One-shot per-anchor dump gate `s_dumpedHeadVars/EyeVars` (:1082-1099); depth-2 cap (:1208-1226) | closed |
| T-17-06a-02 | OOB read | corrupted reflection type descriptor | mitigate | Null-checked `GetMemberTypeByIndex/Name` (:1197,:1215) | closed |
| T-17-06a-03 | Integrity | reflected metadata input | mitigate | HRESULT-checked `GetDesc` bail (:1171,:1178,:1199,:1217); engine-produced metadata only | closed |
| T-17-06a-04 | D3D9 availability | (plugin-local) | accept | No shared/D3D9/header touch | closed |
| T-17-06a-05 | security_enforcement framing | renderer-internal | accept | ASVS L1 does not map; local single-player | closed |
| T-17-06b-01 | Tampering/integrity | writeVarFloat4AtOffset OOB write | mitigate | Bounds check FIRST stmt: `absoluteOffset+sizeof(XMFLOAT4)>layout.TotalSize → return false` (:963-964) | closed |
| T-17-06b-02 | DoS/wrong-data | misidentified offset | mitigate | Single-candidate-per-channel; helper left unwired (Case-C deferred, no speculative write, :968); char-select uses textureFactor/COLOR0 not cbuffer material | closed |
| T-17-06b-03 | D3D9 availability | (plugin-local) | accept | Plugin-local; D-06 covered by 17-05 re-boot | closed |
| T-17-06b-04 | security_enforcement framing | renderer-internal | accept | ASVS L1 does not map; local single-player | closed |
| T-17-07-01 | Tampering/integrity | PS-main param-list rewrite | mitigate | `psrcText.size()>64KiB` cap (:816) + `rawParams.size()>16` cap (:842) + parser-failure-returns-input-unchanged (:814) + non-fatal compile → fallback | closed |
| T-17-07-02 | DoS/wrong-output | mis-rewritten PS input list | mitigate | non-fatal compile→cached-null→fallback; `isCompatibleWithVS_withExplicitPSInputs` gate (StateCache.cpp:1279,:1286-1295); per-pair attribution log (:1304); VS-output-sig-hash cache salt (:314) | closed |
| T-17-07-03 | D3D9 availability | Direct3d11-namespaced edits | accept | D3D9 byte-for-byte; ABI cascade mitigated by all-plugin rebuild discipline | closed |
| T-17-07-04 | security_enforcement framing | renderer-internal | accept | ASVS L1 does not map; local single-player | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-17-01 | T-17-02, T-17-05-01 | Read-only developer diagnostics / doc audit trail — no runtime trust surface | Kenny Long | 2026-05-30 |
| AR-17-02 | T-17-06, T-17-09, T-17-06a-04, T-17-06b-03, T-17-07-03 | D3D9 reference path structurally unregressed — Direct3d11-plugin-local edits only; verified byte-for-byte + dual-renderer boot | Kenny Long | 2026-05-30 |
| AR-17-03 | T-17-05-04, T-17-06a-05, T-17-06b-04, T-17-07-04 | ASVS L1 web categories do not map to a local single-player renderer with no network/auth/IPC surface | Kenny Long | 2026-05-30 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-30 | 28 | 28 | 0 | gsd-security-auditor (verify-mitigations mode; register authored at plan time) |

Auditor adversarially checked the GAP-4 b0 dot3 light-constant memcpys (StaticShaderData.cpp:905-909): written by fixed register offset without an inline per-write bound, but guarded by `isLegacyB0` requiring `layout.TotalSize == kShadowBytes` (:855-857), so the staging buffer is provably sized to the legacy b0 layout the offsets target. Not a gap. All SUMMARY `## Threat Flags` report "None — D3D11 plugin internals only." No unregistered attack surface.

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-30
