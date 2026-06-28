Most-detailed code reader. Codebase D:\Code\swg-client-v2 (modern MSVC v145/C++20 rebuild of 2003 SWG
client). READ-ONLY. Cite exact file:line.

FIRST read .planning/research/CONSULT-46-EVIDENCE-layoutthrottle.md (facts are GIVEN).

YOUR ANGLE = Q2 VALUE, quantification half. Others cover teardown risk, the do/don't recommendation,
and the throttle design -- do NOT duplicate. Answer concretely, with numbers where the code/data allow:

1. How many objects does a cantina interior-layout actually create in the burst? Find the interior
   layout data path: TangibleObject::getInteriorLayout / InteriorLayoutReaderWriter
   (getNumberOfObjects(cellName), getNumberOfCellNames). Where does the .ilf / interior-layout data
   come from, and can you bound the per-cell object count for the Mos Eisley cantina (or interiors of
   that class)? Even an order-of-magnitude (single digits? dozens? hundreds?) is valuable.
2. For each interior-layout object created in ClientInteriorLayoutManager.cpp:103-128, what is the
   first-load cost chain (ObjectTemplateList::createObject -> appearance -> mesh -> shader -> texture)?
   Which of these are deduped/shared across objects (so the Nth identical prop is cheap) vs per-object?
   I.e., is the burst cost dominated by a few unique heavy assets (textures/meshes) loaded once, or by
   N x per-object work?
3. Compare: after the disk shader cache (#1) removed the compile cost, is the remaining first-entry
   cost dominated by (a) the layout-object creation/instantiation itself, or (b) the texture
   create+upload those objects trigger (Direct3d9_TextureData.cpp CreateTexture + mip upload,
   main-thread)? This determines whether throttling object CREATION even helps, or whether the texture
   upload would still stall the same frames regardless of when the objects are created.

Deliver: a tight, file:line-grounded estimate of the burst size and where its cost actually lives, and
a verdict on whether spreading object CREATION across frames would meaningfully reduce the stutter or
whether the texture-upload cost would just move with it.
