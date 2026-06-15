// Summary stats panel (must_haves): renders summary.status_counts via the file-status badge
// map + the per-side node_errors/rejected/tombstoned diagnostics from /compare/files. A thin
// strip above the file-tree so the user sees the diff's shape at a glance. Errors render in
// the destructive color so a degraded side is never silently hidden.

import type { FilesResponse, FileStatus } from "@/lib/types";
import { fileStatusBadge } from "@/lib/status";
import { StatusBadge } from "@/components/StatusBadge";

// Stable display order for the status_counts buckets.
const STATUS_ORDER: FileStatus[] = [
  "added",
  "removed",
  "changed",
  "identical-by-metadata",
  "tombstoned",
];

function SideDiag({
  label,
  side,
}: {
  label: string;
  side: FilesResponse["summary"]["left"];
}) {
  const hasIssue = side.node_errors > 0 || side.rejected > 0;
  return (
    <div className="flex items-center gap-2 font-mono text-xs">
      <span className="uppercase text-muted-foreground">{label}</span>
      <span className={side.node_errors > 0 ? "text-destructive" : "text-muted-foreground"}>
        {side.node_errors} errors
      </span>
      <span className={side.rejected > 0 ? "text-destructive" : "text-muted-foreground"}>
        {side.rejected} rejected
      </span>
      <span className="text-muted-foreground">{side.tombstoned} tombstoned</span>
      {hasIssue && <span className="text-destructive">!</span>}
    </div>
  );
}

export function SummaryStats({
  summary,
}: {
  summary: FilesResponse["summary"];
}) {
  const counts = summary.status_counts ?? ({} as Record<FileStatus, number>);
  return (
    <div className="flex flex-wrap items-center gap-x-4 gap-y-1 border-b border-border bg-background px-4 py-2">
      <div className="flex flex-wrap items-center gap-2">
        {STATUS_ORDER.map((status) => {
          const n = counts[status] ?? 0;
          if (n === 0) return null;
          return (
            <span key={status} className="flex items-center gap-1">
              <StatusBadge descriptor={fileStatusBadge(status)} />
              <span className="font-mono text-sm text-foreground">{n}</span>
            </span>
          );
        })}
      </div>
      <div className="ml-auto flex flex-wrap items-center gap-4">
        <SideDiag label="left" side={summary.left} />
        <SideDiag label="right" side={summary.right} />
      </div>
    </div>
  );
}
