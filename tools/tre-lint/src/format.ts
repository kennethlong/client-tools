/**
 * format.ts — SWG TRE / searchTOC binary format primitives.
 *
 * Ground truth (every layout verified against real bytes + engine source, CONSULT-76/77):
 *   .tre header (36 B):  swg-client-v2 TreeFile_SearchNode.h:239-250, raw LE struct.
 *   .tre TOC entry (24B): TreeFile_SearchNode.h:254-262 — <Iiiiii> crc,length,offset,
 *                         compressor,compressedLength,fileNameOffset. CRC-FIRST, all versions.
 *   .toc header (36 B):  TreeFile_SearchNode.h:331-345 — <4s4sBBBB6I>; tree-file name block
 *                         follows the header UNCOMPRESSED, then TOC block, then name block.
 *   .toc entry (24 B):   TreeFile_SearchNode.h:349-360 — <BBHIIIII> compressor,unused,
 *                         treeFileIndex,crc,fileNameLENGTH(on disk; offsets are rebuilt
 *                         cumulatively — TreeFile_SearchNode.cpp:869-881),offset,length,
 *                         compressedLength. Mirrored in tre_reader.py:368-410.
 *   Tags: big-endian-composed uint32 values serialized little-endian (Tag.h:93-95), so
 *   on-disk ASCII reads mirrored: 'TREE'->"EERT", '0005'->"5000", 'TOC '->" COT", '0001'->"1000".
 *   CRC-32: FORWARD (MSB-first), poly 0x04C11DB7, init/xorout 0xFFFFFFFF, over the
 *   lowercased forward-slash path (Crc.cpp; verified vs 25 stock TOC entries in M0).
 */

// ─── Tag values (numeric, compared as uint32 — never as strings) ─────────────
export const TAG_TREE = 0x54524545; // on disk "EERT"
export const TAG_TOC = 0x544f4320; // on disk " COT"
export const TAG_0001 = 0x30303031; // on disk "1000"
export const TAG_0002 = 0x30303032; // on disk "2000" (Restoration .toc fork; stock FATALs)
export const TAG_0004 = 0x30303034; // on disk "4000" (engine-accepted; never observed)
export const TAG_0005 = 0x30303035; // on disk "5000" (THE stock format)
export const TAG_0006 = 0x30303036; // on disk "6000" (container/Restoration; not searchTree-mountable stock)

export function tagToAscii(tag: number): string {
  // Render the tag VALUE the human way (forward), e.g. 0x54524545 -> "TREE".
  return String.fromCharCode((tag >>> 24) & 0xff, (tag >>> 16) & 0xff, (tag >>> 8) & 0xff, tag & 0xff);
}
export function onDiskAscii(tag: number): string {
  // Render what the bytes look like in a hex dump (mirrored), e.g. "EERT".
  return String.fromCharCode(tag & 0xff, (tag >>> 8) & 0xff, (tag >>> 16) & 0xff, (tag >>> 24) & 0xff);
}

// ─── Forward CRC-32 (Crc.cpp) ────────────────────────────────────────────────
const CRC_TABLE = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
  let c = (i << 24) >>> 0;
  for (let k = 0; k < 8; k++) {
    c = ((c & 0x80000000) !== 0 ? ((c << 1) ^ 0x04c11db7) : (c << 1)) >>> 0;
  }
  CRC_TABLE[i] = c;
}
export function crc32Forward(bytes: Uint8Array): number {
  let crc = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) {
    crc = (CRC_TABLE[((crc >>> 24) ^ bytes[i]) & 0xff] ^ (crc << 8)) >>> 0;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
export function crc32OfString(s: string): number {
  return crc32Forward(new TextEncoder().encode(s));
}

/** Engine name normalization (TreeFile.cpp:511-601 fixUpFileName, the parts that matter here). */
export function normalizeVirtualPath(raw: string): string {
  let s = raw.replace(/\\/g, "/").toLowerCase();
  s = s.replace(/\/{2,}/g, "/");
  while (s.startsWith("./")) s = s.slice(2);
  return s;
}

// ─── .tre header + TOC ───────────────────────────────────────────────────────
export interface TreHeader {
  token: number;
  version: number;
  numberOfFiles: number;
  tocOffset: number;
  tocCompressor: number;
  sizeOfTOC: number;
  blockCompressor: number;
  sizeOfNameBlock: number;
  uncompSizeOfNameBlock: number;
}

export function parseTreHeader(buf: Buffer): TreHeader {
  if (buf.length < 36) throw new Error(`file too small for a TRE header (${buf.length} bytes)`);
  return {
    token: buf.readUInt32LE(0),
    version: buf.readUInt32LE(4),
    numberOfFiles: buf.readUInt32LE(8),
    tocOffset: buf.readUInt32LE(12),
    tocCompressor: buf.readUInt32LE(16),
    sizeOfTOC: buf.readUInt32LE(20),
    blockCompressor: buf.readUInt32LE(24),
    sizeOfNameBlock: buf.readUInt32LE(28),
    uncompSizeOfNameBlock: buf.readUInt32LE(32),
  };
}

export interface TreTocEntry {
  crc: number;
  length: number; // uncompressed; 0 = tombstone
  offset: number;
  compressor: number;
  compressedLength: number;
  fileNameOffset: number;
  name?: string; // resolved from the name block
}

/** Parse retail 24-byte crc-first records from a DECOMPRESSED toc block. */
export function parseTreTocEntries24(toc: Buffer, count: number): TreTocEntry[] {
  const out: TreTocEntry[] = [];
  for (let i = 0; i < count; i++) {
    const o = i * 24;
    out.push({
      crc: toc.readUInt32LE(o),
      length: toc.readInt32LE(o + 4),
      offset: toc.readInt32LE(o + 8),
      compressor: toc.readInt32LE(o + 12),
      compressedLength: toc.readInt32LE(o + 16),
      fileNameOffset: toc.readInt32LE(o + 20),
    });
  }
  return out;
}

/**
 * The two CANDIDATE models for a populated v0006 (on-disk "6000") 32-byte record.
 * PAD    (toolkit assumption):  crc,length,offset,compressor,compressedLength,fileNameOffset,pad[8]
 * REORDER (GR engine field map): crc,length,offset,unk1,unk2,fileNameOffset,compressor,compressedLength
 * Both agree on crc@0,length@4,offset@8 — and (notably) fileNameOffset@20.
 * They differ ONLY in where compressor/compressedLength live (12/16 vs 24/28) and which
 * pair is expected to be zero. lint.ts arbitrates per archive using real bytes.
 */
export function parseTreTocEntry32(toc: Buffer, i: number, model: "pad" | "reorder"): TreTocEntry & { zeroPairA: number; zeroPairB: number } {
  const o = i * 32;
  const base = {
    crc: toc.readUInt32LE(o),
    length: toc.readInt32LE(o + 4),
    offset: toc.readInt32LE(o + 8),
    fileNameOffset: toc.readInt32LE(o + 20),
    zeroPairA: toc.readUInt32LE(o + 12) | toc.readUInt32LE(o + 16), // reorder expects 0 here
    zeroPairB: toc.readUInt32LE(o + 24) | toc.readUInt32LE(o + 28), // pad expects 0 here
  };
  if (model === "pad") {
    return { ...base, compressor: toc.readInt32LE(o + 12), compressedLength: toc.readInt32LE(o + 16) };
  }
  return { ...base, compressor: toc.readInt32LE(o + 24), compressedLength: toc.readInt32LE(o + 28) };
}

/** Resolve entry names from a decompressed name block; returns null on a bad offset. */
export function readNameAt(names: Buffer, offset: number): string | null {
  if (offset < 0 || offset >= names.length) return null;
  const end = names.indexOf(0, offset);
  if (end < 0) return null;
  return names.toString("latin1", offset, end);
}

// ─── .toc (searchTOC) ────────────────────────────────────────────────────────
export interface SearchTocHeader {
  token: number;
  version: number;
  tocCompressor: number;
  nameBlockCompressor: number;
  numberOfFiles: number;
  sizeOfTOC: number;
  sizeOfNameBlock: number;
  uncompSizeOfNameBlock: number;
  numberOfTreeFiles: number;
  sizeOfTreeFileNameBlock: number;
  treeFileNames: string[];
  tocBlockOffset: number; // file offset where the (maybe compressed) TOC block starts
}

export function parseSearchTocHeader(buf: Buffer): SearchTocHeader {
  if (buf.length < 36) throw new Error(`file too small for a searchTOC header (${buf.length} bytes)`);
  const h = {
    token: buf.readUInt32LE(0),
    version: buf.readUInt32LE(4),
    tocCompressor: buf.readUInt8(8),
    nameBlockCompressor: buf.readUInt8(9),
    numberOfFiles: buf.readUInt32LE(12),
    sizeOfTOC: buf.readUInt32LE(16),
    sizeOfNameBlock: buf.readUInt32LE(20),
    uncompSizeOfNameBlock: buf.readUInt32LE(24),
    numberOfTreeFiles: buf.readUInt32LE(28),
    sizeOfTreeFileNameBlock: buf.readUInt32LE(32),
  };
  // Tree-file name block: immediately after the header, UNCOMPRESSED, n null-terminated names.
  const treeFileNames: string[] = [];
  let pos = 36;
  for (let i = 0; i < h.numberOfTreeFiles; i++) {
    const end = buf.indexOf(0, pos);
    if (end < 0) throw new Error("truncated tree-file name block");
    treeFileNames.push(buf.toString("latin1", pos, end));
    pos = end + 1;
  }
  return { ...h, treeFileNames, tocBlockOffset: pos };
}

export interface SearchTocEntry {
  compressor: number;
  treeFileIndex: number;
  crc: number;
  fileNameLengthOnDisk: number; // raw field; offsets are REBUILT cumulatively
  fileNameOffset: number; // rebuilt
  offset: number; // absolute offset inside the member .tre
  length: number; // uncompressed; 0 entries exist but do NOT tombstone (SearchTOC can't delete)
  compressedLength: number;
  name?: string;
}

export function parseSearchTocEntries(toc: Buffer, count: number): SearchTocEntry[] {
  // <BBHIIIII>: comp@0(u8) unused@1(u8) treeIdx@2(u16) crc@4 fnLen@8 offset@12 length@16 clen@20
  const out: SearchTocEntry[] = [];
  let nameOffset = 0;
  for (let i = 0; i < count; i++) {
    const o = i * 24;
    const fnLen = toc.readUInt32LE(o + 8);
    out.push({
      compressor: toc.readUInt8(o),
      treeFileIndex: toc.readUInt16LE(o + 2),
      crc: toc.readUInt32LE(o + 4),
      fileNameLengthOnDisk: fnLen,
      fileNameOffset: nameOffset,
      offset: toc.readUInt32LE(o + 12),
      length: toc.readUInt32LE(o + 16),
      compressedLength: toc.readUInt32LE(o + 20),
    });
    nameOffset += fnLen + 1;
  }
  return out;
}
