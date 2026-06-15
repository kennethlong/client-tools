// Frozen-contract TypeScript types for the tre-compare SPA.
//
// Transcribed VERBATIM from the Phase-29 backend contract (the four FastAPI routes):
//   - tre_compare/diff.py  — status enum literals + the file-level lean row / summary shape
//   - tre_compare/api.py   — the /file/detail 200 body + the _merged_meta / _node_brief /
//                            _hash_brief serializers
//
// Two correctness rules baked into these types (RESEARCH Pitfall 3 / UI-SPEC §Color HARD RULE):
//   1. SET-level and FILE-level statuses are DIFFERENT enums — `SetStatus` vs `FileStatus`,
//      never one shared union. `/compare/set` emits `identical`/`fault`; `/compare/files`
//      emits `identical-by-metadata`/`tombstoned`.
//   2. There is NO path-checksum field anywhere in the contract — `diff.py` deliberately
//      never emits the TOC path checksum (it is a checksum of the path, not a content signal).
//      Do NOT add such a member expecting a value; the UI renders a labeled-absence note only
//      (D-04 / Option 1 — see the UI-SPEC field-gap flag).

// ── Status / verdict enums (the frozen literals) ──────────────────────────────────────

// SET-level statuses (diff.py diff_archive_set, :196-216; test_api.py:24).
export type SetStatus = "added" | "removed" | "changed" | "identical" | "fault";

// FILE-level statuses (diff.py diff_virtual_trees, :278-289; test_api.py:25).
// `identical-by-metadata` is a cheap (len,clen) match — NOT hash-verified.
export type FileStatus =
  | "added"
  | "removed"
  | "changed"
  | "identical-by-metadata"
  | "tombstoned";

// Detail content verdict (api.py:208-231 / RESEARCH :422). null = one side absent.
// `content-confirmed-identical` is the ONLY earned confident "identical" (post-xxhash).
export type Verdict =
  | "content-confirmed-identical"
  | "content-changed"
  | "content-indeterminate"
  | null;

// One side's lean metadata (diff.py _side_meta / api.py _merged_meta) — {len, clen} or null.
export interface SideMeta {
  len: number;
  clen: number;
}

// ── /compare/files (file-level diff) ──────────────────────────────────────────────────

// A single lean file-level diff row (diff.py:305). `qualifier` is optional.
// qualifier ∈ tombstoned_left/right, rejected_left/right, error_left/right.
export interface DiffRow {
  path: string;
  status: FileStatus;
  left: SideMeta | null;
  right: SideMeta | null;
  qualifier?: string[];
}

// Per-side summary diagnostics (diff.py:311-320).
export interface SideSummary {
  node_errors: number;
  rejected: number;
  tombstoned: number;
}

// Full /compare/files response — { rows, summary } (FULL, no pagination, D-08).
export interface FilesResponse {
  rows: DiffRow[];
  summary: {
    left: SideSummary;
    right: SideSummary;
    // keys are exactly the FileStatus strings present in the diff.
    status_counts: Record<FileStatus, number>;
  };
}

// ── /compare/set (set-level archive diff) ─────────────────────────────────────────────

// A single set-level archive row (diff.py diff_archive_set, :190-217). Fields are
// conditionally present per the row status — all optional except basename/kind/status.
export interface SetRow {
  basename: string;
  kind: "tree" | "toc";
  status: SetStatus;
  size_delta?: number;
  entry_count_delta?: number;
  version_left?: string | null;
  version_right?: string | null;
  fault_left?: string;
  fault_right?: string;
}

export interface SetResponse {
  rows: SetRow[];
}

// ── /file/detail (drill-in) ───────────────────────────────────────────────────────────

// One shadowed copy of a winner (api.py _node_brief, :268-273).
export interface NodeBrief {
  archive: string;
  kind: "tree" | "toc";
  priority: number;
}

// Per-side content-hash brief (api.py _hash_brief, :276-279). `error` non-null = degraded.
export interface HashBrief {
  hexdigest: string | null;
  error: string | null;
  source_archive: string;
}

// /file/detail 200 body (api.py:230-242).
export interface DetailResponse {
  path: string;
  status: "ok";
  winning_archive_left: string | null;
  winning_archive_right: string | null;
  left: SideMeta | null;
  right: SideMeta | null;
  shadowed_left: NodeBrief[];
  shadowed_right: NodeBrief[];
  hash_left: HashBrief | null;
  hash_right: HashBrief | null;
  verdict: Verdict;
}

// /file/detail (and 400) error envelope (api.py:83 / :198-200).
export interface ErrorDetail {
  error: string;
  path?: string;
  cfg?: string;
}

// ── /installs + request bodies ────────────────────────────────────────────────────────

// One install-picker entry (config.py / api.py:244-246).
export interface InstallEntry {
  name: string;
  cfg_path: string;
}

// A two-install compare request body (api.py PairRequest, :61-65).
export interface PairReq {
  left_cfg: string;
  right_cfg: string;
}

// A drill-in request body (api.py FileDetailRequest, :68-71) — PairReq + path.
export interface FileDetailReq extends PairReq {
  path: string;
}
