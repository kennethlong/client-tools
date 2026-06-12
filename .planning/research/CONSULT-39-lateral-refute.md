# CONSULT-39 — Refute "it's apply-skip": what ELSE zeros the b0 dot3 block? (LATERAL / skeptic)

Read the actual source in `D:\Code\swg-client-v2` AND drive the RenderDoc MCP. Cite file:line +
RenderDoc event IDs. You are the skeptic. The LOCKED facts are measured truth; everything else,
including the prevailing "apply is skipped" hypothesis, is fair game to REFUTE.

## LOCKED GROUND TRUTH (axioms)
D3D11 bump/dot3 BUILDING meshes render PURE BLACK, deterministic by camera angle. Correct in D3D9.
- Bump PS reads b0 c1/c3/c4 (PSCR_dot3*). BLACK draw: b0 c1/c3=0. LIT draw: b0 c1/c3=sun. Same
  shader black+lit in the SAME frame (Capture30: 4475 black, 6911 lit; same PS family).
- dot3 snapshot `s_pixelDot3State` ALWAYS valid+good sun (probe 0/60000 d3valid=0); persistence
  fix in & working. NOT the cause.
- `StaticShaderData::apply` writes b0 c0..c4 from snapshot `if(d3.valid)`, then `updatePS(0,...)`
  unconditionally, then writeShadowBack. b0 = ONE shared per-slot buffer, Map(WRITE_DISCARD).
- D3D9 is the reference.

## Captures (renderdoc-mcp)
- `D:\Code\swg-client-v2\stage\Capture30.rdc` — black draw eventId **4475** (VS10820/PS10819,
  px 1450,175), lit draw **6911** (px 1545,214), wall meshes **4597/4621** (2376 idx, VS10820/
  PS10819), scene RT `ResourceId::323`.
- `Capture28.rdc` all-black wall (4469); `Capture29.rdc` lit 3° later (4424). Same PS hash.

## THE QUESTION
Same shader, same frame: why do some bump draws read b0 c1/c3=sun (lit) and others 0 (black)?

## Your task (LATERAL — only this). Try to REFUTE the prevailing "the black draw skipped its
Producer-B apply / read a stale-zero b0" hypothesis, and surface alternatives the tracers will
miss. Rank by evidence (source + RenderDoc).

1. **Is the black surface even the dot3 shader's output, or is a SEPARATE black draw on top?**
   Use pixel_history on (1450,175) in Capture30 — is draw 4475 the winning fragment, or does a
   later draw overwrite? Could the "black mesh" be a depth-prepass, a shadow/alpha pass, a decal,
   or a collision/occluder draw that writes black?
2. **Two-pass / instance hypothesis.** Capture30 has TWO 2376-idx same-shader draws (4597,4621).
   Are they two passes of one mesh (one lit, one black overlay) or two separate meshes? Inspect
   both: same VB? same b0? Is one additive/blended over the other?
3. **Per-object vs per-light b0.** c0 (dot3 light DIRECTION) is recomputed per draw in object
   space (worldDirectionToObjectLocal). Could the black draws have a VALID c1/c3 but the dot3
   geometry (v4 tangent-space light dir, or the normalMap) produce zero? Re-check the black PS
   trace: is it c1/c3 that are zero, or is it the dot3 modulation? (Locked fact says c1/c3=0, but
   verify the base term `v1+c1+c3`, not just the modulated result.)
4. **Is `d3.valid` REALLY true for the black draw's frame?** The [P38B/C] probe logs CAPPED at
   ~15:16 and Capture30 is 15:17 — the logs do NOT cover the capture frame. The persistence fix
   makes valid sticky, but VERIFY there is no OTHER reset of `s_pixelDot3State` (install/lostDevice/
   beginScene/per-frame clear) anywhere, and that `getPixelDot3State()` returns the same object
   the writers update. Grep exhaustively.
5. **MaxLOD / shader-tier / vertex-color.** Could the black draws be a LOD that uses a different
   VB (no color/tangent) feeding garbage, or a fallback that binds the asset PS but a zero b0?
6. The single cheapest discriminating probe (RenderDoc op or one log line) that would KILL the
   apply-skip hypothesis if it's wrong.

WRITE findings as markdown to
`D:\Code\swg-client-v2\.planning\research\CONSULT-39-lateral-refute-sonnet.out` (Write tool):
one-line bottom-line, ranked hypotheses with for/against (source file:line + RenderDoc eventIds),
explicit pick, and the discriminating probe. Return a 2–3 sentence summary. Read-only on source.
