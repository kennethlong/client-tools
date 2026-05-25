# Deep-dive: Foliage / vegetation — SpeedTree or in-house?

> Question: Is there a SpeedTree dependency? Or is foliage / tree rendering
> built in-house?

## Verdict: No SpeedTree. Pure in-house.

Same kind of result as the Granny investigation — and even cleaner: there
isn't a single dead reference to SpeedTree, IDV, `.spt`, `.srt`,
"Branch Geometry", or any other middleware-foliage tell.

## Search evidence

| Search target | Pattern | Result |
| --- | --- | --- |
| Vendored SDK directory | grep `SpeedTree\|speedtree\|IDV` in `src/external/` | 0 matches |
| Engine code | grep `SpeedTree\|speedtree` in `src/engine/client/`, `src/engine/shared/`, `src/game/` | 0 matches |
| Tools | grep `SpeedTree\|speedtree\|IDV` in `tools/` | 0 matches |
| Plugins | grep `SpeedTree\|speedtree` in `plugin/` | 0 matches |
| Patcher manifests (every shipped data file) | grep `\.spt\|\.srt\|SpeedTree` in `publish/` | 0 matches |
| Filename scan | filenames containing `speed`, `idv`, `spt`, `srt` in `exe/`, `plugin/`, `tools/` | only `TreeFile`/`xul-tree`/`patchTree` style hits — all unrelated |
| Tree-named subdirectories | scan `clientTerrain`, `sharedTerrain`, `clientObject`, `external/3rd/library` | 0 directories with `tree`/`foliage`/`flora` in `external/` (note: SOE calls vegetation "flora") |

There is no `external/3rd/library/speedtree`, no `IDV` license blob, no
`CSpeedTreeRT*` type, no `.spt` loader. **It's not in the tree.**

## Chronology

The foliage code is all stamped *"copyright 2000, verant interactive"* by
`asommers` (Andy Sommers — the studio's terrain engineer). IDV Inc. didn't
release SpeedTree until 2003 — *after* this system was already shipping. SWG
was simply too early to ride the SpeedTree wave.

## What it actually is — the "Flora" system

SOE's term for all vegetation (trees, bushes, grass, ground scatter) is
**flora**, sometimes "radial flora" because it's evaluated in a sliding
radius around the camera. The code lives in two libraries:

```
src/engine/shared/library/sharedTerrain/src/shared/   — generator, affectors, FloraGroup
src/engine/client/library/clientTerrain/src/shared/   — runtime managers, rendering
```

Two parallel pipelines depending on whether the foliage is real geometry or
just a camera-facing card.

### Static flora — real meshes

`src/engine/client/library/clientTerrain/src/shared/flora/ClientStaticRadialFloraManager.cpp`
(~23 KB).

Header:
```cpp
//===================================================================
//
// ClientStaticRadialFloraManager.cpp
// asommers 9-11-2000
//
// copyright 2000, verant interactive
//
//===================================================================
```

Core path:
```cpp
FileName aptName(FileName::P_appearance, baseName, "apt");
if (TreeFile::exists(aptName))
    m_appearanceTemplate = AppearanceTemplateList::fetch(aptName);
else {
    FileName satName(FileName::P_appearance, baseName, "sat");
    if (TreeFile::exists(satName))
        m_appearanceTemplate = AppearanceTemplateList::fetch(satName);
    else {
        FileName prtName(FileName::P_appearance, baseName, "prt");
        if (TreeFile::exists(prtName))
            m_appearanceTemplate = AppearanceTemplateList::fetch(prtName);
        else {
            DEBUG_WARNING(true, ("ClientStaticRadialFloraManager: could not "
                                 "open appearance template %s",
                                 staticFloraData.familyChildData->appearanceTemplateName));
            m_appearanceTemplate = AppearanceTemplateList::fetch(
                "appearance/defaultappearance.apt");
        }
    }
}
```

Static flora is just an ordinary SWG **`Appearance`** loaded from an
`.apt` / `.sat` / `.prt` template — i.e. the same kind of mesh resource a
building or a vehicle or a creature uses. Trees are skinned/static meshes
authored in **Maya**, exported through the in-house `MayaExporter`,
packaged as `.iff` mesh templates, and fetched here at runtime. There is no
special "tree" appearance type.

Two affector subclasses partition collision behaviour
(`src/engine/shared/library/sharedTerrain/src/shared/generator/AffectorFloraStatic.h`):

* `AffectorFloraStaticCollidableConstant` — collidable trees (you can
  collide with them).
* `AffectorFloraStaticNonCollidableConstant` — non-collidable scatter
  (you walk through them).

### Dynamic flora — billboard quads

`src/engine/client/library/clientTerrain/src/shared/flora/ClientDynamicRadialFloraManager.cpp`
(~35 KB).

This is the grass / small bushes / scatter detail layer. It's not even a
mesh — it's vertex-buffer quads built fresh each frame. The render loop
(around line 600+):

```cpp
const Vector wvec(matrix[0][0]*width,  matrix[1][0]*width,  matrix[2][0]*width);
const Vector hvec(matrix[0][1]*height, matrix[1][1]*height, matrix[2][1]*height);

q.v0.pos = offset       - wvec;          // bottom-left
q.v1.pos = offset + hvec - wvec + sway;  // top-left  (swayed)
q.v2.pos = offset + hvec + wvec + sway;  // top-right (swayed)
q.v3.pos = offset       + wvec;          // bottom-right
```

A two-triangle camera-facing quad with the top two vertices displaced by a
global "sway" vector. The whole patch is built into one big
`m_vertexBuffer.lock()/unlock()` per render bucket and drawn as one batch.

```cpp
m_vertexBuffer.unlock();
Graphics::setObjectToWorldTransformAndScale(Transform::identity, Vector::xyz111);
Graphics::setVertexBuffer(m_vertexBuffer);
Graphics::setIndexBuffer(*ms_indexBuffer);
```

### Wind animation — `sin()`

That's it. No multi-axis wind, no per-branch motion, no leaf-card flutter.
The whole "wind" system is one global angle the manager updates each frame,
multiplied by a per-flora-child `period` and `displacement`:

```cpp
const double swayAngle = m_owner.getFloraSwayAngle();
// ...
Vector sway;
if (floraData.familyChildData->shouldSway) {
    double angle = double(floraData.familyChildData->period) *
                   (swayAngle + double(radialNode->getDeltaDirection()));
    sway = floraData.familyChildData->displacement *
           float(sin(angle)) * Vector::unitX;
} else {
    sway = Vector::zero;
}
```

For static flora (`ClientStaticRadialFloraManager.cpp`), the same idea but
rotating the *J basis vector* (the model's up axis), so the whole tree
leans:

```cpp
const Vector vi = m_object->getObjectFrameI_w();
const Vector vj = m_oldJ +
    (fcd.shouldSway
       ? fcd.displacement *
         m_object->rotate_w2o(vi) *
         sinf(fcd.period * deltaDirection)
       : Vector::zero);
const Vector vk = m_object->getObjectFrameK_w();
```

This is the "tree wobble" you remember from SWG. None of SpeedTree's signature
features (per-branch wind, leaf-card flutter, dynamic LOD billboards,
level-of-detail morph) exist here.

## Where rendering and placement actually live

```
sharedTerrain (generator side, runs at terrain-bake / sample time)
├── TerrainGenerator           — top-level driver
├── FloraGroup                 — registry of "flora families" → list of valid appearance templates
├── RadialGroup                — radial flora rules (distance, density, clear zones)
├── ShaderGroup, EnvironmentGroup, BitmapGroup, FractalGroup, ColorRamp256
└── Affector*                  — the rules that paint flora-family ids onto the planet
    ├── AffectorFloraStaticCollidableConstant
    ├── AffectorFloraStaticNonCollidableConstant
    └── AffectorFloraDynamic

clientTerrain (runtime, runs in the camera's vicinity)
├── ClientProceduralTerrainAppearance        — owns the planet
├── ClientRadialFloraManager                 — base sliding-radius manager
├── ClientStaticRadialFloraManager           — instances real Appearances
└── ClientDynamicRadialFloraManager          — emits billboard quads

clientObject + sharedObject
└── Appearance / AppearanceTemplate          — generic mesh subsystem the static flora uses
```

The placement is **procedural**: the artist doesn't drop individual trees.
They paint flora-family-ids into a 2-D bitmap layer with the
**TerrainEditor** tool (`exe/win32/TerrainEditor_o.exe`), and at runtime the
radial flora managers stream what's near the camera.

### Affector / generator listing

Files in `src/engine/shared/library/sharedTerrain/src/shared/generator/`:

```
Affector.{cpp,h}                        AffectorColor.{cpp,h}
AffectorEnvironment.{cpp,h}             AffectorExclude.{cpp,h}
AffectorFloraDynamic.{cpp,h}            AffectorFloraStatic.{cpp,h}
AffectorHeight.{cpp,h}                  AffectorPassable.{cpp,h}
AffectorRibbon.{cpp,h}                  AffectorRiver.{cpp,h}
AffectorRoad.{cpp,h}                    AffectorShader.{cpp,h}

Array2d.h            BitmapGroup.{cpp,h}        Boundary.{cpp,h}
ColorRamp256.{cpp,h} CoordinateHash.{cpp,h}     EnvironmentGroup.{cpp,h}
FastKeyList.h        FastList.h                 Feather.h
Filter.{cpp,h}       FloraGroup.{cpp,h}         FractalGroup.{cpp,h}
HeightData.{cpp,h}   RadialGroup.{cpp,h}        ShaderGroup.{cpp,h}

TerrainGenerator.{cpp,h}                TerrainGeneratorLoader.{cpp,h}
TerrainGeneratorType.def                TerrainGeneratorType.h
TerrainModificationHelper.{cpp,h}
```

### `FloraGroup` — the family registry

`src/engine/shared/library/sharedTerrain/src/shared/generator/FloraGroup.h`:

```cpp
//===================================================================
//
// FloraGroup.h
// asommers 9-17-2000
//
// copyright 2000, verant interactive
//
//--
//
// A FloraGroup contains lists of families of meshes organized by id.
// A flora family is a list of valid appearances for a specified id. The flora
// family also stores the name of the flora, and its appearanceTemplate.
// The flora group will return created appearances for use with the terrain
// system. In the event that requested flora family id does not
// exist, the flora group will return a [clearly visible] default flora.
// Families will only create flora when they are chosen.
//
//===================================================================

class FloraGroup {
public:
    struct Info {
        Info();
        ~Info();
        int   getFamilyId() const;
        void  setFamilyId(int familyId);
        float getChildChoice() const;
        void  setChildChoice(float childChoice);
    private:
        uint8 m_familyId;
        uint8 m_childChoice;
    };

    struct FamilyChildData {
    public:
        int          familyId;
        float        weight;
        const char*  appearanceTemplateName;
        bool         shouldSway;
        float        period;
        float        displacement;
        bool         alignToTerrain;
        bool         shouldScale;
        float        minimumScale;
        float        maximumScale;
    public:
        FamilyChildData();
        ~FamilyChildData();
    };

private:
    void load_0001(Iff& iff);
    void load_0002(Iff& iff);
    void load_0003(Iff& iff);
    void load_0004(Iff& iff);
    void load_0005(Iff& iff);
    void load_0006(Iff& iff);
    void load_0007(Iff& iff);
    void load_0008(Iff& iff);
    // ...
};
```

Versioned loaders inside a single class (`load_0001` … `load_0008`) is a
giveaway that the format was hand-rolled and evolved across patches — not
adopted from a vendor's SDK.

## Data formats

Every flora-related authoring artefact rides on the SWG **`.iff` chunk
format** — same chunk reader/writer as everything else in the engine.
There is no `.spt`, `.srt`, or proprietary middleware container.

| File | Contents |
| --- | --- |
| `*.trn` (planet terrain) | An `.iff` whose chunks include `ShaderGroup`, `FloraGroup`, `RadialGroup`, `EnvironmentGroup`, `FractalGroup`, `BitmapGroup`, plus a list of `Affector` records. |
| `FloraGroup` chunk | Family id → name, color, list of `FamilyChildData { appearanceTemplateName, shouldSway, period, displacement, alignToTerrain, shouldScale, minimumScale, maximumScale }`. Has eight versioned loaders (`load_0001`..`load_0008`) — clear in-house iteration history. |
| Tree mesh itself | `.apt` (appearance template) → fetched via `AppearanceTemplateList::fetch`, ultimately resolves to a `.msh` / `.mgn` / similar SWG mesh `.iff`. Authored in Maya via the in-house `MayaExporter`. |
| Tree texture | `.dds` referenced by the appearance's shader template. |
| Tree placement | Encoded as flora-family ids in `Array2d<FloraGroup::Info>` per terrain chunk — derived at terrain-bake from the painted bitmap layers. |

## Authoring tools (all in-house)

| Tool | Role |
| --- | --- |
| **TerrainEditor** (`src/engine/client/application/TerrainEditor`) | The artists' planet-authoring app. `TerrainGeneratorHelperFlora.cpp` and `TerrainGeneratorHelperRadial.cpp` are the dialogs for editing FloraGroup families and RadialGroup rules. Outputs `.trn`. |
| **MayaExporter** (`src/engine/client/application/MayaExporter`) | Exports the actual tree/bush meshes from Maya 4–8 to SWG `.iff` mesh templates. No SpeedTree intermediate. |
| **Viewer** (`src/engine/client/application/Viewer`) | Previews any appearance template, including trees. |

## TL;DR

| Question | Answer |
| --- | --- |
| Is there a SpeedTree dependency? | **No.** Zero references anywhere in source, tools, plugins, or patcher manifests. |
| Where does tree rendering live? | `src/engine/client/library/clientTerrain/src/shared/flora/` (Client{Static,Dynamic}RadialFloraManager). |
| Where does tree *placement* live? | `src/engine/shared/library/sharedTerrain/src/shared/generator/` (FloraGroup, AffectorFloraStatic*, AffectorFloraDynamic, RadialGroup), driven by **TerrainEditor** authoring. |
| What are the data formats? | All standard SWG `.iff` chunks — no `.spt`/`.srt`. Trees are normal `.apt`/`.sat`/`.prt` appearance templates. Planet data is `.trn`. |
| Static vs dynamic foliage | Static = real meshes via `AppearanceTemplate`. Dynamic = camera-facing billboard quads in a streamed VB. |
| Wind | Single global `sin(angle)` "sway" applied to billboard tops or to the model's up-axis. No per-branch wind. |
| Why no SpeedTree? | The flora system was written in **September 2000** by Verant Interactive's terrain team. SpeedTree wasn't released until ~2003. SWG simply predates the middleware. |

If you're porting or modernising the foliage, you're working with a clean
owned codebase — no licensing entanglement, no vendor format. That's the
same answer Granny got.
