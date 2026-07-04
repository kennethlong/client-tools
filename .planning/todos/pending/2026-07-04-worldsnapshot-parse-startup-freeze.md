---
created: 2026-07-04
title: WorldSnapshot::load parse is the remaining zone-in freeze (~2.5s) + string-table pump chunking
area: client load path
priority: medium (the dominant residual startup hitch after the CONSULT-59 wave)
references:
  - .planning/research/CONSULT-59-loadstall-fixdesign-SYNTHESIS.md
  - stage/stall-loop16982-s*.mdmp (2026-07-04 9:29 verify boot — KEEP until fixed)
---

After the CONSULT-59 wave (budgeted terrain preload + async-loader time budget +
string-table prefetch + PSRC-dump removal), the verify boot's zone-in produced ONE
remaining big stall instead of the old 3-4.5s mega-stall + burst:

- **loop 16982, 3146ms total (incl ~0.6s dump-write)**: both samples in
  `GroundScene::postload -> WorldSnapshot::load -> WorldSnapshotReaderWriter::load`
  (synchronous world-snapshot IFF parse INSIDE the GroundScene ctor, still before the
  loading screen is enabled at GroundScene.cpp:815). This cost was always there — the
  AM dumps sampled the terrain preload because it ran FIRST in the same frame and ate
  the sample points. Now it IS the charselect-freeze + startup-music-glitch residual.
- **loops 16997 (626ms) + 17063 (938ms)**: our own `preloadSomeLocalizedNameTables`
  pump paying for one BIG .stf each (budget can't preempt mid-table; cost is dominated
  by the blocking FileStreamer read). Under the loading screen by design (replaces the
  mid-combat 620ms stall) but still starves the audio pump enough for a brief skip.

Fix directions (design pass recommended — same shape as the CONSULT-59 lever A consult):
1. Move `WorldSnapshot::load` out of the ctor frame — e.g. kick the IFF parse to a
   worker thread in postload and gate `WorldSnapshot::donePreloading()` /
   `isFinishedLoading` on parse-complete (data is not consumed until the pumped
   loading phase), or chunk the node parse across loading frames.
2. String-table pump: pre-warm file BYTES (TreeFile cache) before the parse, or accept
   (loading-screen only). Optional: raise music pre-mix buffer during loading
   (Audio::setNormalPreMixBuffer is already toggled at _onFinishedLoading — check the
   loading-phase buffer size actually covers ~1s).

Kenny's felt verdict on the verify boot: "pretty smooth, music still glitches a little
during startup" — matches the instrumentation exactly.
