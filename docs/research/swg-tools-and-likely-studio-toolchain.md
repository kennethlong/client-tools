# SWG Tools and Likely Studio Toolchain

Date: 2026-05-26

## Executive Summary

This repository contains the remains of a broad studio content factory, not just a game client plus a few utilities. The tools fall into a recognizable MMO production pipeline: DCC export from Maya, domain editors for effects/terrain/quests/conversations/schematics/space data, compilers for templates and datatables, validators and response-file generators, archive builders for `.tre` packages, and runtime/debug tools for support and development.

The central pattern is consistent across the codebase: authors worked in high-level source formats and editor applications, generated loose SWG runtime assets under data search paths, validated those assets, then packed them into layered TRE archives consumed by the same `TreeFile` system used at runtime. The strongest local evidence is in `docs/data-pipeline.html`, the `sharedFile` IFF/TRE code, `MayaExporter`, `TemplateCompiler`, `DataTableTool`, `TreeFileRspBuilder`, and `TreeFileBuilder`.

The most important historical tool was almost certainly `MayaExporter.mll`. It was the bridge between art production and runtime formats: static meshes, skeletal mesh generators, skeletons, keyframe animation, skeletal appearance templates, shader templates, hardpoints, collision, floor data, portal/building data, and re-export/source-control metadata. The current `tools/swg_blender` work is best understood as a modern replacement for that Maya-export path, not as a replacement for the whole studio pipeline.

The sibling `Utinni` and `UtinniPlugins` repos are a different kind of toolchain. They are live-client development and modding tools: injection, hooks, a C++/C# plugin model, hosted editor UI, gizmo editing, world-snapshot editing, object browser, scene reloads, graphics debug controls, and a roadmap toward all-in-one SWG modding. They complement the repo's offline compilers and editors by validating and editing content inside a running client.

Short-term conclusion: if the team wants a practical modern toolchain, prioritize three tracks. First, continue `tools/swg_blender` as the DCC exporter/importer replacement for `MayaExporter`. Second, use or extend Utinni/Jawa Toolbox for live world placement, inspection, and reload workflows. Third, revive or replace the data build chain around `DataTableTool`, `TemplateCompiler`, `TreeFileRspBuilder`, and `TreeFileBuilder`, because those are the pieces that turn authored assets into shippable client data.

## Scope and Evidence

Local evidence reviewed:

- `docs/data-pipeline.html`
- `docs/research/iff-tre-codebase-map.md`
- `docs/research/maya-exporter-reference.md`
- `docs/research/maya-exporter-parity-checklist.md`
- `docs/research/blender-iff-interchange-PLAN.md`
- Tool/application source under `src/engine/*/application`, `src/game/*/application`, and `src/external/*/application`
- Built binaries in `tools/`
- Current Blender work under `tools/swg_blender`
- Sibling WIP repos `D:\Code\Utinni` and `D:\Code\UtinniPlugins`

Public context reviewed:

- [Swg.Explorer](https://wverkley.github.io/Swg.Explorer/) describes a community TRE viewer/explorer that previews IFF/STF data, extracts files, and exports meshes to Collada.
- [ptklatt/Utinni](https://github.com/ptklatt/Utinni) describes Utinni as a Pre-CU SWG plugin/injection framework with C++ and C# plugin frameworks, ImGuizmo, editor mode, offline scene mode, hosted client UI, and freecam.
- [ptklatt/UtinniPlugins](https://github.com/ptklatt/UtinniPlugins) documents the official Utinni plugin repo and The Jawa Toolbox feature set: gizmo snapshot editing, object browser, scene controls, reloads, wireframe, and skeleton rendering.
- [ModTheGalaxy Jawa Toolbox thread](https://modthegalaxy.com/index.php?threads%2Fretired-the-jawa-toolbox.1217%2F=) describes the original standalone Jawa Toolbox as a development tool for offline environments, object spawning, asset reloads, and live visual World Snapshot editing.
- [SWGANH Tools portal](https://wiki.swganh.org/index.php/Portal%3ASWGANH_Tools) shows the community also converged on the same categories: IFF editor, TRE packer/unpacker, draft schematic editor, STF editor, datatable editor, and heightmap tools.
- [SWGANH TRE/IFF breakdown](https://wiki.swganh.org/index.php/TRE:TRE_Breakdown) gives useful public background on SOE-style IFF forms/chunks and SWG file types.
- [io_scene_swg_msh](https://github.com/nostyleguy/io_scene_swg_msh) is a public Blender add-on for SWG formats, supporting `.mgn`, `.msh`, `.lod`, `.pob`, `.skt`, `.apt`, `.sat`, `.lmg`, and `.sht` material generation.
- [swg_tre crate](https://docs.rs/swg_tre/latest/swg_tre/) documents TRE archives as compressed SWG archive files and mirrors the same archive concept used by this repo's `TreeFile` code.
- [WPS SWG Developer Tools](https://swgnb.com/dev-tools/) lists current VS Code extensions for IFF viewing, datatable compile/decompile, datatable editing, string-table editing, and TRE packaging.
- [SWG DataTable Editor](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-datatable-editor), [SWG IFF Creator](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-iff-creator), [SWG IFF Deconstructor](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-iff-deconstructor), and [SWG TRE Packager](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-tre-packager) are active Marketplace entries in that toolchain.
- [swg_stf crate](https://docs.rs/crate/swg_stf/latest) is a Rust library for reading and creating SWG STF files and is explicitly positioned as a pipeline building block.
- [SIE Sytners IFF Editor wiki page](https://github-wiki-see.page/m/SWG-Source/swg-main/wiki/SIE-Sytners-IFF-Editor) and the public Blender add-on documentation both point to Sytner's IFF Editor as a widely used community editor for many SWG file types.

The public sources are useful context, but the local source tree is the primary source of truth for what this repo contains.

## Core File and Build Model

The toolchain revolves around a small number of runtime file abstractions:

- IFF is the universal chunked binary container. Local source: `src/engine/shared/library/sharedFile/src/shared/Iff.*`.
- TRE is the packed archive/search-path format. Local source: `src/engine/shared/library/sharedFile/src/shared/TreeFile.*` and `TreeFile_SearchNode.*`.
- `.tab` and XML spreadsheets become binary datatable IFF files.
- `.tpf` object templates become binary object template IFF files.
- Maya and Blender-style DCC assets become mesh, skeleton, animation, shader, appearance, collision, floor, and portal/building IFF files.
- `.stf` files carry string tables/localization.
- `.ws` files carry world snapshot placements.
- `.tre` files package runtime-relative file paths.

The runtime and the tools share many of the same libraries. That matters because a tool such as `MayaExporter`, `TemplateCompiler`, or `TreeFileBuilder` is not merely imitating the game format. In many places it is using the same serialization code or the same loader assumptions as the client.

## Likely Studio Pipeline

### 1. Authoring Sources

Artists, designers, and engineers likely worked in several parallel source domains:

- Maya scenes for meshes, skeletons, animations, hardpoints, shaders, collision, floor data, and building/portal data.
- Text or spreadsheet-like data for datatables, schematics, resource tuning, loot, client data, server data, and other gameplay systems.
- `.tpf` source files for object templates.
- Tool-specific editor documents for particles, terrain, conversations, quests, space zones, sound templates, UI pages, and localization.
- Script source and compiled script outputs, referenced by object templates, quests, conversations, and datatables.

There are strong signs of old studio source-control integration. `MayaExporter`, `TemplateCompiler`, and `TemplateDefinitionCompiler` include Perforce/Alienbrain concepts, submit/check-out/re-export support, changelist behavior, and log-based re-export. That suggests production assets were expected to flow through source control, not through one-off manual exports.

### 2. DCC Export

The art path likely began in Maya:

1. Artist opens a Maya asset.
2. `MayaExporter.mll` registers MEL commands such as `exportStaticMesh`, `exportSkeletalMeshGenerator`, `exportSkeleton`, `exportSkeletalAnimation`, `exportSatFile`, `export`, `reexport`, `setBaseDir`, `setAuthor`, and transform-mask/animation-list helpers.
3. Exporter writes runtime-relative loose files into data directories.
4. Exporter records re-export metadata so assets can be batch re-exported later.
5. Viewer/client tools validate the exported asset.

The exporter source includes builders/writers for static mesh, skeletal mesh generator, skeleton template, keyframe skeletal animation template, skeletal appearance template, shader templates, indexed triangle lists, vertex indexing, normal maps, floor/collision, component/detail appearances, and occluded face maps. That is broad enough that Maya was probably the studio's authoritative authoring environment for most visual assets.

### 3. Design and Gameplay Data

Design data appears to have used a spreadsheet/compiler model:

1. Designers edit `.tab` or XML-like table sources.
2. `DataTableTool` converts those sources into IFF datatables.
3. Specialized generators such as `WeaponExporterTool`, `CoreWeaponExporterTool`, `ArmorExporterTool`, and `SwgSchematicXmlParser` generate `.tpf` template sources for schematics and tangible/intangible objects.
4. `TemplateCompiler` compiles `.tpf` into binary object template `.iff`.
5. `DataLintRspBuilder` builds lint response files and duplicate/test reports to detect bad or duplicate assets.

This suggests the studio did not hand-author every runtime object template. Some classes of content, especially crafted items and schematics, were probably generated from higher-level tables.

### 4. Domain Editors

Separate editor apps existed for complex domains:

- `ParticleEditor`, `SwooshEditor`, `LightningEditor`, and `ClientEffectEditor` for visual effects.
- `TerrainEditor` for terrain generator layers, filters, affectors, flora, shaders, fractals, radial/environment groups, and terrain baking.
- `QuestEditor`, `SwgConversationEditor`, `SwgContentBuilder`, `SwgSpaceQuestEditor`, and `SwgSpaceZoneEditor` for quest and mission authoring.
- `SoundEditor` for sound template creation, sample lists, attenuation, pitch/volume/fade behavior, and spreadsheet reporting.
- `UiBuilder`, `UiFontBuilder`, `StringFileTool`, `LocalizationTool`, and related utilities for UI and localization.

These were likely used by dedicated designers/content developers rather than programmers alone.

### 5. Packaging

The packaging path likely looked like this:

1. Client/server config files define layered `searchPath` directories.
2. `TreeFileRspBuilder` scans those search paths and emits bucketed `.rsp` manifests such as:
   - `data_uncompressed_music.rsp`
   - `data_uncompressed_sample.rsp`
   - `data_compressed_texture.rsp`
   - `data_compressed_animation.rsp`
   - `data_compressed_mesh_static.rsp`
   - `data_compressed_mesh_skeletal.rsp`
   - `data_compressed_other.rsp`
3. `TreeFileBuilder` packs each response file into a `.tre`.
4. Publish manifests in `publish/*.mft` and patch scripts determine shipped archive order.
5. The client mounts TREs and loose override paths through `TreeFile`.

This is a layered archive workflow, not an in-place patch workflow. Newer data can override older data by search order.

## Tool Inventory by Category

### Archive, IFF, and Build Packaging

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `TreeFileBuilder` | `src/engine/shared/application/TreeFileBuilder` | Packs response-file entries into `.tre` archives, with optional TOC/file compression controls. | Build/release engineers |
| `TreeFileExtractor` | `src/engine/shared/application/TreeFileExtractor` | Lists or extracts files from a TRE with `-l` and `-e`. | Engineers, modders, build debugging |
| `TreeFileRspBuilder` | `src/engine/shared/application/TreeFileRspBuilder` | Reads search paths from config and emits bucketed `.rsp` manifests by file type. | Build/release engineers |
| `DataLintRspBuilder` | `src/engine/shared/application/DataLintRspBuilder` | Builds `DataLint.rsp` or `DataLintServer.rsp`, reports duplicates, removed entries, and test assets. | Build/release engineers, data QA |
| `ClientCacheFileBuilder` | `src/engine/client/application/ClientCacheFileBuilder` | Builds client-side cache data; exact scope should be verified before use. | Build engineers |
| `Miff` | `src/engine/client/application/Miff` | Text-to-IFF compiler using a C/C++ preprocessor style source format. | Engineers/toolsmiths |
| `ViewIff` | `src/engine/client/application/ViewIff` | MFC IFF tree/data viewer. | Engineers, technical artists |
| `Swg.Explorer` equivalent | External context | Community TRE/IFF/STF viewer and extractor; useful comparison point, not local code. | Modders |

Most important takeaway: `TreeFileRspBuilder` and `TreeFileBuilder` are the bridge from loose data to shippable archives. Any modern pipeline should preserve their semantics or intentionally replace them with compatible output.

### Template, Datatable, and Generated Gameplay Data

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `DataTableTool` | `src/engine/shared/application/DataTableTool` | Converts spreadsheet/XML datatable source into binary datatable IFF, with test/edit/add modes. | Designers, build engineers |
| `TemplateCompiler` | `src/engine/shared/application/TemplateCompiler` | Generates default `.tpf` from `.tdf`, compiles `.tpf` into object template IFF, and has Perforce submit paths. | Designers, engineers |
| `TemplateDefinitionCompiler` | `src/engine/shared/application/TemplateDefinitionCompiler` | Compiles `.tdf` template definitions into generated C++ template classes/sources, with Perforce paths. | Engine/gameplay engineers |
| `ArmorExporterTool` | `src/engine/shared/application/ArmorExporterTool` | Reads armor schematic datatable IFF and writes server/shared `.tpf`, then compiles templates. | Systems designers |
| `WeaponExporterTool` | `src/engine/shared/application/WeaponExporterTool` | Reads weapon schematic datatable IFF and writes server/shared `.tpf`, then compiles templates. | Systems designers |
| `CoreWeaponExporterTool` | `src/engine/shared/application/CoreWeaponExporterTool` | Similar to weapon exporter, with core weapon columns and export path controls. | Systems designers |
| `SwgSchematicXmlParser` | `src/game/server/application/SwgSchematicXmlParser` | Converts XML schematic/object definitions into server/shared tangible and draft schematic `.tpf` files. | Systems designers/toolsmiths |
| `SwgDraftSchematicEditor` | `src/game/server/application/SwgDraftSchematicEditor` | GUI editor for draft schematic slots, attributes, and properties. | Systems designers |
| `LabelHashTool` | `src/engine/shared/application/LabelHashTool` | Generates/inspects label hashes used by asset/data systems. | Engineers/data QA |
| `hash_plugin.dll` | `tools/hash_plugin.dll`, source under `src/engine/shared/application/hash_plugin` | Hash plugin utility; probably used by source-control/editor workflows. | Engineers/toolsmiths |
| `newguid.exe` | `tools/newguid.exe` | GUID generation utility. | Engineers/build scripts |
| `Md5sum` | `src/engine/shared/application/Md5sum` | Checksum utility. | Build/release engineers |
| `VersionNumber` | `src/engine/shared/application/VersionNumber` | Version stamping utility. | Build/release engineers |

The data/template tools are one of the highest-value revival targets. They encode the difference between "asset exists" and "asset is usable by object templates, schematics, crafting, UI strings, and game systems."

### Maya and Visual Asset Production

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `MayaExporter` | `src/engine/client/application/MayaExporter` | Maya plugin for static meshes, skeletal meshes, skeletons, animations, skeletal appearances, shader templates, hardpoints, collision, floor, component/detail appearances, and re-export metadata. | Artists, animators, technical artists |
| `Viewer` | `src/engine/client/application/Viewer` | Asset viewer for appearances, animation, skeletons, wearables, and exported content validation. | Artists, technical artists, engineers |
| `AnimationEditor` | `src/engine/client/application/AnimationEditor` | Animation state hierarchy/logical animation table/editor tooling. | Animators, technical animators |
| `ShaderBuilder` | `src/engine/client/application/ShaderBuilder` | Shader/effect/template editor for render-state and texture-stage style material setup. | Technical artists |
| `TextureBuilder` | `src/engine/client/application/TextureBuilder` | Texture construction/blueprint tool; likely used for composite/customization textures. | Technical artists |
| `CreateShaderTemplate` | `src/engine/client/application/CreateShaderTemplate` | Shader-template generation utility; source should be checked before relying on it. | Technical artists/toolsmiths |
| `DXTex` | `src/external/3rd/application/DXTex` | DirectX texture utility. | Artists/technical artists |
| `SetBrightnessContrastGamma` | `src/engine/client/application/SetBrightnessContrastGamma` | Image adjustment utility. | Artists/toolsmiths |
| `SoePix` | `src/engine/client/application/SoePix` | Legacy image/pixel utility; exact production role needs deeper verification. | Artists/toolsmiths |

The Maya exporter is the key legacy tool here. If the goal is to create or modify complex geometry, characters, wearables, buildings, and animations, `MayaExporter` is the historical reference implementation.

### Blender Replacement Work

| Tool | Location | Role | Current status |
| --- | --- | --- | --- |
| `swg_iff` | `tools/swg_blender/swg_iff` | Python IFF reader/dumper/tag foundation. | Implemented foundation |
| `swg_scene` | `tools/swg_blender/swg_scene` | Scene intermediate representation and loaders/writers for SWG formats. | Active |
| `swg_blender` | `tools/swg_blender/swg_blender` | Blender import/export command-line modules. | Active |
| `swg_blender_addon` | `tools/swg_blender/swg_blender_addon` | Blender add-on with import/export operators and SWG sidebar. | Active |
| `swg_pipeline` | `tools/swg_blender/swg_pipeline` | TRE wrappers, response-file generation, export bundles, DDS/normal-map/shader helpers, validation. | Active |

Current local documentation says the Blender path has completed or partially completed:

- IFF foundation.
- Static mesh import.
- Static mesh export MVP.
- Skeletal import.
- Skeletal mesh, skeleton, and animation export MVP.
- Pipeline bundle generation and add-on integration.
- Version-aware TRE listing.
- Static-export improvements such as hardpoints, vertex colors, SIDX, index optimization, multi-UV, DOT3, normal-map bake, shader export from Blender materials, and index splitting.
- Skeletal appearance `.sat` load/export for several SMAT versions.

The Phase 7 validation document says client validation is still pending user-side in-game confirmation for a static prop and skeletal arms. That means the Blender path is promising and already detailed, but it should still be treated as a replacement-in-progress rather than a finished `MayaExporter` equivalent.

Compared with `MayaExporter`, the biggest remaining parity areas are likely:

- Full shader/material parity.
- SAT/LMG/APT/CMPA/DTLA and wearable/customization workflows.
- Collision, floor, extents, and portalized object/building data.
- Animation-state graph and logical animation table integration.
- Source-control/re-export metadata.
- Batch conversion against real asset corpora.

The public `io_scene_swg_msh` Blender add-on is useful context because it shows the community has also identified Blender as the practical replacement for old Maya tooling. It supports several SWG formats that overlap with this repo's current `tools/swg_blender` goals.

### Effects, Audio, and Client Presentation Tools

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `ParticleEditor` | `src/engine/client/application/ParticleEditor` | Particle emitter/effect editor with waveforms, color ramps, attachments, emitter properties, and particle-system tuning. | VFX artists |
| `ClientEffectEditor` | `src/engine/client/application/ClientEffectEditor` | Client effect composition/editor tooling. | VFX artists/designers |
| `LightningEditor` | `src/engine/client/application/LightningEditor` | Lightning effect authoring/preview. | VFX artists |
| `SwooshEditor` | `src/engine/client/application/SwooshEditor` | Swoosh/trail effect authoring/preview. | VFX artists |
| `SoundEditor` | `src/engine/client/application/SoundEditor` | Sound template editor for 2D/3D sounds, attenuation, sample lists, pitch/volume/fade ranges, loop delays, and reports. | Audio designers |
| `NpcEditor` | `src/engine/client/application/NpcEditor` | NPC client data/template writer and viewer. | Content designers |
| `ShipComponentEditor` | `src/engine/client/application/ShipComponentEditor` | Space ship component/chassis compatibility/tuning editor. | Space systems designers |

The effects tools indicate that visual presentation assets were authored outside Maya when the domain benefited from a specialized preview/edit UI. Particle/swoosh/lightning/client-effect assets would likely be referenced by templates, datatables, scripts, or client data.

### Terrain, World, and Space Tools

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `TerrainEditor` | `src/engine/client/application/TerrainEditor` | Terrain generator editor with layers, filters, affectors, bitmap/fractal/shader/flora/radial/environment groups, 3D preview, and terrain baking. | World builders |
| `WorldSnapshotViewer` | `src/engine/client/application/WorldSnapshotViewer` | Viewer for world snapshot data. | World builders, engineers |
| `PlanetWatcher` | `src/engine/shared/application/PlanetWatcher` | Planet/server/object monitoring utility; exact scope should be verified before active use. | Engineers/live ops |
| `Turf` | `src/engine/shared/application/Turf` | Legacy utility with unclear scope from high-level inventory; likely terrain/world related but needs source-specific confirmation. | Engineers/toolsmiths |
| `SwgSpaceZoneEditor` | `src/game/server/application/SwgSpaceZoneEditor` | Space zone map/property/mobile placement editor. | Space/world designers |
| `SwgSpaceQuestEditor` | `src/game/server/application/SwgSpaceQuestEditor` | Space mission/quest authoring including cargo and mission template dialogs. | Space content designers |
| `SwgBattlefieldTool` | `src/game/server/application/SwgBattlefieldTool` | Battlefield content/config tool. | Systems/content designers |

For a studio-supported SWG workflow, terrain and world data were probably not authored entirely in one tool. `TerrainEditor` handled procedural terrain layers and baking, Maya handled many built objects and collision/portal assets, and snapshot/buildout-style placement was inspected or edited separately.

### Quest, Conversation, and Content Tools

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `QuestEditor` | `src/engine/client/application/QuestEditor` | Generic quest editor with element/property/list editing. | Content designers |
| `SwgConversationEditor` | `src/game/server/application/SwgConversationEditor` | Conversation tree, script, and string-table editing. | Quest/dialog designers |
| `SwgContentBuilder` | `src/game/server/application/SwgContentBuilder` | Content-builder dialogs for quests, rewards, NPC conversations, and mission NPC conversations. | Content designers |
| `SwgDraftSchematicEditor` | `src/game/server/application/SwgDraftSchematicEditor` | Draft schematic GUI editor. | Crafting/systems designers |
| `SwgNameGenerator` | `src/game/server/application/SwgNameGenerator` | Name generation utility. | Designers/QA |

The content tools reinforce the same split: data is edited in custom domain UIs, then converted or saved to runtime formats that the build chain packages.

### UI and Localization

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `UiBuilder` | `src/engine/client/application/UiBuilder` and `src/external/ours/application/UiBuilder` | UI layout/page editing and property management. | UI designers/engineers |
| `UiFontBuilder` | `src/external/ours/application/UiFontBuilder` | Font atlas/font asset building. | UI engineers |
| `StringFileTool` | `src/engine/shared/application/StringFileTool` | STF string table diff/merge/edit tool. | Localization/content teams |
| `LocalizationTool` | `src/external/ours/application/LocalizationTool` | Legacy localization GUI. | Localization team |
| `LocalizationToolCon` | `src/external/ours/application/LocalizationToolCon` | Console localization utility. | Build/localization engineers |
| `UpdateLocalizedStrings` | `src/engine/shared/application/UpdateLocalizedStrings` | Updates localized string assets. | Localization/build engineers |
| `WordCountTool` | `src/engine/shared/application/WordCountTool` | Word-count/report utility. | Localization/producers |

Localization had enough tooling to imply a real production workflow: author strings, diff/merge `.stf`, update localized strings, and run word-count reporting.

### Runtime, Debug, QA, and Operations Tools

| Tool | Location | Role | Likely users |
| --- | --- | --- | --- |
| `SwgClient` | `src/game/client/application/SwgClient` | Main game client. | Runtime |
| `SwgHeadlessClient` / `Headless` | `src/game/client/application/SwgHeadlessClient`, `src/engine/client/application/Headless` | Headless client/runtime test mode. | QA/engineers |
| `SwgGodClient` | `src/game/client/application/SwgGodClient` | Admin/GM-style client. | Designers, QA, live ops |
| `SwgCsTool` | `src/game/client/application/SwgCsTool` | Customer-service/support tool. | CS/live ops |
| `SwgLoadClient` | `src/game/server/application/SwgLoadClient` | Load-test client. | QA/server engineers |
| `SwgContentSync` | `src/game/server/application/SwgContentSync` | Content synchronization utility. | Build/live ops |
| `RemoteDebugTool` | `src/engine/client/application/RemoteDebugTool` | Remote debug UI/tooling. | Engineers |
| `DebugWindow` | `src/engine/client/application/DebugWindow` | Debug window utility. | Engineers |
| `BugTool` | `src/engine/client/application/BugTool` | Bug-reporting utility. | QA |
| `CrashReporter` | `src/engine/client/application/CrashReporter` | Crash-report collection/reporting. | QA/live ops |
| `AddressToLine` | `src/engine/shared/application/AddressToLine` | Symbol/address lookup utility. | Engineers |
| `LagOMatic` | `src/external/3rd/application/LagOMatic` | Network/latency simulation or test utility. | QA/engineers |
| `Direct3d9`, `Direct3d11`, `DllExport` | `src/engine/client/application/*` | Runtime/render/plugin support projects, not content authoring tools. | Engine engineers |

These tools sit around production rather than inside content creation. They help debug, test, support, and operate the client.

## Utinni and UtinniPlugins WIP Tooling

The sibling `D:\Code\Utinni` repo is a modern live-client tool framework. Its local docs describe:

- `Launcher.exe`: starts the SWG client suspended, patches the entry point, injects `UtinniCore.dll`, restores execution.
- `UtinniCore.dll`: native hook/runtime layer using DetourXS, ImGui, ImGuizmo, spdlog, INI handling, C++ plugin loading, and CLR hosting.
- `UtinniCoreDotNet.dll`: C#/.NET editor framework, callbacks, plugin loader, WinForms host, hotkeys, undo/redo, themes.
- Plugin loading for native C++ plugins and C# editor plugins.
- Re-parenting the SWG client window into a WinForms editor shell.
- Hooks into client/game/graphics/terrain/treefile/UI systems.

The local Utinni vision document frames the problem as a fragmented SWG tool ecosystem: TRE browser, IFF editor, object template editor, mesh tools, particle editor, creature editor, conversation editor, quest editor, datatable editor, terrain editor, world snapshot editor, buildout editing, script tooling, STF editing, UI editing, shader editing, and TRE packaging. Its proposed answer is one live-client-centered editor that can browse, edit, preview, author, package, and share content.

`D:\Code\UtinniPlugins` contains the official plugin side, especially The Jawa Toolbox. Its local docs and public README identify these capabilities:

- World snapshot editing: add, remove, move, rotate, duplicate, save/save-as.
- Gizmo manipulation with snapping and undo/redo.
- Object browser populated from loaded game files, with drag/drop placement.
- Offline scene loading and improved free-camera movement.
- Scene controls: reload terrain, reload snapshot, reload UI, time of day, weather, chat modal.
- Player controls: teleport, speed, position, visibility.
- Graphics debug: wireframe, skeleton bones, texture reloads.
- Slash commands for camera/player/speed/reload/time/weather workflows.

Utinni should be treated as a live validation and world-authoring layer, not as a replacement for the offline compilers. It can make the iteration loop much faster because it runs against the actual client and can reload/inspect content in context.

## Dependency Findings

The earlier risk list said several old SDK dependencies were unknown. The local source and project files answer most of that. The high-level answer is that this is a 32-bit Windows toolchain built around old Visual C++ conventions, static MFC for many GUI tools, Qt 3 for several newer editor tools, DirectX 9-era rendering/audio/input libraries, and several bundled third-party libraries under `src/external/3rd/library`.

The `deps/` folder currently contains:

- `BuildTools_Full.exe`
- Visual C++ 2013 x86 redistributable
- Visual Studio 2013 x86 multibyte MFC library installer
- `vcredist_x86.exe`

That does not mean every legacy tool is VS2013-native. Several project files still refer to old library layouts such as `external/3rd/library/qt/3.3.4`, `maya7`, `perforce`, `alienbrain123`, `miles`, `dpvs`, and `directx9`. The repo carries many of those third-party folders, but build success still depends on whether their headers/libs are complete and compatible with the selected compiler.

### Confirmed Local Third-Party Dependency Buckets

| Dependency | Evidence | Tools most affected | Practical implication |
| --- | --- | --- | --- |
| Win32 x86 | Project files use `win32`, old `.dsp` files, x86 library folders, `INTEL` MFC libs, and x86 redistributables in `deps/`. | Nearly all legacy tools. | Assume 32-bit builds first. 64-bit ports are separate engineering work. |
| MFC/ATL | Many MFC GUI tools have `<UseOfMfc>Static</UseOfMfc>` and include `external/3rd/library/atlmfc`. | `TerrainEditor`, `ShaderBuilder`, `TextureBuilder`, `Viewer`, `ViewIff`, `WorldSnapshotViewer`, many SWG server editors. | Needs multibyte/static MFC support. The VS2013 multibyte MFC installer in `deps/` is directly relevant. |
| Qt 3.3.4 | Project files link `qt-mt334.lib` and `qtmain.lib`; source uses `QApplication`, `QWidget`, etc. | `AnimationEditor`, `ParticleEditor`, `SoundEditor`, likely several effect/client editors. | Modern Qt will not be a drop-in replacement. Build/revival likely requires bundled Qt 3.3.4 or porting. |
| DirectX 9 / DirectInput / DirectSound | Project files link `d3d9.lib`, `d3dx9.lib`, `dinput8.lib`, `dsound.lib`, `dxguid.lib`, `ddraw.lib`. | Client viewers/editors, shader/UI/texture tools, runtime projects. | Needs the old DirectX SDK library layout or equivalent vendored libs. D3DX is deprecated in modern Windows SDKs. |
| DPVS | Many tools link `dpvs.lib` from `external/3rd/library/dpvs/lib/win32-x86`. | Render/scene/viewer/editor tools. | Culling library is part of the old engine runtime stack; keep vendored binary compatibility. |
| Miles Sound System | Tools link `mss32.lib`; `SoundEditor` displays the Miles version through `Audio::getMilesVersion()`. | `SoundEditor`, `Viewer`, `AnimationEditor`, particle/effect tools that initialize client audio. | Proprietary audio SDK/runtime dependency. A full rebuild requires matching headers/libs; runtime needs `mss32.dll`. |
| libxml2 2.6.7 | Project files link `libxml2-win32-debug.lib` or `libxml2-win32-release.lib`; include paths point to `libxml2-2.6.7.win32`. | Datatable/XML tools, many editors, world/space tools. | Vendored old libxml2 is part of the build graph. |
| zlib | `TreeFileBuilder`, `DataTableTool`, content editors, and packaging tools link zlib. | TRE, datatable, compression paths. | Straightforward dependency; likely easiest to modernize if needed. |
| PCRE 4.1 | Several tools link `libpcre.a`; template compiler and Qt/editor tools use shared regex systems. | `TemplateCompiler`, `AnimationEditor`, `ParticleEditor`. | Old static library format may be compiler-sensitive. |
| Perforce C++ API | `TemplateCompiler` and `TemplateDefinitionCompiler` include `clientapi.h` and link `libclient.lib`, `librpc.lib`, `libsupp.lib`; `MayaExporter` also links Perforce libs. | Template compilers, Maya export source-control paths. | Compile can be blocked by Perforce API binary compatibility. Runtime compile/use could be decoupled from submit/check-out features if needed. |
| Alienbrain NxN | `MayaExporter` links `NxN_alienbrain_XDK_123.lib`; source includes `NxNIntegratorSDK.h` and wraps Alienbrain in `AlienbrainConnection`. | `MayaExporter`. | This is one of the hardest legacy dependencies. For modern Blender work, do not recreate it; replace with normal filesystem/git workflow. |
| Maya SDK | `MayaExporter` links `Foundation.lib`, `OpenMaya.lib`, `OpenMayaAnim.lib` from `external/3rd/library/maya7/lib`; source includes many `maya/*` headers. | `MayaExporter`. | Building the original plugin requires a matching old Maya SDK. This strongly supports replacing it with Blender tooling instead of reviving it first. |
| ATI/NVIDIA texture/mesh helpers | `MayaExporter` links `ati_compress`, `atidxtc`, and includes `nvtristrip` libraries. | `MayaExporter`, texture/shader export paths. | Explains legacy DDS compression and index/strip optimization behavior. Blender replacement needs equivalent behavior, not identical libraries. |
| Oracle OCI / ODBC | `MayaExporter` links `oci.lib`, `ociw32.lib`, `odbc32.lib`, `odbccp32.lib`. | Asset database/source-control-adjacent Maya exporter paths. | Likely tied to internal asset database behavior; should be isolated or skipped for modern export work. |
| Mozilla embedding libraries | Several richer client/editor tools link `nspr4`, `plc4`, `xpcom`, `xul`, and `profdirserviceprovider_s`. | `AnimationEditor`, `ParticleEditor`, `Viewer`. | Old embedded browser/UI dependency; likely fragile and not needed for minimal asset validation. |
| TinyXML | `SwgSchematicXmlParser` includes `tinyxml.h` and uses `TiXmlDocument`/`TiXmlElement`. | `SwgSchematicXmlParser`. | Small, likely easy to build if bundled tinyxml is intact. |
| Bison/Flex/GNU cpp | `Miff` has `parser.yac`, `parser.lex`, `bison.simple`, and docs say source should be preprocessed through `cpp`. | `Miff`. | Rebuild requires old parser-generation assumptions or checked-in generated files. |

### Tool-Specific Build Risk

| Tool family | Dependency risk | Answer |
| --- | --- | --- |
| Core archive/data CLIs | Low to medium. `TreeFileBuilder` is mostly zlib; `DataTableTool` is zlib plus libxml2; `TreeFileExtractor` rides shared engine file/compression code. | These are the most realistic legacy tools to revive first. |
| Template compilers | Medium. They depend on Perforce API and PCRE/zlib, but the compile path is separable from Perforce submit/check-out behavior in source. | Candidate for a "no source-control" build mode if Perforce libs are missing. |
| Maya exporter | Very high. Requires old Maya SDK, Alienbrain XDK, Perforce API, Oracle/ODBC, ATI/NVIDIA helper libs, DPVS, Miles, libxml2, DirectX, and engine libraries. | Use it as specification/reference. Do not make it the first revival target unless exact old SDKs are already present and buildable. |
| Qt editors | High. Qt 3.3.4 and old linked libraries are the gating dependency. | Useful source references, but likely expensive to revive wholesale. |
| MFC editors | Medium to high. Static multibyte MFC is available via `deps`, but old DirectX/DPVS/Miles/libxml2 dependencies still apply. | Some may build sooner than Qt tools, but runtime validation is still required. |
| Utinni | High but different. It is a live injection framework tied to SWG client binary RVAs/hooks, .NET Framework/WinForms, C++/C# interop, and DetourXS. | Valuable for live editing, but sensitive to exact client executable versions. |
| `tools/swg_blender` | Low to medium. Python/Blender/pytest dependency surface is modern and isolated. | Best near-term DCC replacement path. |

## Current Alternative Tools Worth Tracking

These are tools outside this repo that may reduce the amount of legacy tooling we need to revive.

| Tool | Source | What it does | Fit for this project |
| --- | --- | --- | --- |
| WPS SWG Developer Tools | [swgnb.com/dev-tools](https://swgnb.com/dev-tools/) | VS Code suite for IFF viewing, datatable edit/compile/decompile, STF editing, and TRE packaging. | Strong candidate for modern data/localization/TRE workflows, especially where old MFC/Qt tools are too expensive to revive. Need compatibility tests against this client's `DataTableTool` and `TreeFileBuilder`. |
| SWG DataTable Editor | [Marketplace](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-datatable-editor) | Edits `.tab` datatables with aligned columns and type validation, based on official SWG source code analysis. | Good replacement for simple spreadsheet editing and validation. Does not replace all binary compile semantics by itself. |
| SWG IFF Creator | [Marketplace](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-iff-creator) | Compiles `.tab` or datatable XML to IFF from VS Code, implemented in TypeScript. | Potential replacement for `DataTableTool` in day-to-day workflows if byte/semantic compatibility checks pass. |
| SWG IFF Deconstructor | [Marketplace](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-iff-deconstructor) | Converts datatable IFF back to `.tab` or XML. | Useful for recovery, diffing, and documenting shipped data. Scope appears datatable-focused, not arbitrary IFF. |
| SWG TRE Packager | [Marketplace](https://marketplace.visualstudio.com/items?itemName=WastedPotentialStudiosLLC.swg-tre-packager) | Builds TRE v0005 archives from selected files/folders with compression and validation. | Possible modern replacement or adjunct to `TreeFileBuilder`; must test archive compatibility and patch/search-order conventions. |
| Sytner's IFF Editor (SIE) | [SWG-Source wiki mirror](https://github-wiki-see.page/m/SWG-Source/swg-main/wiki/SIE-Sytners-IFF-Editor), also referenced by `io_scene_swg_msh` | Community editor for many SWG IFF file types. | Valuable manual inspection/editor tool. Best treated as a community reference and emergency editor, not an automated build pipeline. |
| Swg.Explorer | [wverkley.github.io/Swg.Explorer](https://wverkley.github.io/Swg.Explorer/) | TRE archive viewer/explorer, IFF/STF/media preview, extraction, mesh export to Collada, STF export to CSV. | Useful browsing and extraction tool, especially for comparing archive contents. Less relevant for authoring. |
| `io_scene_swg_msh` | [GitHub](https://github.com/nostyleguy/io_scene_swg_msh) | Blender add-on for `.mgn`, `.msh`, `.lod`, `.pob`, `.skt`; can generate `.apt`, `.sat`, `.lmg`, materials from `.sht`, and POB hierarchies. | Important comparison point for `tools/swg_blender`. It may already cover formats still on this repo's roadmap. Need direct compatibility tests before adopting code or behavior. |
| `swg_tre` | [docs.rs](https://docs.rs/swg_tre/latest/swg_tre/) | Rust library for reading and creating TRE files. | Useful if a modern CLI/library wrapper is desired. Need verify version/compression/MD5 compatibility with this client's `TreeFile`. |
| `swg_stf` | [docs.rs](https://docs.rs/crate/swg_stf/latest) | Rust library for reading and creating STF files. | Useful for localization automation and tests. Could replace parts of old STF tooling if compatible. |
| `swb-repo-cli` | [GitHub](https://github.com/jdswebb/swb-repo-cli) | Command-line tree archive editor. | Potential modern CLI comparison for TRE workflows. Needs feature and compatibility evaluation before adoption. |
| SWGANH-era tools | [SWGANH Tools portal](https://wiki.swganh.org/index.php/Portal%3ASWGANH_Tools) | Lists IFF editor, TRE packer/unpacker, Draft Schematic Editor, STF tools, DataTable Editor, HeightMap Tool, packet/test tools. | Historical/community context. Availability and compatibility are uncertain; likely less attractive than WPS/Blender/Utinni unless source/binaries are easy to obtain. |
| ModTheGalaxy ecosystem | [ModTheGalaxy](https://modthegalaxy.com/index.php) | Forum/community resources, guides, tools, and current modding discussion. | Important discovery channel. Some downloads may require accounts or Discord/community access. |

### Source Availability and Activity Status

Status checked on 2026-05-26. "Active" here means there is public evidence of recent maintenance or publication, not that the tool is proven compatible with this repo.

| Tool/project | Open source? | Activity signal | Notes |
| --- | --- | --- | --- |
| `tools/swg_blender` | Local repo code, yes for this workspace. | Active local WIP as of this research pass. | Best-aligned modern DCC replacement because it is being built against this repo's source and docs. |
| Utinni | Yes, public GitHub, MIT license. | GitHub API: last push 2021-07-02; repo metadata updated 2026-05-20. | Source is available, but code activity appears old. Still valuable because local sibling repo docs and plugins are present. |
| UtinniPlugins / Jawa Toolbox | Yes, public GitHub, MIT license. | GitHub API: last push 2021-07-02; metadata updated 2025-02-28. | Source is available, but apparent public code activity is old. Local docs remain useful. |
| `io_scene_swg_msh` | Yes, public GitHub. No SPDX license was exposed by GitHub API in this pass. | GitHub API: last push 2026-03-07; metadata updated 2026-03-13. | Appears active/recent. License must be checked manually before borrowing code. Very relevant feature overlap with Blender work. |
| Swg.Explorer | Yes, public GitHub. No SPDX license was exposed by GitHub API in this pass. | GitHub API: last push 2013-03-16; metadata updated 2025-08-24. | Source exists but appears stale. Good reference/extractor, not a likely code dependency. |
| WPS VS Code extensions | Source availability unclear from Marketplace pages. Some pages state MIT license text, but a public repository was not confirmed in this pass. | Marketplace/search pages were crawled within the last few weeks and show copyright ranges through 2026. | Appears active/current as packaged tools. Treat as external binaries/extensions until source repo is verified. |
| Sytner's IFF Editor (SIE) | Source not confirmed. Distribution appears community/forum based. | Still repeatedly referenced by current community tooling/docs. | Likely active in use, but source and maintenance status are unclear without ModTheGalaxy access. |
| `swg_tre` / `swg_stf` crates | Yes, repository listed as `Smash-Wars-Galaxies/swg-rs`; license AGPL-3.0 from GitHub API. | Crates updated 2024-11-03 and 2024-11-05; repo last push 2024-11-09. | Open source and recent enough to evaluate. AGPL license is important if integrating rather than using as a separate tool. |
| `swb-repo-cli` | Yes, public GitHub. No SPDX license was exposed by GitHub API in this pass. | GitHub API: last push 2025-01-14; metadata updated 2025-08-10. | Public and relatively recent. Needs deeper feature review. |
| SWGANH-era tools | Mixed/unclear. | Wiki is still crawled, but many listed tools are old. | Treat as historical references unless source/binaries are found. |
| ModTheGalaxy-hosted tools | Mixed/unclear. | Forum is active/current, but individual tools vary. | Discovery channel more than a stable dependency source. |

### Alternative Tooling Gaps

The current alternative ecosystem is strongest for:

- TRE browsing/packaging.
- Datatable editing and compile/decompile.
- STF editing.
- Manual IFF inspection/editing.
- Blender mesh import/export.
- Live world snapshot placement via Utinni/Jawa Toolbox.

It is weaker for:

- Full source-compatible object template generation/compilation.
- Quest/conversation authoring with exact server integration.
- Particle/swoosh/lightning/client-effect authoring with exact runtime preview.
- Terrain generator authoring and baking.
- Complete `MayaExporter` parity for collision, floor, POB, SAT/APT/LMG/CMPA/DTLA, shader/customization, and batch re-export.
- Automated end-to-end packaging that exactly mirrors the old publish manifest and patch-tree process.

### Feature Notes on New Third-Party Finds

#### WPS SWG Developer Tools

The WPS tools are the most complete modern editor-integrated alternative found in this pass. They are implemented as VS Code extensions and are aimed at day-to-day content work rather than old Windows/MFC desktop workflows.

The published WPS toolchain page groups the suite into core IFF workflow, data/localization, and packaging. The important feature split:

- `SWG IFF Viewer`: right-click `.iff` inspection, chunk hierarchy visualization, offsets, file statistics, embedded string extraction, and enhanced DataTable/DTII previews. The Marketplace page also says it supports common SWG form types such as DTII, shader/object template forms, mesh, skeleton, animation, and appearance, but it is inspection-only.
- `SWG IFF Creator`: compiles `.tab` or DataTable XML into SWG-ready IFF directly from VS Code. It advertises batch compile, a pure TypeScript compiler, no external dependency requirement, and common datatable type support.
- `SWG IFF Deconstructor`: converts DataTable IFF back to `.tab` or XML, with single-file and batch-folder operation. The useful niche is recovery and diffability of binary datatables. The stated scope is DataTable IFF, not arbitrary SWG IFF authoring.
- `SWG DataTable Editor`: opens `.tab` files in a readable aligned layout, validates column types, supports save-back to normal tab-delimited format, and understands common SWG type codes such as integer, float, string, hash string, bool, enum, packed objvars, and comments. Its Marketplace page says it was built from analysis of official `DataTable`/`DataTableWriter` source.
- `SWG String File Editor`: listed on the WPS toolchain page as a binary STF editing workflow that converts to readable text, preserves IDs/CRC mappings, supports comments and multi-line strings, and saves back to binary.
- `SWG TRE Packager`: packages selected files or folders into game-ready `.tre` archives. The WPS page describes TRE v0005 output, compression, relative path preservation, validation, and MD5 integrity blocks.

Where these fit:

- Strong replacement candidates for `DataTableTool` day-to-day usage, `StringFileTool` basics, simple IFF inspection, and simple TRE packaging.
- Weak replacement candidates for object-template compilation, shader/mesh/appearance authoring, quest/conversation/particle/terrain editors, and exact old publish manifest behavior.
- Must be validated against this codebase's loaders. "Works for SWG" does not automatically mean byte-identical to this client's expected archive and datatable variants.
- Source availability is currently unclear from public Marketplace pages. Some pages include MIT license text, but until a repository is identified, assume these are external extensions rather than code we can vendor or patch.
- Activity appears current: search/Marketplace pages show recent crawls and 2024-2026 copyright/version language.

Recommended validation:

- Pick a representative `.tab` and compile it with WPS IFF Creator and local `DataTableTool`; load both through this client's `DataTableManager`.
- Use WPS IFF Viewer and local `ViewIff` on the same `.iff`; compare hierarchy and tag interpretation.
- Package a minimal test TRE with WPS TRE Packager and local `TreeFileBuilder`; validate both with local `TreeFileExtractor` and with a running client search path.
- Round-trip one `.stf` with WPS String File Editor and local string/localization tooling.

#### Sytner's IFF Editor (SIE)

SIE appears repeatedly in community references as one of the primary manual SWG file editors. The public Blender add-on documentation calls it a recommended external tool and describes it as an editor for almost all SWG file types. Reddit/community references also point users to SIE for opening TREs and extracting assets such as sounds.

Likely strengths:

- Manual inspection/editing of broad IFF file families.
- Practical modder workflow for one-off fixes, exploration, extraction, and emergency patching.
- Complements `ViewIff` because it is community-maintained/used and likely has file-type-specific affordances not present in the old local viewer.

Likely limits:

- Not a deterministic build pipeline.
- May require ModTheGalaxy access to download.
- Compatibility depends on the specific SWG era/file variant.
- Manual editing can bypass source formats such as `.tab`, `.tpf`, and tool-specific editor documents, which makes long-term maintenance harder.

Best use here: keep it in the toolbox for inspection, diffing, and format research, but avoid making it the authoritative authoring path for generated data.

Source/activity note: source availability was not confirmed. Community references are current enough to treat it as relevant, but not enough to treat it as an actively maintained open-source dependency.

#### Swg.Explorer

Swg.Explorer is a community TRE archive explorer. Its public feature list is clear:

- Load multiple TRE archives into one combined view.
- View and preview media/data files.
- Preview IFF and STF.
- Extract files.
- Export static and dynamic meshes to Collada `.dae`.
- Export string files to CSV.

Where it fits:

- Good archive browser and extraction tool.
- Useful for comparing live client TRE contents against build output.
- Useful for asset discovery and quick mesh/string extraction.

Where it does not fit:

- Not presented as an authoring/build pipeline.
- Mesh export to Collada is useful for inspection/conversion, but it is not equivalent to source-preserving Maya/Blender round-trip export.

Source/activity note: Swg.Explorer is public C# source on GitHub, but the last code push reported by GitHub API was 2013-03-16. Treat it as stale but useful.

#### `io_scene_swg_msh` Blender Add-on

The public `io_scene_swg_msh` project overlaps heavily with the local `tools/swg_blender` effort. Its documented features include:

- Import/export `.mgn` animated meshes.
- Import/export `.msh` static meshes.
- Import/export `.lod` static mesh LOD groups.
- Import/export `.pob` portalized objects.
- Import/export `.skt` skeletons.
- Generate `.apt`, `.sat`, and `.lmg`.
- Generate Blender materials from `.sht` shader files.
- Generate `.pob` hierarchies.
- Recommended external workflow with SIE, Krita, and NVIDIA Texture Tools Exporter.

This is the most relevant third-party comparison for the current Blender substitute work. It appears to already cover some areas still listed as local roadmap items, especially `.pob`, `.apt`, `.sat`, `.lmg`, and shader-derived materials.

Adoption options:

- Use it as a behavioral reference and test corpus comparison.
- Run the same stock assets through both add-ons and compare the client-visible result.
- Borrow concepts only after checking license and format compatibility.
- Avoid assuming exact parity: the local client's source and `MayaExporter` still define the authoritative runtime expectations.

Source/activity note: the repository is public and appears recently active, with a GitHub API last push of 2026-03-07. No SPDX license was exposed in the API result, so license review is required before code reuse.

#### Rust `swg_tre` and `swg_stf`

The Rust crates are interesting because they are pipeline components rather than GUI tools.

`swg_tre`:

- Reads and creates TRE files.
- Exposes `TreArchive` for reading and `TreWriter` for writing.
- Documents TRE v0005 structure: `TREE` magic, version, header, compressed records, compressed metadata, and name string blocks.
- Has explicit compression/decompression modules.

`swg_stf`:

- Reads and creates STF files.
- Describes itself as a building block for editing/updating files required by SWG servers and clients.
- Provides a programmatic path that could support tests, converters, and localization automation.

Where they fit:

- Good candidates for modern automated tests and small CLIs.
- Potentially easier to integrate into CI than old MFC/Qt tools.
- Useful for independent validation of local `TreeFileBuilder`, `TreeFileExtractor`, and `StringFileTool` behavior.

Where they need caution:

- Need exact compatibility tests against this client's TRE/STF variants.
- Need license review before vendoring or linking into project tooling. The repository was reported as AGPL-3.0 by GitHub API, which is a major integration consideration.
- TRE v0005 support may not cover every archive variant encountered in Restoration/CU/NGE-derived clients.
- Activity appears recent enough for evaluation: both crates were updated in November 2024, and their shared repository was last pushed on 2024-11-09.

#### `swb-repo-cli`

`swb-repo-cli` is a public command-line tree archive editor by James Webb/Sytner. It was already referenced in `docs/data-pipeline.html` as an external SWG archive tool.

Known from this pass:

- Public GitHub repository.
- Described as a command-line tree archive editor.
- GitHub API reported last push on 2025-01-14 and metadata update on 2025-08-10.
- No SPDX license was exposed by the API result in this pass.

Where it may fit:

- CLI-oriented TRE/archive operations.
- Possible modern comparison point for `TreeFileBuilder`, `TreeFileExtractor`, and WPS TRE Packager.
- Potentially useful in scripts if licensing and compatibility check out.

Open questions:

- Exact supported TRE versions and compression behavior.
- Whether it creates archives, edits existing archives, extracts, lists, or all of those.
- Whether it handles the same path normalization and MD5/integrity conventions this client expects.

#### SWGANH-Era Tooling

The SWGANH tools portal is mostly historical, but it confirms the same missing-tool categories:

- IFF Commando/IFF Editor.
- TRE Packer and beta TRE unpacker.
- Draft Schematic Editor.
- STF Editor and STF Manager.
- DataTable Editor.
- HeightMap Tool.
- Packet/testing/server utilities.

These tools are useful as a map of what the community historically needed. They are less immediately actionable unless working binaries/source are found and proven compatible.

#### ModTheGalaxy Ecosystem

ModTheGalaxy remains important as a discovery and distribution channel. Search results and public forum snippets show active/current SWG modding discussions, guides, resource listings, and tools. It is also where SIE, TRE tools, Utinni/Jawa Toolbox history, and many practical workflows are referenced.

For this project, ModTheGalaxy is best treated as:

- A place to discover missing tools and current workflows.
- A place to validate assumptions with experienced SWG modders.
- A likely source for binaries not mirrored in GitHub.
- Not a substitute for local compatibility testing.

## How the Pieces Most Likely Worked Together

### Static Prop Pipeline

Likely historical flow:

1. Model prop in Maya.
2. Assign SWG shader/material metadata and hardpoints.
3. Run `exportStaticMesh` through `MayaExporter`.
4. Exporter writes `.msh`, `.sht`, texture references, extents/hardpoints, and logs re-export metadata.
5. Preview in `Viewer` or client.
6. Reference the appearance from an object template `.tpf`.
7. Compile template with `TemplateCompiler`.
8. Generate `.rsp` with `TreeFileRspBuilder`.
9. Pack `.tre` with `TreeFileBuilder`.

Modern equivalent:

1. Model in Blender.
2. Use `tools/swg_blender` import/export.
3. Generate a dev bundle through `swg_pipeline.export_bundle`.
4. Mount loose bundle via client `searchPath0`.
5. Validate in-game.
6. Eventually pack via `TreeFileBuilder`.

### Character/Wearable/Animation Pipeline

Likely historical flow:

1. Author skeleton, skinned mesh, and animations in Maya.
2. Run `exportSkeleton`, `exportSkeletalMeshGenerator`, `exportSkeletalAnimation`, and possibly `exportSatFile`.
3. Export `.skt`, `.mgn`, `.ans`/`.kfat`, `.sat`, LAT/LMG/APT-related data, and re-export logs.
4. Use `AnimationEditor` for state hierarchy/logical animation table behavior.
5. Preview in `Viewer` and client.
6. Package through the TRE build chain.

Modern equivalent:

`tools/swg_blender` has imported and partially exported skeletal assets, animations, and SAT data, but the parity checklist still needs in-client validation and wider format coverage.

### Crafted Item/Schematic Pipeline

Likely historical flow:

1. Designer edits schematic datatable source.
2. `DataTableTool` produces a datatable IFF.
3. `ArmorExporterTool`, `WeaponExporterTool`, or `CoreWeaponExporterTool` reads the datatable IFF.
4. Tool writes server/shared `.tpf` for draft schematics and related object templates.
5. Tool invokes or relies on `TemplateCompiler` to compile `.tpf` into `.iff`.
6. String tables/localized names are updated through STF/localization tooling.
7. Build validates and packages output.

This is a good example of a pipeline that would be painful to replace with manual editing. The source code embeds column schemas, template class versions, path conventions, and generated server/shared template relationships.

### Quest/Conversation Pipeline

Likely historical flow:

1. Designer authors conversation in `SwgConversationEditor`.
2. Designer authors quest/space mission in `QuestEditor`, `SwgContentBuilder`, or `SwgSpaceQuestEditor`.
3. Tool output references scripts, string IDs, object templates, datatables, rewards, NPCs, mission templates, and space zones.
4. Datatables/templates/scripts compile through their own tools.
5. QA validates in a client/server environment, possibly with god/client support tools.
6. Assets package into TREs.

The repo has several overlapping quest/content tools, which may reflect different game eras, departments, or ground-vs-space workflows.

### World/Terrain Pipeline

Likely historical flow:

1. World builder authors procedural terrain in `TerrainEditor`.
2. Built structures and props come from Maya/exported appearances.
3. Snapshot/buildout placements are inspected with `WorldSnapshotViewer` or edited through internal tools not fully represented here.
4. Space zones are authored in `SwgSpaceZoneEditor`.
5. Runtime validation happens in client/god-client tools.

Modern equivalent:

Utinni/Jawa Toolbox is strongest here because it gives a live gizmo/object-browser workflow against the real client.

### Effects and Audio Pipeline

Likely historical flow:

1. VFX authors particles/swooshes/lightning/client effects in specialized editors.
2. Technical artists author or adjust shader/texture assets in `ShaderBuilder` and `TextureBuilder`.
3. Audio designers author sound templates in `SoundEditor`.
4. Designers reference these assets from object templates, datatables, scripts, or client effect definitions.
5. Build pipeline validates and packages.

## Practical Recommendations

### 1. Treat `MayaExporter` as the format specification for Blender work

`tools/swg_blender` should continue to use local C++ source and golden assets as the authority. Public Blender tools are useful comparisons, but this repo's client will care about this repo's exact IFF chunk structure, runtime search paths, and loader assumptions.

Near-term priority should be a small validation matrix:

- One static mesh with texture/shader/hardpoints.
- One large static mesh that crosses 16-bit index limits.
- One skeletal mesh plus skeleton.
- One skeletal animation.
- One SAT/wearable-style appearance.
- One collision/floor/extents test.
- One POB/building-style asset when implemented.

### 2. Keep Utinni focused on live validation and world editing

Utinni/Jawa Toolbox is extremely useful for:

- Mounting and validating loose assets.
- Reloading terrain/snapshots/UI without full client restarts.
- Visual world placement.
- Debugging appearance, skeleton, camera, graphics, and scene state.

It should not be expected to replace `DataTableTool`, `TemplateCompiler`, or `TreeFileBuilder`. Those are build-chain tools with deterministic file outputs.

### 3. Revive the packaging chain early

Even if assets are generated through Blender or new editors, the team still needs a reliable path from loose files to TREs:

- Confirm `TreeFileRspBuilder` can scan the current client search-path layout.
- Confirm `TreeFileBuilder` can pack archives consumed by the target client.
- Confirm `TreeFileExtractor` can round-trip test archives.
- Add a modern smoke test around a tiny archive with mesh, shader, texture, datatable, and object template.

### 4. Preserve generated-source workflows

Avoid hand-editing generated `.tpf` and datatable outputs when a generator exists. The schematic exporter tools encode assumptions about server/shared template pairs, naming, base templates, class versions, and crafting slot structures.

### 5. Make a buildability matrix

The repo has many old MFC, Qt, Maya SDK, DirectX, Perforce, Alienbrain, and external-library dependencies. A useful next research artifact would be a matrix:

- Builds today?
- Required SDK/runtime?
- Runtime starts?
- Opens sample data?
- Saves compatible output?
- Needed for current goals?
- Replace, revive, or archive?

### 6. Compatibility-test modern alternatives before adopting them

The WPS VS Code extensions, Rust crates, SIE, Swg.Explorer, and public Blender add-on are valuable, but they should be tested against this exact client before becoming official workflow pieces. Good smoke tests:

- Compile one `.tab` through WPS IFF Creator and this repo's `DataTableTool`; compare semantic load and, where possible, byte structure.
- Package a tiny archive through WPS TRE Packager and this repo's `TreeFileBuilder`; verify `TreeFileExtractor` and the client can read both.
- Round-trip one `.stf` through `swg_stf` or a modern STF editor and this repo's string tooling.
- Import/export the same `.msh`, `.mgn`, `.skt`, `.sat`, and `.pob` through `tools/swg_blender` and `io_scene_swg_msh`; compare loader behavior in this client.
- Use SIE and `ViewIff` on the same IFF files to compare chunk interpretation.

## Risk and Unknowns

Answered or narrowed:

- The major dependency buckets are now identified from project files and source: Win32/x86, MFC/ATL, Qt 3.3.4, DirectX 9/D3DX, DPVS, Miles, libxml2, zlib, PCRE, Perforce, Alienbrain, Maya SDK, ATI/NVIDIA helper libs, Oracle/ODBC, Mozilla embedding libraries, TinyXML, and parser-generation tooling.
- The highest-risk legacy tool is `MayaExporter`; the lowest-risk legacy tools are the core archive/data CLIs.
- The repo carries many third-party library directories, but buildability still depends on binary compatibility with the chosen compiler and whether all required headers/libs are intact.
- There are credible modern alternatives for datatables, STF, TRE, IFF inspection, and Blender asset workflows.

Still open:

- Some projects may be stubs, AppWizard leftovers, or incomplete.
- Some tool roles may have changed across SWG eras.
- Paths such as `data`, `dsrc`, `exe/shared/*.cfg`, and publish manifests matter; tool output may fail if the expected directory layout is absent.
- Public community tools may support formats or game versions that do not exactly match this client source.
- `tools/swg_blender` is active WIP. It has substantial coverage, but in-client validation and full `MayaExporter` parity are still the gate.
- Utinni/Jawa Toolbox remains sensitive to exact client executable versions and hook/RVA alignment.

## Bottom Line

The historical studio pipeline was probably a conventional but large MMO content pipeline: Maya and custom editors generated loose game data, compilers converted human-authored sources into binary IFF formats, validation tools checked the output, and TRE build tools packaged everything into layered archives. The current repo still contains most of the pieces needed to understand that pipeline.

For modern work, the best path is not to revive every old editor at once. Use `tools/swg_blender` to replace the Maya dependency for art assets, use Utinni/Jawa Toolbox for live client inspection and world editing, and revive the deterministic data/build tools that still define how content becomes shippable SWG runtime data.
