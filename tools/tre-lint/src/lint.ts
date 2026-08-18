/**
 * lint.ts — archive + set lint passes. Findings, not refusals (tolerate-and-report).
 *
 * Severity vocabulary: error / warn / info (per the PROVIDER-DESIGN §4 contracts).
 * Every rule carries the engine-behavior rationale in its message where it has teeth.
 */

import * as fs from "node:fs";
import * as path from "node:path";
import * as zlib from "node:zlib";
import {
  TAG_TREE, TAG_TOC, TAG_0001, TAG_0002, TAG_0004, TAG_0005, TAG_0006,
  tagToAscii, onDiskAscii, crc32OfString, normalizeVirtualPath,
  parseTreHeader, parseTreTocEntries24, parseTreTocEntry32, readNameAt,
  parseSearchTocHeader, parseSearchTocEntries,
  type TreTocEntry, type SearchTocEntry,
} from "./format.ts";
import { looksLikeIff, validateIff } from "./iff.ts";

export type Severity = "error" | "warn" | "info";

export interface Finding {
  ruleId: string;
  severity: Severity;
  archive: string; // basename of the file the finding is about
  path?: string; // virtual path, when entry-scoped
  message: string;
}

export interface ArchiveReport {
  file: string;
  kind: "tre" | "toc" | "unreadable";
  sizeBytes: number;
  version?: string; // tag value rendered forward, e.g. "0005"
  entryCount?: number;
  tombstones?: number;
  findings: Finding[];
  /** v0006 populated-TOC model arbitration verdict, when applicable. */
  v0006Model?: { winner: "pad" | "reorder" | "ambiguous"; padScore: number; reorderScore: number; compressorValues: Record<string, number> };
  /** Parsed entries kept for set-level passes (name -> entry). Not serialized. */
  entryMap?: Map<string, TreTocEntry>;
  searchTocEntries?: SearchTocEntry[];
  treeFileNames?: string[];
}

const KNOWN_TRE_VERSIONS = new Map<number, string>([
  [TAG_0004, "0004"],
  [TAG_0005, "0005"],
  [TAG_0006, "0006"],
]);

function inflate(buf: Buffer): Buffer | null {
  try {
    return zlib.inflateSync(buf);
  } catch {
    return null;
  }
}

function stricmpLess(a: string, b: string): boolean {
  const x = a.toLowerCase();
  const y = b.toLowerCase();
  return x < y;
}

/** Per-rule finding cap per archive — identical-rule floods roll up into one count line. */
const PER_RULE_CAP = 25;

function makeReporter(rep: ArchiveReport, base: string) {
  const counts = new Map<string, number>();
  const F = (ruleId: string, severity: Severity, message: string, vpath?: string) => {
    const n = (counts.get(ruleId) ?? 0) + 1;
    counts.set(ruleId, n);
    if (n <= PER_RULE_CAP) rep.findings.push({ ruleId, severity, archive: base, path: vpath, message });
  };
  const flushRollups = () => {
    for (const [ruleId, n] of counts) {
      if (n > PER_RULE_CAP) {
        const sev = rep.findings.find((f) => f.ruleId === ruleId)?.severity ?? "info";
        rep.findings.push({ ruleId: `${ruleId}-rollup`, severity: sev, archive: base, message: `…and ${n - PER_RULE_CAP} more ${ruleId} findings (total ${n})` });
      }
    }
  };
  return { F, flushRollups };
}

// ─── .tre archive lint ───────────────────────────────────────────────────────

export interface DeepOptions {
  /** Run the IFF size-fit walk on decompressible payloads matching this predicate. */
  deep: boolean;
  filter?: (virtualPath: string) => boolean;
  /** Validate every zlib payload inflates to `length` (expensive). Implied by deep. */
  validatePayloads?: boolean;
}

export function lintTreArchive(filePath: string, opts: DeepOptions): ArchiveReport {
  const base = path.basename(filePath);
  const buf = fs.readFileSync(filePath);
  const rep: ArchiveReport = { file: base, kind: "tre", sizeBytes: buf.length, findings: [] };
  const { F, flushRollups } = makeReporter(rep, base);

  let h;
  try {
    h = parseTreHeader(buf);
  } catch (e) {
    rep.kind = "unreadable";
    F("tre-header-truncated", "error", String(e));
    return rep;
  }

  // R: magic
  if (h.token !== TAG_TREE) {
    if (h.token === 0x45455254 /* forward "TREE" bytes */) {
      F("tre-magic-forward", "error", `magic is FORWARD ASCII "TREE" — mirrored uint32 convention violated; the engine requires on-disk "EERT" (Tag.h:93-95 + raw LE struct read)`);
    } else {
      F("tre-magic", "error", `magic 0x${h.token.toString(16)} is not TAG_TREE — not a TRE archive`);
    }
    rep.kind = "unreadable";
    return rep;
  }

  // R: version
  const versionName = KNOWN_TRE_VERSIONS.get(h.version);
  if (!versionName) {
    if (h.version === 0x35303030 || h.version === 0x34303030 || h.version === 0x36303030) {
      F("tre-version-forward", "error", `version bytes are FORWARD ASCII "${onDiskAscii(h.version)}" — this is the mirrored-tag defect class; the stock client FATALs on it (TreeFile_SearchNode.cpp:505-514; runtime-proven 2026-08-17, exit 0x80000003 in ~4s)`);
    } else {
      F("tre-version-unknown", "error", `version tag 0x${h.version.toString(16)} ("${onDiskAscii(h.version)}" on disk) is not a known version`);
    }
    return rep;
  }
  rep.version = versionName;
  if (h.version === TAG_0004) {
    F("tre-version-4000-observed", "warn", `version '0004' — engine-accepted but ZERO real archives exist in the 869-archive census; verify provenance`);
  }
  if (h.version === TAG_0006) {
    F("tre-v0006-not-searchtree-mountable", "info", `version '0006' — the STOCK client cannot mount this on a searchTree_ line (no case arm; FATAL). Legal only as a .toc member or under extended engines`);
  }

  rep.entryCount = h.numberOfFiles;
  if (h.numberOfFiles === 0) {
    F("tre-blob-container", "info", `numberOfFiles=0 — a .toc-membered blob container (readable by offset, NEVER enumerable); not a defect`);
    return rep;
  }

  // R: header bounds
  const stride = h.version === TAG_0006 ? 32 : 24;
  if (h.tocOffset + h.sizeOfTOC > buf.length) {
    F("tre-toc-bounds", "error", `TOC block [${h.tocOffset}, +${h.sizeOfTOC}] exceeds file size ${buf.length}`);
    return rep;
  }

  // R: TOC decode
  const tocRaw = buf.subarray(h.tocOffset, h.tocOffset + h.sizeOfTOC);
  let toc: Buffer;
  if (h.tocCompressor !== 0) {
    const inflated = inflate(tocRaw);
    if (!inflated) {
      F("tre-toc-inflate", "error", `TOC block declares compressor ${h.tocCompressor} but does not inflate as zlib — obfuscated or corrupt (known in the wild: SWG Beyond 6000 dialect)`);
      return rep;
    }
    toc = inflated;
  } else {
    toc = Buffer.from(tocRaw);
  }
  const expected = stride * h.numberOfFiles;
  if (toc.length !== expected) {
    F("tre-toc-size", toc.length > expected ? "warn" : "error", `decompressed TOC is ${toc.length} bytes; ${h.numberOfFiles} × ${stride} = ${expected} expected`);
    if (toc.length < expected) return rep;
  }

  // R: name block decode
  const nameBlockOffset = h.tocOffset + h.sizeOfTOC;
  const nameRaw = buf.subarray(nameBlockOffset, nameBlockOffset + h.sizeOfNameBlock);
  let names: Buffer;
  if (h.blockCompressor !== 0) {
    const inflated = inflate(nameRaw);
    if (!inflated) {
      F("tre-nameblock-inflate", "error", `name block declares compressor ${h.blockCompressor} but does not inflate`);
      return rep;
    }
    names = inflated;
  } else {
    names = Buffer.from(nameRaw);
  }
  if (names.length !== h.uncompSizeOfNameBlock) {
    F("tre-nameblock-size", "error", `decompressed name block is ${names.length} bytes; header declares uncompSizeOfNameBlock=${h.uncompSizeOfNameBlock} — the engine allocates and decompresses against this field (TreeFile_SearchNode.cpp:452-484)`);
  }

  // v0006 populated TOC: arbitrate the 32-byte record model on real bytes.
  let entries: TreTocEntry[];
  if (stride === 32) {
    const n = h.numberOfFiles;
    let padOk = 0;
    let reorderOk = 0;
    const compressorValues: Record<string, number> = {};
    const scored = { pad: [] as TreTocEntry[], reorder: [] as TreTocEntry[] };
    for (const model of ["pad", "reorder"] as const) {
      for (let i = 0; i < n; i++) {
        const e = parseTreTocEntry32(toc, i, model);
        scored[model].push(e);
        const name = readNameAt(names, e.fileNameOffset);
        const zeroWhereExpected = model === "pad" ? e.zeroPairB === 0 : e.zeroPairA === 0;
        const compSane = e.compressor >= 0 && e.compressor <= 2;
        const clenSane = e.offset >= 0 && e.offset + (e.compressor !== 0 ? e.compressedLength : e.length) <= buf.length;
        const crcOk = name !== null && crc32OfString(name) === e.crc;
        if (zeroWhereExpected && compSane && clenSane && crcOk) {
          if (model === "pad") padOk++;
          else reorderOk++;
        }
        if (model === "reorder") {
          compressorValues[String(e.compressor)] = (compressorValues[String(e.compressor)] ?? 0) + 1;
        }
      }
    }
    const winner = padOk === reorderOk ? "ambiguous" : padOk > reorderOk ? "pad" : "reorder";
    rep.v0006Model = { winner, padScore: padOk / n, reorderScore: reorderOk / n, compressorValues };
    F("tre-v0006-record-model", "info", `populated v0006 TOC: record-model arbitration — pad ${(100 * padOk) / n}% vs reorder ${(100 * reorderOk) / n}% ⇒ ${winner}`);
    entries = winner === "pad" ? scored.pad : scored.reorder; // ambiguous falls to reorder view for reporting
  } else {
    entries = parseTreTocEntries24(toc, h.numberOfFiles);
  }

  // Resolve names + per-entry rules
  let tombstones = 0;
  const namesSeen = new Map<string, number>();
  const crcSeen = new Map<number, string>();
  let prev: TreTocEntry | null = null;
  for (const e of entries) {
    const name = readNameAt(names, e.fileNameOffset);
    if (name === null) {
      F("tre-name-offset", "error", `entry crc=0x${e.crc.toString(16)} fileNameOffset=${e.fileNameOffset} is outside/unterminated in the name block`);
      continue;
    }
    e.name = name;
    if (e.length === 0) tombstones++;

    // CRC of stored name
    const crcStored = crc32OfString(name);
    if (crcStored !== e.crc) {
      const norm = normalizeVirtualPath(name);
      if (crc32OfString(norm) === e.crc) {
        F("tre-name-not-normalized", "warn", `stored name is not in canonical lowercase/forward-slash form (CRC matches only after normalization)`, name);
      } else {
        F("tre-crc-mismatch", "error", `entry CRC 0x${e.crc.toString(16)} != forward-CRC32 of the stored name (0x${crcStored.toString(16)}) — lookups binary-search on CRC; this entry is unfindable or misfiled`, name);
      }
    }

    // compressor — dialect-aware: the Restoration v0006 family uses code 1 for zlib
    // (confirmed on 215k+ real entries under the arbitrated reorder model, 2026-08-17).
    if (e.compressor !== 0 && e.compressor !== 2) {
      if (e.compressor === 1 && stride === 32) {
        F("tre-compressor-dialect", "info", `compressor code 1 — the Restoration-family zlib code (stock uses 2)`, name);
      } else {
        F("tre-compressor", e.compressor === 1 ? "warn" : "error", `compressor code ${e.compressor} (stock emits only 0=stored, 2=zlib; 1 is CT_deprecated)`, name);
      }
    }
    // payload bounds
    const onDisk = e.compressor !== 0 ? e.compressedLength : e.length;
    if (e.length > 0 && (e.offset < 0 || e.offset + onDisk > buf.length)) {
      F("tre-payload-bounds", "error", `payload [${e.offset}, +${onDisk}] exceeds file size ${buf.length}`, name);
    }
    if (e.compressor === 0 && e.compressedLength !== 0 && e.compressedLength !== e.length) {
      F("tre-stored-clen", "warn", `stored entry has compressedLength=${e.compressedLength} (stock convention writes 0 for stored)`, name);
    }

    // stub-sized textures (the ILM preference-kill class: structurally valid tiny
    // textures parked at stock paths to blank content). Non-deep: size heuristic.
    // Deep: real DDS header dimensions (magic "DDS ", height@12, width@16 LE).
    if (e.length > 0 && e.length <= 512 && /\.(dds|tga)$/.test(name)) {
      if (opts.deep || opts.validatePayloads) {
        const onDiskLen = e.compressor !== 0 ? e.compressedLength : e.length;
        if (e.offset >= 0 && e.offset + onDiskLen <= buf.length) {
          let p: Buffer | null = buf.subarray(e.offset, e.offset + onDiskLen);
          if (e.compressor === 2) p = inflate(p);
          if (p && p.length >= 20 && p.toString("latin1", 0, 4) === "DDS ") {
            const height = p.readUInt32LE(12);
            const width = p.readUInt32LE(16);
            if (width <= 16 && height <= 16) {
              F("tre-stub-texture", "warn", `${width}×${height} DDS in ${e.length} bytes — the stub-texture landmine class (valid file, blanks whatever it shadows)`, name);
            }
          } else {
            F("tre-stub-sized-texture", "info", `texture entry is only ${e.length} bytes — stub-sized`, name);
          }
        }
      } else {
        F("tre-stub-sized-texture", "info", `texture entry is only ${e.length} bytes — stub-sized (run --deep for DDS dimensions)`, name);
      }
    }

    // duplicates
    const dupName = namesSeen.get(name);
    if (dupName !== undefined) {
      F("tre-dup-path", "warn", `path appears ${dupName + 1}+ times in one archive`, name);
    }
    namesSeen.set(name, (dupName ?? 0) + 1);
    const crcName = crcSeen.get(e.crc);
    if (crcName !== undefined && crcName !== name) {
      F("tre-dup-crc", "warn", `CRC collision: 0x${e.crc.toString(16)} for both "${crcName}" and "${name}" — engine tie-breaks by name compare; writers must keep both`, name);
    }
    crcSeen.set(e.crc, name);

    // sort order (crc asc, tie stricmp)
    if (prev) {
      if (e.crc < prev.crc || (e.crc === prev.crc && prev.name && e.name && stricmpLess(e.name, prev.name))) {
        F("tre-toc-sort", "error", `TOC not sorted ascending by (crc, stricmp) at "${name}" — binary-search precondition violated; lookups past this point are unreliable`, name);
        prev = null; // report once per break
        continue;
      }
    }
    prev = e;
  }
  rep.tombstones = tombstones;
  if (tombstones > 0) {
    F("tre-tombstones", "info", `${tombstones} zero-length entries — REAL runtime tombstones when mounted via searchTree (they do NOT delete via a .toc)`);
  }

  // Deep: payload decode + IFF size-fit
  if ((opts.deep || opts.validatePayloads) && rep.version !== "0006") {
    for (const e of entries) {
      if (!e.name || e.length === 0) continue;
      if (opts.filter && !opts.filter(e.name)) continue;
      const onDisk = e.compressor !== 0 ? e.compressedLength : e.length;
      if (e.offset < 0 || e.offset + onDisk > buf.length) continue; // already reported
      let payload: Buffer | null = buf.subarray(e.offset, e.offset + onDisk);
      if (e.compressor === 2) {
        payload = inflate(payload);
        if (!payload) {
          F("tre-payload-inflate", "error", `zlib payload does not inflate`, e.name);
          continue;
        }
        if (payload.length !== e.length) {
          F("tre-payload-length", "error", `inflated payload is ${payload.length} bytes; TOC declares length=${e.length}`, e.name);
          continue;
        }
      }
      if (opts.deep && looksLikeIff(payload)) {
        for (const d of validateIff(payload)) {
          const sev: Severity = d.kind === "iff-over-declared" ? "error" : d.kind === "iff-trailing-bytes" ? "info" : "warn";
          F(d.kind, sev, `${d.where || "(root)"}: ${d.detail}`, e.name);
        }
      }
    }
  }

  rep.entryMap = new Map(entries.filter((e) => e.name).map((e) => [e.name as string, e]));
  flushRollups();
  return rep;
}

// ─── .toc lint (+ member cross-checks when a resolver is provided) ───────────

export function lintSearchToc(filePath: string, resolveMember?: (bareName: string) => string | null): ArchiveReport {
  const base = path.basename(filePath);
  const buf = fs.readFileSync(filePath);
  const rep: ArchiveReport = { file: base, kind: "toc", sizeBytes: buf.length, findings: [] };
  const { F, flushRollups } = makeReporter(rep, base);

  if (buf.length < 36 || buf.readUInt32LE(0) !== TAG_TOC) {
    rep.kind = "unreadable";
    F("toc-magic", "error", `not a searchTOC file (token != ' COT')`);
    return rep;
  }
  const version = buf.readUInt32LE(4);
  if (version === TAG_0002) {
    rep.version = "0002";
    F("toc-version-0002", "info", `Restoration-fork .toc version '0002' — the stock engine FATALs on it; enumerate-only territory, skipping record parse (32-byte fork records unmodeled)`);
    return rep;
  }
  if (version !== TAG_0001) {
    F("toc-version-unknown", "error", `version tag 0x${version.toString(16)} ("${onDiskAscii(version)}" on disk) — stock accepts only '0001' ("1000" on disk)`);
    return rep;
  }
  rep.version = "0001";

  let h;
  try {
    h = parseSearchTocHeader(buf);
  } catch (e) {
    F("toc-header", "error", String(e));
    return rep;
  }
  rep.treeFileNames = h.treeFileNames;
  rep.entryCount = h.numberOfFiles;

  const tocRaw = buf.subarray(h.tocBlockOffset, h.tocBlockOffset + h.sizeOfTOC);
  const toc = h.tocCompressor !== 0 ? inflate(tocRaw) : Buffer.from(tocRaw);
  if (!toc) {
    F("toc-block-inflate", "error", `TOC block declares compressor ${h.tocCompressor} but does not inflate`);
    return rep;
  }
  if (toc.length < h.numberOfFiles * 24) {
    F("toc-block-size", "error", `decompressed TOC is ${toc.length} bytes; ${h.numberOfFiles} × 24 expected`);
    return rep;
  }
  const nameRaw = buf.subarray(h.tocBlockOffset + h.sizeOfTOC, h.tocBlockOffset + h.sizeOfTOC + h.sizeOfNameBlock);
  const names = h.nameBlockCompressor !== 0 ? inflate(nameRaw) : Buffer.from(nameRaw);
  if (!names) {
    F("toc-nameblock-inflate", "error", `name block does not inflate`);
    return rep;
  }

  const entries = parseSearchTocEntries(toc, h.numberOfFiles);
  let tombstoneLike = 0;
  const memberSizes = new Map<number, number>();
  for (const e of entries) {
    const name = readNameAt(names, e.fileNameOffset);
    if (name === null) {
      F("toc-name-offset", "error", `entry crc=0x${e.crc.toString(16)}: rebuilt name offset ${e.fileNameOffset} invalid — record fileNameLength chain broken`);
      continue;
    }
    e.name = name;
    if (name.length !== e.fileNameLengthOnDisk) {
      F("toc-name-length", "error", `on-disk fileNameLength=${e.fileNameLengthOnDisk} but name "${name}" is ${name.length} chars — offsets past here are skewed`, name);
    }
    const crcStored = crc32OfString(name);
    if (crcStored !== e.crc) {
      F("toc-crc-mismatch", "error", `entry CRC != CRC of name`, name);
    }
    if (e.treeFileIndex >= h.numberOfTreeFiles) {
      F("toc-tree-index", "error", `treeFileIndex ${e.treeFileIndex} out of range (${h.numberOfTreeFiles} tree files)`, name);
      continue;
    }
    if (e.length === 0) tombstoneLike++;

    if (resolveMember) {
      const bare = h.treeFileNames[e.treeFileIndex];
      const resolved = resolveMember(bare);
      if (resolved === null) {
        F("toc-member-unresolvable", "error", `member tree file "${bare}" not found — engine resolves vs CWD then TOCTreePath and hard-FATALs on a miss (TreeFile_SearchNode.cpp:835)`, name);
      } else {
        let sz = memberSizes.get(e.treeFileIndex);
        if (sz === undefined) {
          sz = fs.statSync(resolved).size;
          memberSizes.set(e.treeFileIndex, sz);
        }
        const onDisk = e.compressor !== 0 ? e.compressedLength : e.length;
        if (e.length > 0 && e.offset + onDisk > sz) {
          F("toc-member-bounds", "error", `payload [${e.offset}, +${onDisk}] exceeds member "${h.treeFileNames[e.treeFileIndex]}" size ${sz} — stale .toc vs repacked member?`, name);
        }
      }
    }
  }
  if (tombstoneLike > 0) {
    F("toc-zero-length-entries", "warn", `${tombstoneLike} zero-length entries — a .toc CANNOT tombstone (SearchTOC::exists hardcodes deleted=false); these entries delete NOTHING and shadowed lower-priority files still win`);
  }
  rep.searchTocEntries = entries;
  flushRollups();
  return rep;
}

// ─── Set-level pass over a directory of already-linted archives ──────────────

export interface SetFindings {
  findings: Finding[];
  shadowSummary: { path: string; count: number }[];
}

export function lintSet(reports: ArchiveReport[], resolveDir: string): SetFindings {
  const findings: Finding[] = [];
  const byPath = new Map<string, { file: string; length: number }[]>();
  const add = (name: string, file: string, length: number) => {
    const arr = byPath.get(name);
    const rec = { file, length };
    if (arr) arr.push(rec);
    else byPath.set(name, [rec]);
  };
  for (const r of reports) {
    if (r.kind === "tre" && r.entryMap) {
      for (const [name, e] of r.entryMap) add(name, r.file, e.length);
    }
    // TOC-layer blind spot (standing rule): searchTOC-indexed copies live inside member
    // containers whose internal TOCs may be empty — per-tre scans miss them. Merge them
    // into the census, deduping entries the member's own internal TOC already contributed.
    if (r.kind === "toc" && r.searchTocEntries && r.treeFileNames) {
      const selfIndexed = new Set(
        reports.filter((x) => x.kind === "tre" && x.entryMap && (x.entryCount ?? 0) > 0).map((x) => x.file.toLowerCase()),
      );
      for (const e of r.searchTocEntries) {
        if (!e.name) continue;
        const member = path.basename(r.treeFileNames[e.treeFileIndex] ?? "").toLowerCase();
        if (selfIndexed.has(member)) continue; // already counted via the member's internal TOC
        add(e.name, `${r.file}→${member}`, e.length);
      }
    }
  }
  const shadows: { path: string; count: number }[] = [];
  for (const [p, files] of byPath) {
    if (files.length > 1) shadows.push({ path: p, count: files.length });
  }
  shadows.sort((a, b) => b.count - a.count || (a.path < b.path ? -1 : 1));
  if (shadows.length > 0) {
    findings.push({
      ruleId: "set-shadowed-paths",
      severity: "info",
      archive: path.basename(resolveDir),
      message: `${shadows.length} paths exist in more than one archive (first-match-wins shadowing); top: ${shadows
        .slice(0, 5)
        .map((s) => `${s.path}×${s.count}`)
        .join(", ")}`,
    });
  }

  // Degenerate-shadow rule (the ILM stub landmine class): a path whose copies diverge so
  // hard in size that one is a stub of another. Order-free by design — we don't parse cfgs,
  // so the finding names both copies and states the hazard conditionally. Thresholds:
  // biggest copy ≥ 4 KiB, smallest ≤ max(512 B, 5% of biggest), smallest > 0 (0 = tombstone,
  // a different rule's business).
  const degenerate: { path: string; small: { file: string; length: number }; big: { file: string; length: number }; ratio: number }[] = [];
  for (const [p, copies] of byPath) {
    if (copies.length < 2) continue;
    let small = copies[0];
    let big = copies[0];
    for (const c of copies) {
      if (c.length < small.length) small = c;
      if (c.length > big.length) big = c;
    }
    if (small.length > 0 && big.length >= 4096 && small.length <= Math.max(512, big.length * 0.05)) {
      degenerate.push({ path: p, small, big, ratio: small.length / big.length });
    }
  }
  degenerate.sort((a, b) => a.ratio - b.ratio || (a.path < b.path ? -1 : 1));
  // NO cap here: every pair goes into the report so the --baseline mode can detect a NEW
  // pair anywhere in the set (the cli caps console printing, not the JSON).
  for (const d of degenerate) {
    findings.push({
      ruleId: "set-degenerate-shadow",
      severity: "warn",
      archive: d.small.file,
      path: d.path,
      message: `${d.small.length} B copy in ${d.small.file} vs ${d.big.length} B in ${d.big.file} — a stub this small BLANKS the real content if it mounts at higher priority (the ILM preference-kill class)`,
    });
  }

  // .toc ↔ self-indexed member coherence
  const treByFile = new Map(reports.filter((r) => r.kind === "tre").map((r) => [r.file.toLowerCase(), r]));
  for (const r of reports) {
    if (r.kind !== "toc" || !r.searchTocEntries || !r.treeFileNames) continue;
    let checked = 0;
    let mismatched = 0;
    for (const e of r.searchTocEntries) {
      if (!e.name || e.length === 0) continue;
      const member = treByFile.get(path.basename(r.treeFileNames[e.treeFileIndex] ?? "").toLowerCase());
      if (!member?.entryMap) continue; // blob containers have no internal map — bounds-checked in lintSearchToc
      const internal = member.entryMap.get(e.name);
      if (!internal) continue; // .toc can index paths the member's internal TOC lacks? count silently
      checked++;
      if (internal.offset !== e.offset || internal.length !== e.length || internal.compressor !== e.compressor || internal.compressedLength !== e.compressedLength) {
        mismatched++;
        if (mismatched <= 10) {
          findings.push({
            ruleId: "set-toc-member-incoherent",
            severity: "error",
            archive: r.file,
            path: e.name,
            message: `.toc coordinates (off=${e.offset},len=${e.length},comp=${e.compressor},clen=${e.compressedLength}) != member "${member.file}" internal TOC (off=${internal.offset},len=${internal.length},comp=${internal.compressor},clen=${internal.compressedLength}) — stale .toc reads wrong bytes SILENTLY (no payload CRC in the engine)`,
          });
        }
      }
    }
    if (mismatched > 10) {
      findings.push({ ruleId: "set-toc-member-incoherent", severity: "error", archive: r.file, message: `…and ${mismatched - 10} more incoherent .toc↔member entries (of ${checked} checked)` });
    } else if (checked > 0 && mismatched === 0) {
      findings.push({ ruleId: "set-toc-member-coherent", severity: "info", archive: r.file, message: `${checked} .toc entries cross-checked against self-indexed members: all coherent` });
    }
  }
  return { findings, shadowSummary: shadows.slice(0, 50) };
}
