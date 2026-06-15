// Status / verdict -> badge descriptor maps (UI-SPEC §Status / Verdict Badge Palette).
//
// THE HARD RULE (D-04 / D-06 — Kenny's #1 correctness pitfall, the honesty distinction):
//   `identical-by-metadata` (a cheap (len,clen) match — NOT hash-verified) must render
//   VISUALLY DISTINCT from `content-confirmed-identical` (a drill-in xxhash match). The
//   confident solid-green "identical" visual is reserved EXCLUSIVELY for the post-hash verdict;
//   the metadata match gets a muted/neutral "≈ metadata" badge with contentVerified:false.
//   Never present the cheap metadata match as a content guarantee.
//
// SET-level and FILE-level statuses are DIFFERENT enums (RESEARCH Pitfall 3) -> TWO separate
// maps: setStatusBadge (over SetStatus) and fileStatusBadge (over FileStatus). verdictBadge
// maps the drill-in Verdict. Every descriptor carries a TEXT `label` so the signal survives
// color-blindness and screen readers (status is never color-only — UI-SPEC §Visual Hierarchy).

import type { FileStatus, SetStatus, Verdict } from "./types";

// `variant` keys the semantic palette; the component maps it onto a shadcn Badge variant /
// className. Keeping it a small string descriptor keeps status.ts pure + unit-testable.
export type BadgeVariant =
  | "success" // green — earned only by an `added` or the post-hash content match
  | "destructive" // red — removed / fault / read error
  | "warning" // amber — changed / indeterminate
  | "neutral" // muted grey — metadata-only / set-level identical
  | "tombstone"; // violet/neutral outline — a deliberate engine removal

export interface BadgeDescriptor {
  label: string;
  variant: BadgeVariant;
  className?: string;
  // true ONLY for content-confirmed-identical (post-xxhash). false / undefined everywhere
  // else — drives the "not content-verified" affordance on the metadata-only badge.
  contentVerified?: boolean;
}

// ── FILE-level status badges (/compare/files row `status`) ─────────────────────────────

const FILE_STATUS_BADGES: Record<FileStatus, BadgeDescriptor> = {
  added: { label: "added", variant: "success" },
  removed: { label: "removed", variant: "destructive" },
  changed: { label: "changed", variant: "warning" },
  // The honesty distinction: muted/neutral, explicitly NOT content-verified, "≈ metadata".
  "identical-by-metadata": {
    label: "≈ metadata",
    variant: "neutral",
    className: "border-dashed",
    contentVerified: false,
  },
  tombstoned: {
    label: "tombstoned",
    variant: "tombstone",
    className: "border-dashed",
  },
};

export function fileStatusBadge(status: FileStatus): BadgeDescriptor {
  return FILE_STATUS_BADGES[status];
}

// ── SET-level status badges (/compare/set row `status`) — SEPARATE map (Pitfall 3) ─────

const SET_STATUS_BADGES: Record<SetStatus, BadgeDescriptor> = {
  added: { label: "added", variant: "success" },
  removed: { label: "removed", variant: "destructive" },
  changed: { label: "changed", variant: "warning" },
  // set-level `identical` is a plain neutral badge — distinct from the file-level
  // `≈ metadata`; the two maps never share a label so they cannot be confused.
  identical: { label: "identical", variant: "neutral" },
  fault: { label: "fault", variant: "destructive" },
};

export function setStatusBadge(status: SetStatus): BadgeDescriptor {
  return SET_STATUS_BADGES[status];
}

// ── content VERDICT badges (/file/detail `verdict`, post-xxhash) ───────────────────────

export function verdictBadge(verdict: Verdict): BadgeDescriptor {
  switch (verdict) {
    case "content-confirmed-identical":
      // The ONLY earned confident "identical" — solid green, content-verified.
      return { label: "= content", variant: "success", contentVerified: true };
    case "content-changed":
      return { label: "≠ content", variant: "destructive" };
    case "content-indeterminate":
      return { label: "? content — read error", variant: "warning" };
    case null:
    default:
      // null = one side absent (added/removed). Defined neutral fallback, never green.
      return { label: "—", variant: "neutral" };
  }
}
