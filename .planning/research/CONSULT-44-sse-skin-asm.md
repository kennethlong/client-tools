# CONSULT-44 — SSE skinning __asm → _mm_* port: resolve the transform-stride contradiction

Treat the following as GIVEN (do not re-derive). I need ONE thing: confirm or refute the
transform memory layout the asm assumes, so a `_mm_*` intrinsic port is lane-faithful.

## File
`src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp`
Two `__asm` blocks: `_fillVertexBufferHard_sse` (~line 2044) and `_fillDot3VertexBufferHard_sse` (~1739).
They are SSE peers of the SCALAR `fill_vb_work::fillVertexBufferHard()` (~2278) /
`fillDot3VertexBufferHard()` (~2240). Both run on the SAME `transformArray`
(`const PoseModelTransform *`), selected only by `s_useSSE` at the dispatch site (~1568/1591).
`PoseModelTransform` is a typedef of `Transform`.

## GIVEN facts
1. `Transform::matrix` is `typedef real matrix_t[3][4]` = `float[3][4]` = 48 bytes, ROW-major.
   `Transform` has NO `align(16)`, so `sizeof(Transform) == 48`.
2. Scalar `Transform::rotate_l2p(v).x = m[0][0]*x + m[0][1]*y + m[0][2]*z` (dot of ROW 0).
   `.y`=row1, `.z`=row2. `rotateTranslate_l2p` adds `m[r][3]`.
3. The asm loads 4 aligned 16-byte blocks at `[eax+0/16/32/48]`, calls them "column 0..3",
   broadcasts source x/y/z, does `col0*x + col1*y + col2*z + col3`, then stores lanes 0/1/2.
   So asm result.x = `f0*x + f4*y + f8*z + f12` where f0=float[0], f4=float[4], etc.
   In row-major terms asm result.x = `m[0][0]*x + m[1][0]*y + m[2][0]*z + float[12]`.
4. The asm computes the transform index offset as `transformIndex << 6` = ×64 bytes.
   The scalar path uses `transformArray + transformIndex` = ×`sizeof(Transform)` = ×48 bytes.
5. The `[eax+48]` (col3) read goes 16 bytes past the 48-byte matrix.

## The contradiction to resolve
- asm result (col-dot, ×64 stride, reads 64 bytes/transform) vs scalar (row-dot, ×48 stride).
  These give DIFFERENT numeric results AND different element strides on the SAME array — yet
  both paths must produce identical skinning on the same `transformArray`.

## Questions (answer each, cite file:line)
Q1. Is `sizeof(PoseModelTransform)` actually 48, or is the transform passed to skinData a
    DIFFERENT, pre-transposed / 64-byte-stride layout built somewhere before dispatch? Trace
    `getBindPoseModelToRootTransforms()` / how `transformArray` reaching skinData is built.
Q2. Given the row-major matrix, what is the asm ACTUALLY computing per output lane — is it the
    transpose of `rotate_l2p` (i.e. the array is transposed), or does it coincidentally equal
    `rotate_l2p` under some property?
Q3. Is the SSE path even reachable at runtime today (is `s_useSSE` ever true; is
    `m_skinningMode==SM_hardSkinning` the only caller), or is this dead on modern builds?
Q4. For a faithful `_mm_loadu_ps`-based port: should I (a) replicate the asm's col-dot +
    64-byte stride EXACTLY (bug-for-bug), or (b) match the SCALAR row-dot semantics (the
    visually-correct reference)? Which is the ground truth the renderer depends on?
