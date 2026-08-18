You are analyzing the repository at D:\Code\SWG-Toolkit (an Electron + TypeScript + C++
N-API modding toolkit). Read-only task. Report with file:line citations for every claim;
prefer exhaustive tables over prose.

CONTEXT (treat as given, do not re-derive): the C++ TRE modules live in
packages/native-core/modules/core/tre/ (TreVersion.h, TreArchive.cpp, TreBuilder.cpp,
TreMount.cpp) with N-API bindings in packages/native-core/src/tre_binding.cpp and a TS
surface in packages/native-core/index.d.ts.

TASK: Map the toolkit's TRE version taxonomy end-to-end — every behavior keyed off the
TreVersion enum, and the full blast radius of ANY future change to that taxonomy.

Deliverables:

1. A table: for each TreVersion enum arm (V0004, V0005, V0006, V5000, V6000) —
   (a) exact on-disk byte sequence at file offsets 4..7 that parses to this arm on READ
   (cite parseVersionString and any other parse site);
   (b) exact bytes WRITTEN at offsets 0..7 when TreBuilder emits this arm (cite the
   header-write code);
   (c) every behavioral difference keyed off the arm: record stride, isEnumerateOnly,
   codec selection (TreCodec / codecForTreVersion — what does each arm dispatch to,
   including any encryption/decryption paths), refusals (build/repack throws), and
   anything else you find.
2. Every CALLER of buildTre/repackTre and the C++ TreBuilder::build/repack — which
   version argument do they pass in practice (default V0005? explicit?), across C++,
   bindings, TS services, and UI code.
3. Every place a version string reaches the UI or persisted state (e.g. "v0005"/"v5000"
   labels in TreMountArchiveInfo, project files, deploy metadata) — what would a user
   currently see for a real retail archive?
4. The test inventory: every test that pins version-related behavior (parse, stride,
   enumerate-only, builder round-trip, codec). For each: what byte sequences does the
   fixture actually contain at offsets 0..7 (read the fixture files or the fixture
   generation code), and does any test ever validate a TreBuilder-produced archive
   against anything OTHER than the toolkit's own parser (an external reader, a real
   client, a real retail archive byte-diff)?
5. Anywhere the docs/ tree or code comments make claims about version semantics
   ("legacy format", "Restoration", "encrypted", "enumerate-only", stride 32) — list
   each claim with its location.

Do NOT propose fixes and do NOT judge which taxonomy is correct — this is an inventory
task. If two code sites disagree with each other, flag the disagreement neutrally.
