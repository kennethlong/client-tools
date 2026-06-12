I'm in **Ask mode**, so I can't write `.planning/research/RENDER-PATH-TREE-cursor.md` to disk. Switch to **Agent mode** if you want that file created verbatim. Below is the full deliverable content.

---

# RENDER-PATH-TREE (Cursor) — Skinned / Skeletal Geometry

**Repo:** `D:\Code\swg-client-v2`  
**Scope:** Every CPU→GPU path for skeletal/skinned (and hardpoint-attached rigid) meshes  
**Notation:** `shared` = `clientSkeletalAnimation` / `clientGraphics` (identical D3D9/D3D11); **plugin divergence** columns at `Graphics::` → `Gl_api` → plugin.

---

## 0. Top-level scene entry

```
Camera::beginScene()
  ObjectListCamera::beginScene()                    [ObjectListCamera.cpp:111-114]
    ShaderPrimitiveSorter::pushCell(envTex,…)       [ShaderPrimitiveSorter.cpp:1334+]

Camera::drawScene()
  ObjectListCamera::drawScene()                     [ObjectListCamera.cpp:126-143]
    ObjectWatcherList::render(excluded)             [per object list]
      Object::render() … Appearance::objectListCameraRender()  [Appearance.cpp:300-305]
        SkeletalAppearance2::render()               [SkeletalAppearance2.cpp:1508+]

Camera::endScene()
  ObjectListCamera::endScene()                      [ObjectListCamera.cpp:148-151]
    ShaderPrimitiveSorter::popCell()                [ShaderPrimitiveSorter.cpp:1400-1413]
      Phase::popCell()                              [ShaderPrimitiveSorter.cpp:856-862]
        Phase::sort()
        Phase::draw()                               [ShaderPrimitiveSorter.cpp:689-825]
          entry.shaderPrimitive.prepareToDraw()     [:725]
          Graphics::setStaticShader(staticShader,i) [:746]
          entry.shaderPrimitive.draw()              [:781]
```

**Alternate cell paths:** `RenderWorld.cpp:968/1011`, `RenderWorldCommander.cpp:211/217`, etc. — same `ShaderPrimitiveSorter::Phase::draw()` contract.

---

## 1. SkeletalAppearance2::render — routing hub

**File:** `SkeletalAppearance2.cpp:1508-1895`

| Step | Method / condition | File:line |
|------|-------------------|-----------|
| LOD select | `calculateDisplayLodIndex(screenDiameterFraction, …)` | `:1631` |
| Skip render | `screenDiameterFraction <= getNoRenderScreenFraction()` | `:1553-1567` |
| Rebuild gate | `rebuildOrAdjustDisplayLodIndex()` | `:1660` |
| **Batch selector** | `useBatcher = true` when `!s_disableBatcher && fade==FS_notFading && !hologram && !blueGlowie` AND screen fraction thresholds `:1707-1722` | `:1706-1723` |
| **Skinning mode** | `mustUseSoftSkinning()` → force soft; else screen fraction ladder | `:1735-1742` |
| | `≤ getNoSkinningScreenFraction()` → `SM_noSkinning` | `:1737-1738` |
| | `≤ getHardSkinningScreenFraction()` OR `m_forceHardSkinningEnabled` → `SM_hardSkinning` | `:1739-1740` |
| | else → `SM_softSkinning` | `:1741-1742` |
| Per-primitive | `sbsShaderPrimitive->setSkinningMode(skinningMode)` | `:1769` |
| Non-batch | `ShaderPrimitiveSorter::add(*shaderPrimitive)` or `addWithAlphaFadeOpacity` | `:1772-1777` |
| Batch | `FullGeometrySkeletalAppearanceBatchRenderer::getInstance()->submit(*this)` | `:1787-1789` |
| Attachments | `transformToWorld = o2w * jointToRoot[hardpoint]`; `attachedAppearance->objectListCameraRender()` | `:1827-1835` |

**Shader primitive construction (load time, not per frame):**  
`SkeletalMeshGeneratorTemplate.cpp:3445` → `new SoftwareBlendSkeletalShaderPrimitive(...)`  
Optional wrap: `:3508-3511` → `new TextureRendererShaderPrimitive(initialShaderPrimitive)` replaces vector entry.

---

## 2. ShaderPrimitiveSorter::Phase::draw — per-primitive GPU prep

```
Phase::draw()                                           [ShaderPrimitiveSorter.cpp:689]
  ShaderPrimitiveSorter::setLights(entry.lightBitSet)   [:718]
  shaderPrimitive.prepareToDraw()                       [:725]  ← SKIN + VB/IB BIND
  for each shader pass:
    Graphics::setStaticShader(staticShader, pass)       [:746]
    shaderPrimitive.draw()                              [:781]  ← DRAW CALLS
```

**TextureRenderer wrapper:** `TextureRendererShaderPrimitive::prepareToDraw()` → `m_shaderPrimitive->prepareToDraw()` [`TextureRendererShaderPrimitive.cpp:241-243`]; same for `draw()` `:248-250`. **No alternate skin path.**

---

## 3. SoftwareBlendSkeletalShaderPrimitive — full call graph

### 3.1 Class / init (determines VB layout)

```
SoftwareBlendSkeletalShaderPrimitive::install()
  ms_useMultiStreamVertexBuffers = (maxStreamCount>1) && supportsStreamOffsets && !disableCfg
  [SoftwareBlendSkeletalShaderPrimitive.cpp:340-348]

SoftwareBlendSkeletalShaderPrimitive::_initialize(mesh, shaderIndex, multiStream)
  buildIndexBufferAndRenderCommands()                   [:1166, :1459+]
  m_dynamicStream = new DynamicVertexBuffer(dynamicFormat)  [:1245-1274]
  fillConstantVertexBufferData() into static or system VB   [:1392-1411]
  m_vertexBufferVector = VertexBufferVector(static+dynamic) OR (dynamic only)  [:1401-1416]
```

**Runtime latch (typical D3D11 session):** `ms_useMultiStreamVertexBuffers=0` (see `gap6_latch.log` / `:350-351`).

**vSize=36 layout (non-dot3, color, 1×2D UV, single-stream):**  
`dynamicFormat`: position+normal+color0+1 TC set dim 2 → **12+12+4+8 = 36** [`:1248-1261`, `:1646-1707`]

### 3.2 prepareToDraw() — shared skeleton path

```
SoftwareBlendSkeletalShaderPrimitive::prepareToDraw()   [SoftwareBlendSkeletalShaderPrimitive.cpp:483]
│
├─ Graphics::setObjectToWorldTransformAndScale(
│     m_appearance.getTransform_w(), Vector::xyz111)    [:490-491]
│     → Graphics.cpp → ms_api->setObjectToWorldTransformAndScale
│     → D3D9: Direct3d9.cpp (cbuffer/world matrix path)
│     → D3D11: Direct3d11_StateCache (slot-0 shadow / W matrix)
│
├─ Skeleton &skeleton = m_appearance.getSkeleton(m_lodIndex)     [:559]
├─ transformCount = skeleton.getTransformCount()                   [:562]
├─ transformArray = skeleton.getBindPoseModelToRootTransforms()  [:563]
│     → Skeleton::calculateBindPoseModelToRootTransforms()         [Skeleton.cpp:1498]
│        jointToRoot * bindPoseModelToJoint per joint              [:1540-1542]
│        **SPACE: bind-pose model → skeleton-root (OBJECT / model space)**
│
└─ [STREAM SUB-PATH — see §3.3]
    Graphics::setVertexBuffer(*m_vertexBufferVector)               [:819]
    Graphics::setIndexBuffer(*m_indexBuffer)                       [:823]
```

### 3.3 Stream sub-paths (prepareToDraw `:661-783`)

**Selector:** `ms_useMultiStreamVertexBuffers` at `:661`

#### A) Multi-stream ON (`:661-743`) — *typically OFF at runtime*

```
m_systemStream->beginWriteOnly()                        [:667]
  skinData(transformCount, transformArray, writeIterator)  [:668]
m_dynamicStream->lock(m_vertexCount)                    [:737]
  outIt.copy(systemStream→dynamic, m_vertexCount)       [:739-740]
m_dynamicStream->unlock()                               [:742]
```

- **VB:** skin → `SystemVertexBuffer`; copy → `DynamicVertexBuffer` (pos/normal[/dot3] only in dynamic)
- **Bind:** `VertexBufferVector(staticStream, dynamicStream)` [`:1401`]
- **Copy:** yes, system→dynamic

#### B) Single-stream, `m_systemStream` exists (**DEFAULT** — `:761-782`)

```
m_systemStream->beginWriteOnly()                        [:768]
  skinData(...)                                         [:769]
m_dynamicStream->lock(m_vertexCount)                    [:776]
  outIt.copy(systemStream→dynamic, full count)          [:778-779]
m_dynamicStream->unlock()                               [:781]
```

- **VB:** skin into `SystemVertexBuffer`; **memcpy copy** to `DynamicVertexBuffer` via `VertexBufferWriteIterator::copy` [`VertexBufferIterator.h:878-884`]
- **Bind:** `VertexBufferVector(*m_dynamicStream)` only [`:1416`]
- **Copy:** **YES** (primary NPC path)

#### C) Single-stream, `!m_systemStream` (`:748-759`) — *init always creates systemStream in non-multi `:1409`; likely dead*

```
m_dynamicStream->lock(m_vertexCount)                    [:754]
  skinData direct to dynamic                            [:756-757]
m_dynamicStream->unlock()                               [:759]
```

- **Copy:** no

#### D) Every-other-frame skip (`skinData :1717-1736`)

```
if (m_hasBeenSkinned && m_everyOtherFrameSkinningEnabled && parity) return;  [:1734-1736]
```

- **Effect:** no skin, no copy update that frame (stale dynamic VB)

#### E) SM_noSkinning re-skip (`skinData :1739-1742`)

```
if (m_skinningMode==SM_noSkinning && m_hasBeenSkinned) return;
```

- **Effect:** keeps prior skin in systemStream; copy still runs in prepareToDraw

### 3.4 skinData() — fill dispatch (`:1712-1825`)

```
skinData(transformCount, transformArray, iterator)
│
├─ SM_softSkinning → fillVertexBuffer(...)              [:1747-1749]
│
└─ SM_hardSkinning / SM_noSkinning                       [:1752-1821]
     fill_vb_work::construct(...)
     if m_hasDot3Vector:
       fillDot3VertexBufferHard_sse / fillDot3VertexBufferHard  [:1767-1792]
     else:
       fillVertexBufferHard_sse / fillVertexBufferHard          [:1806-1815]
```

**Hard fill uses ONLY `m_firstTransformData.m_transformIndex` (100% single bone), no weight blend** [`fillVertexBufferHard :2549-2554`, SSE `:2305+`].

### 3.5 fillVertexBuffer (soft) — exonerated path

```
fillVertexBuffer(...)                                   [:1832+]
  for each vertex:
    blend via transformArray[bone].rotateTranslate_l2p(sourcePos) * weight  [:1923-1936]
    destVertexIt.setPosition(blendPosition)             [:1985 area]
    destVertexIt.setNormal(blendNormal)
  **SPACE: OBJECT (model-to-root), NO o2w on CPU**
```

- **dot3 branch:** `:1848-1900` (extra TC set)
- **non-dot3 branch:** `:1902+` (vSize=36 case)

### 3.6 draw()

```
SoftwareBlendSkeletalShaderPrimitive::draw()            [:828]
  std::for_each(m_renderCommands, RenderCommand::render)  [:832]

RenderCommand::render()                                 [:306]
  Graphics::setCullMode(flipCullMode?)                  [:310-311]
  (*m_drawPrimitiveCommand)(0, m_minimumVertexBufferIndex,
      m_numberOfVertices, m_startIndexBufferIndex, m_primitiveCount)  [:314]
  → Graphics::drawIndexedTriangleList / drawIndexedTriangleStrip
  → ms_api->drawPartialIndexedTriangleList(...)
```

**Render commands built at:** `buildIndexBufferAndRenderCommands` `:1459+`  
- Tri list: `createTriList(minVbIndex, (max-min)+1, firstTriListIndex, triCount)` `:1537`  
- Tri strip: `createTriStrip(...)` `:1575`

---

## 4. Plugin divergence table (Graphics → GPU)

### 4.1 DynamicVertexBuffer::lock

| Step | Shared | D3D9 | D3D11 | **DIVERGENCE FLAG** |
|------|--------|------|-------|---------------------|
| Entry | `DynamicVertexBuffer::lock(n, forceDiscard)` | `DynamicVertexBuffer.h:186` | same | — |
| Plugin | `m_graphicsData->lock(n, forceDiscard)` | `Direct3d9_DynamicVertexBufferData::lock` **:172** | `Direct3d11_DynamicVertexBufferData::lock` **:259** | **⚠ PRIME SUSPECT** |
| Ring | Single `ms_d3dVertexBuffer` | `:207` Lock at `ms_used` | **Two rings:** main (vSize 28/24) vs **skin ring** (vSize ≠28,≠24) **:268-272** | **⚠ vSize=36 → SKIN RING on D3D11 ONLY** |
| Lock flags | — | `D3DLOCK_NOOVERWRITE` / `DISCARD` `:184-203` | `D3D11_MAP_WRITE_NO_OVERWRITE` / `WRITE_DISCARD` `:297-313` | Same semantics, different API |
| Offset | `m_offset = ms_used / vertexSize` | D3D9 `:217` | D3D11 `:331` | — |
| Return ptr | slice start | `:219` | `:334-335` | — |
| unlock advance | `ms_used += length` on **unlock** | D3D9 `:244` | D3D11: advanced in **lock**, not unlock **:335 vs :533-537** | **⚠ accounting timing differs** |

### 4.2 setVertexBuffer / setVertexBufferVector

| | D3D9 | D3D11 | **DIVERGENCE** |
|---|------|-------|----------------|
| Single dynamic | `Direct3d9::setVertexBuffer` **:3686**; `byteOffset=0`; `ms_sliceFirstVertex=data->getOffset()` **:3739-3740** | `Direct3d11_StateCache::setVertexBuffer` **:2242**; `ms_currentVBOffset = getOffset()*stride` **:2292** | **⚠ D3D9: BaseVertexIndex; D3D11: IA byte offset** (Plan 11-09.12 fix) |
| Vector (SBSSP single-stream uses this) | `Direct3d9::setVertexBufferVector` **:3764**; dynamic: `byteOffset=getOffset()*vSize` OR `ms_sliceFirstVertex=getOffset()` **:3854-3862** | `Direct3d11_VertexBufferVectorData::bind` **:101**; `offsets= getOffset()*stride` **:145** → `setVertexBufferVectorBindState` **:156** | Largely aligned post-fix |
| Ring buffer bind | always same D3D9 VB | `data->getRingBuffer()` skin vs main **:2277, :143** | **⚠ skin ring selection D3D11-only** |

### 4.3 setIndexBuffer

| | D3D9 | D3D11 |
|---|------|-------|
| Static IB | `Direct3d9::setIndexBuffer` (static path) | `Direct3d11_StateCache::setIndexBuffer` **:2356-2367** |
| SBSSP uses | `StaticIndexBuffer` **:823** | same |

### 4.4 Draw indexed

| | D3D9 | D3D11 | **DIVERGENCE** |
|---|------|-------|----------------|
| Full draw | `drawIndexedTriangleList()` → `DrawIndexedPrimitive(..., ms_sliceFirstVertex+baseIndex, …)` **:4350, :4157** | `drawIndexedTriangleList()` → `DrawIndexed(count, 0, 0)` **:3078-3082** | Offset in IA bind vs BaseVertex |
| Partial (SBSSP) | `drawIndexedPrimitive(D3DPT_TRIANGLELIST, ms_sliceFirstVertex+baseIndex, minimumVertexIndex, numberOfVertices, ms_sliceFirstIndex+startIndex, …)` **:4157, :4512** | `drawPartialIndexedTriangleList` → `DrawIndexed(prim*3, startIndex, baseIndex)` **:3451-3454** | **⚠ D3D11 partial: `minimumVertexIndex` unused in DrawIndexed** (only in diagnostics `:3524+`) |
| GPU call | `IDirect3DDevice9::DrawIndexedPrimitive` | `ID3D11DeviceContext::DrawIndexed` | — |

---

## 5. Leaf path catalog

### LEAF 1 — SBSSP / non-batch / single-stream / system→dynamic / **SM_softSkinning** / non-dot3

| Field | Value |
|-------|-------|
| **Selector** | `!useBatcher` `:1772`; `ms_useMultiStreamVertexBuffers==0`; `m_systemStream` non-null; `m_skinningMode==SM_softSkinning` `:1747`; `!m_hasDot3Vector` |
| **Primitive** | `SoftwareBlendSkeletalShaderPrimitive` |
| **Fill** | `fillVertexBuffer` `:1832` (non-dot3 `:1902`) |
| **Space** | **OBJECT** (model-to-root) |
| **VB** | Lock `m_dynamicStream` `:776`; write via `skinData`→system `:768-769`, copy `:778-779` |
| **Draw** | `RenderCommand::render` → `drawIndexedTriangleList/Strip` `:306-314` |
| **Instrument** | `fillVertexBuffer` after `:1935` (blendPosition); post-copy: D3D11 unlock probe `:464-520` |

**Status:** **EXONERATED** per your instrumentation.

---

### LEAF 2 — SBSSP / non-batch / single-stream / system→dynamic / **SM_hardSkinning** / non-dot3

| Field | Value |
|-------|-------|
| **Selector** | Screen fraction ≤ `getHardSkinningScreenFraction()` OR `m_forceHardSkinningEnabled`; `!mustUseSoftSkinning()` `:1739-1740` |
| **Fill** | `fillVertexBufferHard` `:2543` or `_fillVertexBufferHard_sse` `:2305` |
| **transformArray** | `skeleton.getBindPoseModelToRootTransforms()` `:563` — same array as soft |
| **Space** | **OBJECT** — `xf->rotateTranslate_l2p(m_sourceVectors->m_position)` `:2553`; **no `getTransform_w()` in fill** |
| **World xform** | GPU only: `setObjectToWorldTransformAndScale(transform_apw)` `:491` |
| **VB / copy / draw** | Same as LEAF 1 |
| **Instrument** | `fillVertexBufferHard` `:2564` (`*(Vector*)(viter+0)`); or `skinData` `:1814` before return |

**⚠ Primary CPU-path candidate** given soft exoneration.

---

### LEAF 3 — SBSSP / … / **SM_noSkinning**

| Field | Value |
|-------|-------|
| **Selector** | `screenDiameterFraction <= getNoSkinningScreenFraction()` `:1737-1738` |
| **First frame** | Same hard fill as LEAF 2 |
| **Later frames** | `skinData` early return `:1739-1742` — **stale positions** copied to dynamic |
| **Space** | OBJECT (when skin runs) |
| **Instrument** | `skinData :1739` (confirm skip); systemStream read before copy `:778` |

---

### LEAF 4 — SBSSP / multi-stream / SM_* (when `ms_useMultiStreamVertexBuffers==1`)

| Field | Value |
|-------|-------|
| **Selector** | `install()` `:344-348` all true |
| **Fill** | `skinData` → system `:667-668`; copy `:739-740` |
| **Bind** | `VertexBufferVector(static, dynamic)` — **2 streams** |
| **dynamic vSize** | pos+normal(+dot3 TC) only — **not 36** for non-dot3 |
| **Instrument** | systemStream scan `:689-708`; `Direct3d11_VertexBufferVectorData::bind` `:101` |

**Runtime:** typically **inactive** (GAP-6 latch off).

---

### LEAF 5 — SBSSP / skin-direct-to-dynamic (`!m_systemStream`, single-stream)

| **Selector** | `:748-759` |
| **Fill** | `skinData` direct to dynamic |
| **Copy** | none |
| **Instrument** | `m_dynamicStream->begin()` `:756` |

**Likely dead** given `_initialize :1409`.

---

### LEAF 6 — FullGeometrySkeletalAppearanceBatchRenderer

| Field | Value |
|-------|-------|
| **Selector** | `useBatcher==true` `:1787`; first submit adds batch LSP `:489-491` |
| **prepareToDraw** | `LocalShaderPrimitive::prepareToDraw` sets **identity** o2w `:328-329` |
| **Fill** | `getSkinnedVertexBuffer()` re-skins to system `:923-936`; `renderBuffers` **CPU world transform** `:230-231` |
| **Space** | **WORLD in batch VB** (`transform_a2w->rotateTranslate_l2p`) |
| **VB** | `s_batchVertexBuffer` (pos+normal+color, **vSize≈28** → D3D11 **main ring**) |
| **Draw** | `Graphics::drawIndexedTriangleList()` `:290` |
| **Instrument** | `renderBuffers` `:227-235`; `cape-batch-probe.txt` `:271-277` |

**Status:** **DORMANT** this session per your note. **Only path that intentionally writes WORLD-space positions on CPU.**

---

### LEAF 7 — TextureRendererShaderPrimitive wrapper

Identical to LEAF 1–5; delegates `:241-243`, `:248-250`. Sort key / shader from wrapper; skin path unchanged.

---

### LEAF 8 — Skeletal shadow volume (disabled)

`prepareToDraw :790-807` — **`false &&` disabled** `:790`. Would use `m_systemStream` + `ShadowVolume::render`. Not active.

---

### LEAF 9 — Skeleton debug draw (PRODUCTION==0)

`s_showSkeleton` → `Skeleton::addShaderPrimitives` `:1910-1911` → `Skeleton::LocalShaderPrimitive`. Debug bones, not mesh skin path.

---

### LEAF 10 — Hardpoint-attached rigid meshes

```
SkeletalAppearance2::render attachments               [SkeletalAppearance2.cpp:1797-1835]
  transformToWorld = object->getTransform_o2w()
                   * transformToRoot[hardpointIndex]  [:1827]
  attachedAppearance->setTransform_w(transformToWorld) [:1828]
  attachedObject->setTransform_o2p(transformToWorld)   [:1832]
  attachedAppearance->objectListCameraRender()         [:1835]
```

**Typical rigid path:** `ShaderPrimitiveSet::LocalShaderPrimitive`  
- `prepareToDraw`: `Graphics::setObjectToWorldTransformAndScale(m_owner.getTransform_w(), scale)` [`ShaderPrimitiveSet.cpp:228-229`]  
- `draw`: `m_template.draw()` → static/dynamic VB from template, **no skeletal skin**  
- **Space:** mesh OBJECT in VB; o2w via `setObjectToWorldTransformAndScale` (same contract as SBSSP)

**Not vSize=36 skin-ring SBSSP** unless attachment is itself a `SkeletalAppearance2`.

---

## 6. Transform source reference

| Transform | Source | Used where | Space |
|-----------|--------|------------|-------|
| `transformArray` | `Skeleton::getBindPoseModelToRootTransforms()` → `calculateBindPoseModelToRootTransforms()` `Skeleton.cpp:1498-1542` | All `skinData` / fill paths | **Bind-pose model → skeleton root (OBJECT)** |
| `transform_apw` | `m_appearance.getTransform_w()` | `prepareToDraw :490-491` | **Appearance → WORLD (GPU cbuffer)** |
| Batch `transform_a2w` | `appearance.getTransform_w()` | `FullGeometrySkeletalAppearanceBatchRenderer.cpp:230` | **CPU WORLD bake** |

**Hard fill does NOT multiply by `transform_apw`.** If VB positions look like world coordinates, that is **not** produced by the standard hard/soft fill — it indicates **stale ring data**, **batch path**, or **misreading post-transform VS output as VB data**.

---

## 7. THE MONEY QUESTION

**Observed:** WORLD-space, **vSize=36**, pos+normal+color+1×2D UV, **no dot3**, **~1692/2691** verts, dynamic VB, skeletal NPC, D3D11-only spike.

### Answer: which leaf?

**Primary leaf path:**

```
SkeletalAppearance2::render
  !useBatcher                                          [SkeletalAppearance2.cpp:1772]
  setSkinningMode(SM_hardSkinning)                     [likely for mid-distance NPC — soft exonerated]
  ShaderPrimitiveSorter → SBSSP::prepareToDraw
    ms_useMultiStreamVertexBuffers == 0                [install :344-348]
    m_systemStream exists                              [_initialize :1409]
    !m_hasDot3Vector                                  [_initialize :1219-1221]
    skinData → fillVertexBufferHard[_sse]              [skinData :1806-1814]
    m_dynamicStream->lock(m_vertexCount)               [:776]
    systemStream → dynamicStream copy                  [:778-779]
    Graphics::setVertexBuffer(*m_vertexBufferVector)   [:819]  → D3D11 vector bind
    Graphics::setIndexBuffer(*m_indexBuffer)           [:823]
  SBSSP::draw → RenderCommand::render → drawIndexed*   [:306-314]
    → D3D11 drawPartialIndexedTriangleList → DrawIndexed [:3451-3454]
```

**If still on soft skinning LOD:** same tree with `fillVertexBuffer` — you've ruled that out.

**Coordinate space of written dynamic VB:** **OBJECT** (model-to-root).  
**World transform supplier:** `m_appearance.getTransform_w()` → `Graphics::setObjectToWorldTransformAndScale` at `:491` → D3D11 W matrix in `applyPreDrawState` flush — **not** in VB fill.

**transformArray for hard fill:** `skeleton.getBindPoseModelToRootTransforms()` at `:563`, indexed by `m_sourceVectors[i].m_firstTransformData.m_transformIndex` only (`:2550-2551`).

### Why does it look WORLD-space?

No standard SBSSP leaf writes world into a vSize=36 skin-ring VB. Most plausible explanations **within this tree:**

1. **⚠ D3D11 skin-ring stale tail / NO_OVERWRITE hazard** (`Direct3d11_DynamicVertexBufferData.cpp:259-335`) — vSize=36 uses **dedicated skin ring** (`:268`). Tail vertices not overwritten retain **previous draw** data; if that prior slice was from a mesh already at a large world position (or mis-indexed), positions appear "world sized."

2. **⚠ Index read-past into ring tail** — `buildIndexBufferAndRenderCommands` probe `:1589-1633` documents `maxIdx >= vtxCount` (e.g. 1692 written vs indices to 2630). **Tri-strip** path has additional raw-vs-compacted index risk (`Direct3d11_StateCache.cpp:3462-3467`). GPU fetches **unwritten/stale** verts → spike at stale WORLD-like coords.

3. **1692 vs 2691 specifically:**  
   - `m_vertexCount` = full mesh (2691) locked/copied [`:776-779`]  
   - `RenderCommand::m_numberOfVertices` = `(maxVbIndex-minVbIndex)+1` per sub-mesh (`:1537`) — can be **1692** for one draw call  
   - If **indices reference verts ≥1692** but draw validation/`numberOfVertices` understates the fetch range on D3D11, tail reads stale ring — **D3D11-only divergence** at `:3451-3454` vs D3D9 `:4157`.

4. **Batch path (WORLD CPU bake)** — exonerated this session; would be vSize **28** main ring, not 36.

### Best instrumentation for THIS leaf

| Stage | File:line |
|-------|-----------|
| Hard skin output (CPU, OBJECT) | `fillVertexBufferHard` `:2564` |
| Post-skin systemStream | after `skinData` return, `m_systemStream` read `:768-769` |
| Post-copy dynamic (CPU, pre-Unmap) | D3D11 `Direct3d11_DynamicVertexBufferData::unlock` `:464-520` |
| GPU-bound slice | D3D11 staging read in `drawPartialIndexedTriangleList` `:3281+` |
| Index read-past | `cape-index-probe.txt` from `:1620-1630` |

---

## 8. D3D9 ∥ D3D11 divergence summary (bug suspects)

| # | Divergence | D3D9 | D3D11 | Risk for vSize=36 spike |
|---|------------|------|-------|-------------------------|
| 1 | **Dual VB ring** | Single ring | Skin ring for vSize≠28,24 `:268` | **HIGH** — stale tail |
| 2 | **Ring advance on unlock vs lock** | unlock `:244` | lock `:335` | **MEDIUM** — slice accounting |
| 3 | **Dynamic offset application** | BaseVertexIndex `:4157` | IA byte offset `:2292`, `:145` | **MEDIUM** — misbind |
| 4 | **Partial draw MinVertexIndex** | Used in DrawIndexedPrimitive | **Unused** in DrawIndexed `:3451` | **HIGH** — read-past |
| 5 | **setVertexBufferVector path** | Stream offsets `:3854-3862` | `VertexBufferVectorData::bind :145` | LOW (post 11-09.12 fix) |
| 6 | **Tri-strip index semantics** | Same engine IB | Strip probe `:3462-3467` | **MEDIUM** if mesh uses strips |

---

## 9. Quick selector decision tree

```
SkeletalAppearance2::render
├─ useBatcher? [1706-1723]
│  YES → LEAF 6 (batch, WORLD CPU, vSize≈28)
│  NO  ↓
├─ ShaderPrimitive type
│  ├─ TextureRendererShaderPrimitive → delegates to SBSSP (LEAF 1-5)
│  └─ SoftwareBlendSkeletalShaderPrimitive ↓
│     ├─ ms_useMultiStreamVertexBuffers? [661]
│     │  YES → LEAF 4
│     │  NO  ↓
│     ├─ m_systemStream? [748/761]
│     │  NO  → LEAF 5 (direct dynamic)
│     │  YES → system→dynamic copy [769-781] ↓
│     └─ m_skinningMode [1745]
│        ├─ SM_softSkinning → LEAF 1 (exonerated)
│        ├─ SM_hardSkinning → LEAF 2 ★ MONEY PATH
│        └─ SM_noSkinning → LEAF 3
├─ attachments [1797] → LEAF 10 (rigid, not vSize=36 skin)
└─ s_showSkeleton → LEAF 9 (debug)
```

---

**Bottom line:** For your observed **vSize=36, non-dot3, ~1692/2691, dynamic skin-ring** draw with batch dormant and soft fill exonerated, the **identifying leaf is LEAF 2** (standard non-batch SBSSP, single-stream system→dynamic, **SM_hardSkinning**, hard fill). CPU fill is **OBJECT-space** via `getBindPoseModelToRootTransforms()`; **WORLD-looking spike tail vertices are not produced by that fill** — they implicate **D3D11 plugin ring/read-past/stale-slice behavior** (divergences #1, #4) rather than a separate world-space skin path.

Switch to Agent mode if you want this written to `D:\Code\swg-client-v2\.planning\research\RENDER-PATH-TREE-cursor.md`.
