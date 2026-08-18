# Writer-Oriented TRE/TOC Format Spec

## 1. Format Variants and Writer Target

1. The Python reader recognizes per-archive TRE tags `0004`, `0005`, `6000`, `0006`, and `5000`; it 
treats `0004`/`0005` as retail and `6000`/`0006`/`5000` as extended or Restoration-family variants. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:40), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:41), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:42)

2. The reader uses a 24-byte TRE TOC stride for `0004`, `0005`, and `5000`, and a 32-byte stride for 
`6000` and `0006`; for extended rows it still unpacks only the first 24 bytes with the retail field 
layout. [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:138), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:139), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:141), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:214)

3. The reader recognizes two master-index families: COT2000 when the file starts with `b" COT2000"`, 
and retail SearchTOC when the first two tags are `TAG_TOC_LE` and `TAG_0001_LE`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:23), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:148), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:150), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:152)

4. `parse_toc2000()` is only a legacy alias for the COT2000 parser, so “toc2000” is not a third 
independent on-disk shape in this reader. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:461), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:462), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:463)

5. A new writer should target retail `TREE/0005` archives, with retail SearchTOC `TOC/0001` only 
when a master index is needed. The evidence is that the stock `TreeFileBuilder` writes `TAG_0005`, 
the engine SearchTree constructor handles `TAG_0004` and `TAG_0005`, and the Python reader’s retail 
set is `0004`/`0005`. [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeF
ileBuilder/src/shared/TreeFileBuilder.cpp:773), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engin
e/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:778), [TreeFile_SearchNode.cpp](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:446), [T
reeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFil
e_SearchNode.cpp:448), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/shar
edFile/src/shared/TreeFile_SearchNode.cpp:449), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:41)

6. Compatibility warning: `SearchTree::validate()` in this engine source accepts only `TAG_0004`, 
even though the constructor accepts `0004` and `0005` and the builder writes `0005`; if any caller 
gates loading through `validate()`, `0005` may fail there. [TreeFile_SearchNode.cpp](D:/Code/swg-clie
nt-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:388), [TreeFile_SearchN
ode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cp
p:407), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shar
ed/TreeFile_SearchNode.cpp:411), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/li
brary/sharedFile/src/shared/TreeFile_SearchNode.cpp:448), [TreeFile_SearchNode.cpp](D:/Code/swg-clien
t-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:449)

## 2. Target TRE `TREE/0005` Write Path

1. Write a 36-byte little-endian header with format `<4s4s7I`: token, version, number of files, TOC 
offset, TOC compressor, size of TOC, block compressor, size of name block, uncompressed size of name 
block. [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:28), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:29), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:172), [TreeFile_SearchNode.h](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:239)

2. The token must be the little-endian bytes for `TAG_TREE`, and the version should be `b"0005"` for 
the target writer. The Python reader rejects a non-`TAG_TREE_LE` token and unsupported versions. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:19), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:163), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:164), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:167), [TreeFileBuilder.cpp](D:/
Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:777),
 [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared
/TreeFileBuilder.cpp:778)

3. Write order is: provisional header, file payloads in response-file order, TOC block, file-name 
block, MD5 block; then seek back and rewrite the header with final offsets and sizes. [TreeFileBuilde
r.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder
.cpp:773), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/
src/shared/TreeFileBuilder.cpp:780), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/ap
plication/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:782), [TreeFileBuilder.cpp](D:/Code/swg-clie
nt-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:792), [TreeFileBui
lder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuil
der.cpp:799), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuild
er/src/shared/TreeFileBuilder.cpp:800), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared
/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:803), [TreeFileBuilder.cpp](D:/Code/swg-c
lient-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:805)

4. `tocOffset` is `sizeof(header) + totalSmallestSize`, where `totalSmallestSize` is the sum of 
stored payload sizes chosen by compression. [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/sh
ared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:795), [TreeFileBuilder.cpp](D:/Code/s
wg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:796), [Tree
FileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeF
ileBuilder.cpp:765)

5. Each retail TOC record is 24 bytes with fields `<Iiiiii`: `crc:uint32`, `length:int32`, 
`offset:int32`, `compressor:int32`, `compressedLength:int32`, `fileNameOffset:int32`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:31), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:32), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:214), [TreeFile_SearchNode.h](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:254)

6. TOC ordering must be by `CrcLowerString` ordering: CRC first, and normalized case-insensitive 
string comparison when CRCs match. The engine searches with a binary search by CRC and `_stricmp`, 
so unordered TOCs can be unreadable even if the Python reader lists them. [TreeFileBuilder.cpp](D:/Co
de/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:302), [
TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/T
reeFileBuilder.cpp:305), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/Tr
eeFileBuilder/src/shared/TreeFileBuilder.cpp:493), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/en
gine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:494), [TreeFile_SearchNode.cpp
](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:534),
 [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/Tree
File_SearchNode.cpp:536), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/s
haredFile/src/shared/TreeFile_SearchNode.cpp:545), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/sr
c/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:552)

7. Normalize logical paths before writing: convert backslashes to `/`, lowercase characters, 
suppress repeated slashes, and suppress dots immediately after slashes; TreeFileBuilder explicitly 
lowercases and slash-normalizes names before creating `CrcLowerString`. [TreeFileBuilder.cpp](D:/Code
/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:444), [Tr
eeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/Tre
eFileBuilder.cpp:445), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/Tree
FileBuilder/src/shared/TreeFileBuilder.cpp:448), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engi
ne/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:451), [CrcString.cpp](D:/Code/sw
g-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcString.cpp:24), [CrcString.cpp](
D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcString.cpp:30), [CrcSt
ring.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcString.cpp:3
4), [CrcString.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcSt
ring.cpp:42), [CrcString.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/sh
ared/CrcString.cpp:48)

8. Name block is the concatenation of normalized logical path strings in TOC order, each 
NUL-terminated; `fileNameOffset` is the byte offset into that uncompressed name block. [TreeFileBuild
er.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilde
r.cpp:624), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder
/src/shared/TreeFileBuilder.cpp:625), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/a
pplication/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:629), [TreeFileBuilder.cpp](D:/Code/swg-cli
ent-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:638), [TreeFileBu
ilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBui
lder.cpp:647), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuil
der/src/shared/TreeFileBuilder.cpp:648)

9. The Python reader decodes TRE names as `latin1`; COT2000 tree names are decoded as ASCII with 
replacement, and SearchTOC tree names as `latin1`. A writer should emit ASCII-compatible lowercase 
SWG paths to satisfy all readers. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:218), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:278), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:351)

10. Compression IDs are engine enum values: `0 = none`, `1 = deprecated/unsupported`, `2 = zlib`; 
writers must not emit compressor `1`. [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared
/library/sharedFile/src/shared/TreeFile_SearchNode.h:231), [TreeFile_SearchNode.h](D:/Code/swg-client
-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:233), [TreeFile_SearchNode.
h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:234), 
[TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFil
e_SearchNode.h:235), [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFi
le/src/shared/TreeFile_SearchNode.h:276), [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/sh
ared/library/sharedFile/src/shared/TreeFile_SearchNode.h:278)

11. TreeFileBuilder tries zlib only when compression is not disabled and the uncompressed block is 
larger than 1024 bytes; it stores compressed bytes only if zlib output is smaller than the original. 
[TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/
TreeFileBuilder.cpp:682), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/T
reeFileBuilder/src/shared/TreeFileBuilder.cpp:687), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/e
ngine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:700), [TreeFileBuilder.cpp](D
:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:705
), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shar
ed/TreeFileBuilder.cpp:709)

12. For compressed file payloads, set `compressor = 2`, `compressedLength = zlib byte length`, 
`length = original length`, and write compressed bytes at `offset`; for stored file payloads, set 
`compressor = 0`, write original bytes, set `length = original length`, and note TreeFileBuilder 
writes `compressedLength = 0` for stored file data. [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/e
ngine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:589), [TreeFileBuilder.cpp](D
:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:590
), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shar
ed/TreeFileBuilder.cpp:595), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/applicatio
n/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:728), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/sr
c/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:733), [TreeFile_SearchNode
.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:6
95), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/
TreeFile_SearchNode.cpp:696), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/libra
ry/sharedFile/src/shared/TreeFile_SearchNode.cpp:698), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v
2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:704)

13. `tocCompressor` and `blockCompressor` use the same compressor IDs as file payloads; the engine 
decompresses TOC and name block when the corresponding compressor field is nonzero. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:201), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:210), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:458), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:469), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sh
aredFile/src/shared/TreeFile_SearchNode.cpp:481), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src
/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:492)

14. `sizeOfTOC` and `sizeOfNameBlock` are the on-disk byte counts after optional compression; 
`uncompSizeOfNameBlock` is always the uncompressed name-block length. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:200), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:208), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:209), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:452), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:455), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sh
aredFile/src/shared/TreeFile_SearchNode.cpp:461), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src
/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:484)

15. CRC is `Crc::calculate()` over the normalized logical path. TreeFileBuilder stores 
`treeFileEntry.getCrc()`, `treeFileEntry` is a `CrcLowerString`, `CrcLowerString` constructs via 
normalized `PersistentCrcString`, and engine lookup calculates the same CRC from the lookup filename 
before binary search. [TreeFileBuilder.h](D:/Code/swg-client-v2/src/engine/shared/application/TreeFil
eBuilder/src/shared/TreeFileBuilder.h:48), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/sha
red/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:619), [CrcLowerString.cpp](D:/Code/swg
-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcLowerString.cpp:41), [CrcLowerStr
ing.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/CrcLowerString.c
pp:43), 
[Crc.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/Crc.cpp:66), 
[Crc.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/Crc.cpp:73), 
[Crc.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFoundation/src/shared/Crc.cpp:76), [T
reeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFil
e_SearchNode.cpp:534)

16. TreeFileBuilder appends an MD5 block after the name block: `numberOfFiles * 
Md5::Value::cms_dataSize`, in TOC order, uncompressed because it calls `compressAndWrite(... 
disableCompression=true ...)`; the Python reader ignores trailing bytes after the name block. [TreeFi
leBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFil
eBuilder.cpp:658), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFile
Builder/src/shared/TreeFileBuilder.cpp:660), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/s
hared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:665), [TreeFileBuilder.cpp](D:/Code/
swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:670), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:208), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:209)

17. No alignment or padding is required by the Python reader or the observed builder write path; the 
header and each subsequent block are written contiguously. The only explicit alignment note in the 
engine source is that SearchTOC’s unused byte exists to keep 32-bit word alignment inside its 
24-byte entry struct. [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeF
ileBuilder/src/shared/TreeFileBuilder.cpp:780), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engin
e/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:792), [TreeFileBuilder.cpp](D:/Co
de/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:799), [
TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/T
reeFileBuilder.cpp:800), [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/shar
edFile/src/shared/TreeFile_SearchNode.h:349)

## 3. SearchTOC `TOC/0001` Writer Requirements

1. SearchTOC header is 36 bytes with format `<4s4sBBBB6I`: token, version, TOC compressor, 
filename-block compressor, two unused bytes, number of files, size of TOC, size of filename block, 
uncompressed filename-block size, number of tree files, size of tree-file-name block. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:35), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:36), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:340), [TreeFile_SearchNode.h](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:331)

2. Token/version must be `TAG_TOC`/`TAG_0001`; the Python reader and engine SearchTOC validator 
reject other values. [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:343), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:344), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:743), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:747)

3. After the header, write the tree-name list: `numberOfTreeFiles` NUL-terminated TRE filenames 
packed back-to-back, with total byte count `sizeOfTreeFileNameBlock`. The engine reads this block 
immediately after the header, splits it by NUL, and opens each TRE by trying the current path plus 
configured `SharedFile/TOCTreePath` prefixes. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:345), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:347), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:351), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:785), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:803), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sh
aredFile/src/shared/TreeFile_SearchNode.cpp:807), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src
/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:817), [TreeFile_SearchNode.cpp](
D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:828)

4. Then write the SearchTOC TOC block, optionally zlib-compressed when `tocCompressor != 0`. The 
table row layout is `<BBHIIIII`: compressor byte, unused byte, `treeFileIndex:uint16`, `crc:uint32`, 
filename-length field on disk, payload offset, uncompressed length, compressed length. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:37), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:38), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:372), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:374), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:381), [TreeFile_SearchNode.h](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:350)

5. SearchTOC’s fifth row field is a filename length on disk, not an offset; the engine rewrites 
those lengths into running offsets after reading the TOC, and the Python reader mirrors that by 
accumulating `fn_field + 1`. [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/librar
y/sharedFile/src/shared/TreeFile_SearchNode.cpp:869), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2
/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:876), [TreeFile_SearchNode.c
pp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:877
), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/Tr
eeFile_SearchNode.cpp:879), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:387), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:388), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:389)

6. `treeFileIndex` binds an entry to the NUL-terminated TRE filename at the same zero-based index in 
the tree-name list; the engine opens payload bytes from `m_treeFiles[entry.treeFileIndex]`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:400), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:404), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:405), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:1043),
 [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/Tree
File_SearchNode.cpp:1107), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/
sharedFile/src/shared/TreeFile_SearchNode.cpp:1108), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/
src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:1112)

7. SearchTOC entries must be sorted by the same CRC/name binary-search order as direct TRE entries, 
because the engine SearchTOC lookup binary-searches by `Crc::calculate(fileName)` and `_stricmp`. [Tr
eeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile
_SearchNode.cpp:958), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/share
dFile/src/shared/TreeFile_SearchNode.cpp:960), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/en
gine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:969), [TreeFile_SearchNode.cpp](D:/
Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:976)

8. After the SearchTOC TOC block, write the filename block as NUL-terminated logical paths in TOC 
order, optionally zlib-compressed when `fileNameBlockCompressor != 0`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:390), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:391), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:395), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:396), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:883), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:894), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sh
aredFile/src/shared/TreeFile_SearchNode.cpp:900)

## 4. COT2000 / TOC2000 Reader Variant

1. COT2000 starts with `b" COT2000"` and then seven little-endian `uint32` fields beginning at byte 
8; the reader interprets these as reserved, number of files, TOC size, name-block size, duplicate 
name size, number of trees, and tree-name-block size. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:23), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:264), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:267), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:269)

2. COT2000 tree names begin at offset 36 and are NUL-terminated; after tree names comes a 
32-byte-entry global TOC, then the name block. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/swg_pipeline/tre_reader.py:24) is a typo 
path; actual source: [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:24), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:272), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:280), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:281)

3. COT2000 global entry layout is `<BBHIIIII`: compressor byte, unused byte, tree index, CRC, 
filename length, file offset, uncompressed length, compressed length. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:25), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:26), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:303)

4. COT2000 is not the recommended writer target for stock retail compatibility because the shared 
evidence labels retail SearchTOC as the retail master index and COT2000 as SwgRestoration-specific. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:4), [CONSULT-76-EVIDENCE-tre-bu
ilder.md](D:/Code/swg-client-v2/.planning/research/CONSULT-76-EVIDENCE-tre-builder.md:25), [CONSULT-7
6-EVIDENCE-tre-builder.md](D:/Code/swg-client-v2/.planning/research/CONSULT-76-EVIDENCE-tre-builder.m
d:26), [CONSULT-76-EVIDENCE-tre-builder.md](D:/Code/swg-client-v2/.planning/research/CONSULT-76-EVIDE
NCE-tre-builder.md:27)

## 5. Lenient Reader Points vs Strict Writer Form

1. Reader lenient: any nonzero compressor value triggers zlib decompression for TOC, name blocks, 
and payloads. Writer strict form: emit only `0` for stored or `2` for zlib, never deprecated `1`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:189), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:190), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:192), [TreeFile_SearchNode.h](D
:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:231), [Tre
eFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_Se
archNode.h:234), [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/s
rc/shared/TreeFile_SearchNode.h:235), [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared
/library/sharedFile/src/shared/TreeFile_SearchNode.h:278)

2. Reader lenient: direct TRE lookup compares requested logical names case-insensitively after 
replacing backslashes with slashes. Writer strict form: store normalized lowercase slash paths and 
CRCs for those same normalized paths. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:241), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:243), [TreeFileBuilder.cpp](D:/
Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:444),
 [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared
/TreeFileBuilder.cpp:451)

3. Reader lenient: it lists TRE entries in on-disk order and does not verify sort order. Writer 
strict form: sort by engine binary-search order, because engine lookup requires it. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:212), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:229), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:536), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:545)

4. Reader lenient: it does not validate CRC fields against path names. Writer strict form: compute 
exact engine CRC, because engine lookup first compares CRC before `_stricmp`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:214), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:222), [TreeFile_SearchNode.cpp]
(D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:534), 
[TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeF
ile_SearchNode.cpp:545), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sh
aredFile/src/shared/TreeFile_SearchNode.cpp:552)

5. Reader lenient: it accepts extra bytes after the expected TOC table and ignores the 8 trailing 
bytes in 32-byte extended TRE entries. Writer strict form: emit 24-byte retail entries for `0005`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:202), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:203), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:212), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:214)

6. Reader strict: TRE token must match `TAG_TREE_LE`, and SearchTOC token/version must match 
`TAG_TOC_LE`/`TAG_0001_LE`. 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:164), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:167), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:343)

7. Reader strict: name offsets must land on NUL-terminated names; missing terminators raise format 
errors. [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:215), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:216), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:217), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:396), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:397), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:398)

## 6. Open Questions for Engine/Tool Confirmation

1. Resolve the `SearchTree::validate()` mismatch: validate accepts only `0004`, constructor accepts 
`0004`/`0005`, and TreeFileBuilder writes `0005`; check which client config paths actually call 
`validate()` before `SearchTree` construction. [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/en
gine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:411), [TreeFile_SearchNode.cpp](D:/
Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:448), [Tre
eFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_
SearchNode.cpp:449), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFi
leBuilder/src/shared/TreeFileBuilder.cpp:778)

2. Confirm whether the MD5 block is required by all production client builds or only by 
TreeFileBuilder/Extractor tooling; the Python reader ignores it, while TreeFileBuilder always writes 
it. [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/sha
red/TreeFileBuilder.cpp:802), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/src/engine/shared/applicati
on/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:803), [TreeFileBuilder.cpp](D:/Code/swg-client-v2/s
rc/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:658), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:208)

3. Confirm whether retail SearchTOC writer tooling exists in this tree, because this Python repo 
reads SearchTOC but the inspected TreeFileBuilder path writes standalone TRE archives, not a master 
SearchTOC. [tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:335), 
[tre_reader.py](D:/Code/swg-blender-plugin/swg_pipeline/tre_reader.py:368), [TreeFileBuilder.cpp](D:/
Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:773)

4. Confirm whether SearchTOC permits compressed tree-name blocks; the header has no compressor field 
for tree-name names, and the engine reads them raw immediately after the header. [TreeFile_SearchNode
.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFile_SearchNode.h:331),
 [TreeFile_SearchNode.h](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src/shared/TreeFi
le_SearchNode.h:344), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/share
dFile/src/shared/TreeFile_SearchNode.cpp:813)

5. Confirm duplicate CRC/name behavior in generated SearchTOC sets. Engine binary search handles 
equal CRCs by `_stricmp`, so duplicate logical paths or inconsistent normalization can make entries 
unreachable. [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shared/library/sharedFile/src
/shared/TreeFile_SearchNode.cpp:969), [TreeFile_SearchNode.cpp](D:/Code/swg-client-v2/src/engine/shar
ed/library/sharedFile/src/shared/TreeFile_SearchNode.cpp:976), [TreeFileBuilder.cpp](D:/Code/swg-clie
nt-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp:500), [TreeFileBui
lder.cpp](D:/Code/swg-client-v2/src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuil
der.cpp:512)

## 7. Verification Note

`python -m pytest tests/ -q -p no:cacheprovider` could not start in this sandbox because Python 
reported no usable temporary directory, including `D:\Code\swg-blender-plugin`. No tests were 
collected or run.