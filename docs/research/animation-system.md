# Deep-dive: Animation system — Granny or in-house?

> Question: Does SWG use RAD Game Tools' Granny animation middleware, or
> is the animation system in-house?

## Verdict: 100% in-house

The runtime animation system is bespoke SOE code. There is no Granny SDK in
the tree, no Granny library is linked, no Granny header is included by the
runtime. The naming, file layout, and `.iff` format are all SOE conventions.

The only mentions of "Granny" in the entire `src/` tree are two **dead
references inside the Viewer tool**:

* `src/engine/client/application/Viewer/src/win32/AnimationDialog.cpp:303`
  — a `//-- -TRF- fix, removed granny` comment followed by an `#if 0` block.
* `src/engine/client/application/Viewer/src/win32/ViewerDoc.cpp:2225` — a
  `CFileDialog` titled `"Select Granny File"` with a `.grn` filter, used
  to load a wearable item for preview in the Viewer (a developer tool,
  not the shipping client).

Both live in a desktop preview tool, both look like leftovers from an early
experiment that was removed. The note `-TRF-` is the initials of the engineer
who made the fix. Nothing else in `src/external/`, `plugin/`, the engine, or
the editors references RAD's Granny.

## Search evidence

| Search target | Pattern | Result |
| --- | --- | --- |
| Vendored 3rd-party tree | grep `granny\|Granny\|GRANNY` in `src/external/` | 0 matches |
| Engine | grep `granny\|Granny` in `src/engine/client/`, `src/engine/shared/`, `src/game/` | 0 matches |
| Engine, broad | grep `granny\|Granny` in `src/engine/client/` (deep search) | 2 matches — both in `Viewer` tool, both dead/wrapped in `#if 0` |
| Tools | grep `granny\|Granny` in `tools/` | 0 matches |
| Plugins | grep `granny\|Granny` in `plugin/` | 0 matches |
| Animation library response files | grep `granny\|Granny` in `clientSkeletalAnimation/build/win32/` | 0 matches |
| Maya exporter | scan filenames for `granny`, `.grn`, GR2 | 0 matches |
| Includes in `clientSkeletalAnimation` | `external/3rd/library` paths in `includePaths.rsp` | only `boost`, `dpvs`, `libxml2`, `miles`, `stlport453`, `archive`, `fileInterface`, `unicode`, `unicodeArchive` — **no Granny** |

## What the system actually is — two libraries

```
src/engine/client/library/clientAnimation/         (~14 .cpp,  generic playback engine)
src/engine/client/library/clientSkeletalAnimation/ (~94 .cpp, character-rig system)
```

### `clientAnimation` — generic "playback script" engine

Small, retained-mode library at
`src/engine/client/library/clientAnimation/src/shared/playback/`. Its job is
to schedule **timed actions** during gameplay events (not just bone
animation — also sound triggers, attached object spawns, message events).

Key types (source files in
`clientAnimation/src/shared/playback/`):

* `PlaybackScript`, `PlaybackScriptTemplate` — sequence of timed actions
  with priorities.
* `PlaybackAction`, `PlaybackActionTemplate` — base action class.
* `PassthroughPlaybackAction`
* `TimerPlaybackAction`, `TimerPlaybackActionTemplate`
* `WatcherAction`, `WatcherActionTemplate`
* `StopActionTemplate`
* `PriorityPlaybackScript`
* `PlaybackScriptManager` — runs them.

Plus:

* `clientAnimation/src/shared/core/SetupClientAnimation.{cpp,h}`
* `clientAnimation/src/shared/core/ConfigClientAnimation.{cpp,h}`
* `clientAnimation/src/shared/core/FirstClientAnimation.h` — pch.
* `clientAnimation/src/win32/FirstClientAnimation.cpp`

Think of it as the in-house equivalent of an animation/event director that
combat and special-attacks ride on top of.

### `clientSkeletalAnimation` — character rig system

The full skeletal animation, mesh, and controller system. Five conceptual
sub-folders under
`src/engine/client/library/clientSkeletalAnimation/src/shared/`:

#### `appearance/` — bind pose, bones, skinning

```
Skeleton.{cpp,h}                       BasicSkeletonTemplate.{cpp,h}
LodSkeletonTemplate.{cpp,h}            SkeletonTemplate.{cpp,h}
SkeletonTemplateList.{cpp,h}           SkeletonTransformNameMap.{cpp,h}

SkeletalAppearance2.{cpp,h}            SkeletalAppearanceTemplate.{cpp,h}
SkeletalMeshGenerator.{cpp,h}          SkeletalMeshGeneratorTemplate.{cpp,h}
LodMeshGeneratorTemplate.{cpp,h}       MeshGenerator.{cpp,h}
MeshGeneratorTemplate.{cpp,h}          MeshGeneratorTemplateList.{cpp,h}
CompositeMesh.{cpp,h}                  BasicMeshGeneratorTemplate.{cpp,h}

OcclusionZoneSet.{cpp,h}
OwnerProxyShader.{cpp,h}               OwnerProxyShaderTemplate.{cpp,h}
SoftwareBlendSkeletalShaderPrimitive.{cpp,h}
PoseModelTransform.h                   Transform*Template.{cpp,h}
```

The bind pose, bones, transform-name maps, LOD swapping, and the
skinned-mesh shader primitive.

#### `animation/` — clips, blenders, time-warpers, masks

```
SkeletalAnimation.{cpp,h}              SkeletalAnimationTemplate.{cpp,h}
SkeletalAnimationTemplateList.{cpp,h}

KeyframeSkeletalAnimation.{cpp,h}      KeyframeSkeletalAnimationTemplate.{cpp,h}
KeyframeSkeletalAnimationTemplateDef.h CompressedKeyframeAnimation.{cpp,h}
CompressedKeyframeAnimationTemplate.{cpp,h}

DirectionSkeletalAnimation.{cpp,h}     DirectionSkeletalAnimationTemplate.{cpp,h}
SpeedSkeletalAnimation.{cpp,h}         SpeedSkeletalAnimationTemplate.{cpp,h}
TimeScaleSkeletalAnimation.{cpp,h}     TimeScaleSkeletalAnimationTemplate.{cpp,h}
YawSkeletalAnimationTemplate.{cpp,h}

PriorityBlendAnimation.{cpp,h}         PriorityBlendAnimationTemplate.{cpp,h}
MaskedPrioritySkeletalAnimation.{cpp,h}
SinglePrioritySkeletalAnimation.{cpp,h}
BasePriorityBlendAnimation.{cpp,h}

ProxySkeletalAnimationTemplate.{cpp,h}
StringSelectorSkeletalAnimationTemplate.{cpp,h}
ActionGeneratorSkeletalAnimationTemplate.{cpp,h}

TransformMaskList.{cpp,h}
TransformAnimationResolver.{cpp,h}
TimedBlendSkeletalAnimation*           AnimationStateNameIdManager.{cpp,h}
AnimationPriorityMap.{cpp,h}
```

The animation containers, blenders, time-warpers, and masks.

#### `controller/` — state machine, "pick which clip to play"

```
TrackAnimationController.{cpp,h}                StateHierarchyAnimationController.{cpp,h}
LogicalAnimationTableTemplate.{cpp,h}            LogicalAnimationTableTemplateList.{cpp,h}
AnimationStateHierarchyTemplate*                AnimationStateHierarchyTemplateList.{cpp,h}
AnimationEnvironment.{cpp,h}                     AnimationNotification.{cpp,h}
AnimationHeldItemMapper.{cpp,h}                  AnimationPostureMapper.{cpp,h}
AnimationMessageActionTemplate.{cpp,h}
AnimationCallback*                               CallbackAnimationNotification.{cpp,h}

Editable*: EditableAnimationState{,HierarchyTemplate}.{cpp,h}
           EditableBasicAnimationAction.{cpp,h}
           EditableMovementAnimationAction.{cpp,h}

Logical*Pattern*                                StateHierarchyAnimationController*
TransformAnimationController.{cpp,h}
```

The state-machine layer that picks which clip to play, plus the `Editable*`
variants used by the AnimationEditor tool.

#### `modifier/` — runtime per-bone overrides (IK-lite)

```
LookAtTransformModifier.{cpp,h}        TargetPitchTransformModifier.{cpp,h}
AlignmentTransformModifier.{cpp,h}     ZeroTransformModifier.{cpp,h}
TransformModifier.{cpp,h}
```

Head turning, gun aim pitch, weapon alignment.

#### `playback/` — bridges to clientAnimation

```
AnimationMessage*                      ShowAttachedObjectAction.{cpp,h}
                                       ShowAttachedObjectActionTemplate.{cpp,h}
```

Bridges `clientAnimation`'s playback script to skeletal animations (e.g. fire
a particle from the model when a clip reaches frame X, attach an object
during a clip, etc.).

#### `core/` and `debug/`

```
core/CharacterLodManager.{cpp,h}
core/ConfigClientSkeletalAnimation.{cpp,h}
core/SetupClientSkeletalAnimation.{cpp,h}
core/FirstClientSkeletalAnimation.h
debug/SkeletalAnimationDebugging.{cpp,h}
```

#### `batch/`

```
FullGeometrySkeletalAppearanceBatchRenderer.{cpp,h}
SkeletalAppearance*BatchRenderer.{cpp,h}
```

The batched render path for many skinned appearances at once.

### Subsystem registry — `SetupClientSkeletalAnimation::install()`

`src/engine/client/library/clientSkeletalAnimation/src/shared/core/SetupClientSkeletalAnimation.cpp`
calls 50+ `Foo::install()` functions. Selected highlights:

```cpp
ConfigClientSkeletalAnimation::install();
SkeletalAnimationDebugging::install();
AnimationPriorityMap::install(cms_defaultPriorityMapFileName);
TrackAnimationController::install();
StateHierarchyAnimationController::install();
AnimationStateHierarchyTemplateList::install();
LogicalAnimationTableTemplate::install();
LogicalAnimationTableTemplateList::install();
AnimationEnvironment::install();

Skeleton::install();
SkeletonTemplateList::install();
BasicSkeletonTemplate::install();
LodSkeletonTemplate::install(data.allowLod0Skipping);

TransformMaskList::install();

AnimationNotification::install();
CallbackAnimationNotification::install();
SkeletalAnimationTemplateList::install();
SkeletalAnimationTemplate::install();
SkeletalAnimation::install();
BasePriorityBlendAnimation::install();
SpeedSkeletalAnimation::install();
CompressedKeyframeAnimationTemplate::install();
CompressedKeyframeAnimation::install();
KeyframeSkeletalAnimationTemplate::install();
KeyframeSkeletalAnimation::install();
ProxySkeletalAnimationTemplate::install();
DirectionSkeletalAnimationTemplate::install();
DirectionSkeletalAnimation::install();
PriorityBlendAnimationTemplate::install();
PriorityBlendAnimation::install();
SinglePrioritySkeletalAnimation::install();
SpeedSkeletalAnimationTemplate::install();
StringSelectorSkeletalAnimationTemplate::install();
TimeScaleSkeletalAnimationTemplate::install();
TimeScaleSkeletalAnimation::install();
ActionGeneratorSkeletalAnimationTemplate::install();
YawSkeletalAnimationTemplate::install();

OcclusionZoneSet::install();
CompositeMesh::install();
SkeletalAppearanceTemplate::install();
SkeletalAppearance2::install();
SoftwareBlendSkeletalShaderPrimitive::install();

MeshGeneratorTemplateList::install();
SkeletalMeshGenerator::install();
SkeletalMeshGeneratorTemplate::install();
LodMeshGeneratorTemplate::install(data.allowLod0Skipping);

AnimationMessageActionTemplate::install();
ShowAttachedObjectAction::install();
ShowAttachedObjectActionTemplate::install();

AnimationStateNameIdManager::install();

// @todo -TRF- Someday make non-editable, more efficient versions and only
//             use these editable ones in the AnimationEditor.
EditableBasicAnimationAction::install();
EditableMovementAnimationAction::install();
EditableAnimationState::install();
EditableAnimationStateHierarchyTemplate::install();

AnimationHeldItemMapper::install("animation/held_item_map.iff");
AnimationPostureMapper::install("animation/posture_map.iff");

OwnerProxyShaderTemplate::install();
OwnerProxyShader::install(data.stitchedSkinInheritsFromSelf);

FullGeometrySkeletalAppearanceBatchRenderer::install();

LookAtTransformModifier::install();
TargetPitchTransformModifier::install();

CharacterLodManager::install();
```

All `Foo::install()` — no third-party `GrFoo_initialize()` /
`GrnRuntime_Init()` style calls. This is a fully owned subsystem.

### Three "setup data" presets

```cpp
SetupClientSkeletalAnimation::setupGameData(Data &data);    // shipping client
SetupClientSkeletalAnimation::setupToolData(Data &data);    // editor tools
SetupClientSkeletalAnimation::setupViewerData(Data &data);  // Viewer tool
```

The viewer is the only one that sets `stitchedSkinInheritsFromSelf=true`.

## Data formats — also in-house

`src/engine/client/library/clientSkeletalAnimation/src/shared/animation/KeyframeSkeletalAnimationTemplate.h`
declares the on-disk record types directly:

```cpp
struct RealKeyData {
public:
    RealKeyData();
    RealKeyData(float frameNumber, float keyValue);

public:
    float  m_frameNumber;
    float  m_keyValue;
    float  m_oneOverDistanceToNextKeyframe;
};

struct QuaternionKeyData {
public:
    QuaternionKeyData();
    QuaternionKeyData(float frameNumber, const Quaternion &rotation);

public:
    float       m_frameNumber;
    Quaternion  m_rotation;
    float       m_oneOverDistanceToNextKeyframe;
};

struct VectorKeyData {
    // ...
};
```

So animation clips are SOE's own keyframe records (per-channel
real / quaternion / vector tracks), serialised through SOE's own IFF reader
and writer. The compressed variant (`CompressedKeyframeAnimation`) is a
separate SOE compression scheme.

Two lookup files referenced by `SetupClientSkeletalAnimation`:

* `animation/priority_map.iff` — defines layer priorities
* `animation/held_item_map.iff` — maps weapon types to grip animations
* `animation/posture_map.iff` — maps player postures (stand / crouch / prone)
  to animation sets

These are loaded with `TreeFile::open` like everything else.

## Maya is the authoring tool — confirmation

`src/engine/client/application/MayaExporter/src/` writes the **SOE-format**
files directly out of Maya:

```
ExportSkeleton.{cpp,h}
ExportKeyframeSkeletalAnimation.{cpp,h}
SkeletonTemplateWriter.{cpp,h}
LodSkeletonTemplateWriter.{cpp,h}
KeyframeSkeletalAnimationTemplateWriter.{cpp,h}
MayaAnimationList.{cpp,h}
AddAnimationCommand.{cpp,h}
DeleteAnimationCommand.{cpp,h}
VisitAnimationCommand.{cpp,h}
AnimationMessageCollector.{cpp,h}
MayaAnimatingTextureShaderTemplateWriter.{cpp,h}
```

The pipeline is: **Maya scene → SOE MayaExporter plug-in → `.iff` skeleton +
`.iff` keyframe animation → packed into `.tre`**. There's no `.gr2` (Granny)
intermediate, no Granny exporter or importer in the runtime path.

## Why it matters (porting/maintenance perspective)

* **No Granny licence problem.** This is a clean, owned format. The whole
  skeletal/animation stack can be modified freely because SOE wrote it.
* **You need Maya 4–8 with the SOE exporter** to author new animations from
  scratch. The exporter source is in `MayaExporter/`; its build pulls in real
  Maya SDK headers from `external/3rd/library/maya4..maya8`. For modern Maya
  you'd port the exporter.
* **Existing community emulators target the same `.iff` formats**, so any
  custom content emitted by the SOE Maya exporter still loads in retail
  clients and emulator clients alike.
* **There's no third-party DLL for the runtime to chase down.** Unlike Bink /
  Miles / Vivox / DPVS, the animation system requires no external SDK to be
  linked or distributed.

## The one Granny artefact left over

The `.grn` file dialog in `Viewer` looks like an early development
experiment (probably circa 2001–2002) where someone tried using Granny as
an import path during pre-production prototyping, and it was abandoned. The
dead `#if 0` block and the `-TRF- removed granny` comment confirm a deliberate
rip-out. It would only resurface if you tried to use Viewer's "Wear Item"
preview menu — and even then, since no Granny runtime is linked, it would
just fail to open the file.

## TL;DR

| Question | Answer |
| --- | --- |
| Is Granny present at runtime? | **No.** |
| Is Granny present anywhere? | Two dead references inside the Viewer tool. |
| What is the animation system, then? | Two in-house C++ libraries — `clientAnimation` (~14 .cpp) and `clientSkeletalAnimation` (~94 .cpp). |
| What format are the animation files? | SOE `.iff` (Interchange File Format) chunked binary, with per-channel keyframes (`RealKeyData`, `QuaternionKeyData`, `VectorKeyData`). |
| Where do animations come from? | Maya → SOE MayaExporter (`ExportKeyframeSkeletalAnimation` etc.) → `.iff` → `.tre`. |
| Licensing implication | None — fully SOE-owned. Porting and modification are free of middleware constraints. |
