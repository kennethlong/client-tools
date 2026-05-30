---
status: complete
phase: 17-psrc-census-char-select-beachhead
source: [17-VERIFICATION.md, ROADMAP.md success criteria CHAR-01/02/03]
started: 2026-05-30
updated: 2026-05-30
---

## Current Test

[testing complete]

## Tests

### 1. Skin & clothing diffuse (CHAR-01)
expected: On char-select under D3D11, skin & clothing render with correct diffuse textures, matching D3D9 — not untextured, flat-white, or magenta.
result: pass

### 2. Eyes (CHAR-02)
expected: Character eyes render with correct customization-palette color, seated in the face, not gray, and not visible through the back of the head.
result: pass

### 3. Head/face multi-stage composite (CHAR-03)
expected: Head/face multi-stage materials (sul_*_head.sht, sul_eye.sht) composite their texture stages correctly under D3D11, matching D3D9.
result: pass

## Summary

total: 3
passed: 3
issues: 0
pending: 0
skipped: 0

## Gaps

[none — all tests passed; matched A/B evidence + per-lane attribution in 17-VERIFICATION.md]
