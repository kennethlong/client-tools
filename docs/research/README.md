# whitengold — Research Deep-Dives

Detailed investigations into specific aspects of the SWG (Star Wars Galaxies)
source archive at `C:\Code\whitengold`. Each document below is a long-form
analysis with file paths, line numbers, code snippets, search evidence, and
verdicts.

The HTML versions of these documents (with mermaid diagrams) live one level up
in `docs/*.html`.

## Index

| File | Question answered |
| --- | --- |
| [swgclient-build.md](swgclient-build.md) | What does `SwgClient.vcproj` build, what does it depend on, what's missing? |
| [animation-system.md](animation-system.md) | Is the animation system Granny middleware or in-house? |
| [foliage-system.md](foliage-system.md) | Is there a SpeedTree dependency, or is foliage in-house? |
| [runtime-middleware.md](runtime-middleware.md) | Inventory of all runtime middleware: link mode, surface area, replaceability. |
| [../recon/05-client-boot-sequence.md](../recon/05-client-boot-sequence.md) | 17-phase boot from `WinMain()` to first frame; MVB milestones and SOE-stub map. |
| [code-conventions.md](code-conventions.md) | Extended conventions: logging macros, function/module design patterns, compiler flags. |
| [architecture-layers.md](architecture-layers.md) | Engine/library layer details, data flow, key abstractions, entry points, anti-patterns. |
| [blender-asset-agent-foundations.md](blender-asset-agent-foundations.md) | IFF/TRE formats, mesh/shader/animation pipelines, and foundational requirements for a Blender MCP asset agent. |
| [blender-mcp-vs-addon.md](blender-mcp-vs-addon.md) | maya-usd / MayaExporter comparison — MCP+skills vs thin Blender add-on decision. |
| [blender-iff-interchange.md](blender-iff-interchange.md) | IFF ↔ Scene IR ↔ Blender plan, tool layout, and Cursor skills for import/export. |
| [blender-iff-interchange-PLAN.md](blender-iff-interchange-PLAN.md) | Phased execution plan — Phases 0–6 done; **7–15 remaining** (incl. world/building). |
| [iff-tre-codebase-map.md](iff-tre-codebase-map.md) | Index of existing C++ Iff/TreeFile tools, loaders, writers, and reuse strategy for Blender pipeline. |
| [maya-exporter-reference.md](maya-exporter-reference.md) | MayaExporter plug-in source map — authoritative IFF **export** reference for Blender porting. |
| [maya-exporter-parity-checklist.md](maya-exporter-parity-checklist.md) | Row-by-row MayaExporter feature inventory with Blender phase + parity status. |
| [sample-tre-files.md](sample-tre-files.md) | **COT2000** master `.toc` + **TRE v6000** format spec for `D:\\Sample-TRE-Files` fixture. |

## Methodology

Each investigation was carried out against the unmodified February 28, 2015
SWG source archive. Findings are grounded in:

* Direct file reads (project files, `.cpp` / `.h`, build response files,
  config files).
* `ripgrep`-equivalent searches over `src/`, `tools/`, `plugin/`, `exe/`,
  `publish/`.
* Cross-referencing internal libraries against vendored 3rd-party SDKs
  (`src/external/3rd/library/`, `src/external/ours/library/`).
* Where useful, web research to corroborate provenance (e.g. the SWG-Source
  community fork that descends from this same tree).

When a claim names a specific file path or line number, the underlying read
verified it at the time of investigation. If the codebase changes, re-verify
before relying on a memory.

## Cross-references back to the high-level docs

These deep-dives complement the higher-level guides:

| If you want… | Read… |
| --- | --- |
| Repo layout and what's in each top-level directory | [`../../CLAUDE.md`](../../CLAUDE.md) |
| Cluster/process architecture overview | `docs/architecture.html` |
| Per-server roles | `docs/servers.html` |
| Client subsystem map | `docs/client.html` |
| Scripting / `dsrc/` overview | `docs/scripting.html` |
| Build system overview | `docs/build.html` |
| What's missing & how to run locally | `docs/gaps.html`, `docs/run-locally.html` |
| Glossary | `docs/glossary.html` |
