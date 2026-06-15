// The focal center pane (D-03): the merged file-tree, virtualized via TanStack Virtual over
// the flattened-visible rows so it never hangs on the real 10k-100k-entry diff.
//
// Model (RESEARCH Pattern 1 / Pitfall 5):
//   - buildFolderTree(rows) ONCE per /compare/files response (useMemo keyed on `rows`).
//   - expanded: Set<string> — starts EMPTY (collapsed to top level by default, D-03).
//   - accept(row): the in-browser filter — status filter + name/path search + hide-identical
//     + the set-delta cross-filter scope (all over the one payload; no server round-trip).
//   - flattenVisible(tree, expanded, accept) per interaction → useVirtualizer (28px FIXED).
//
// Empty-folder suppression under hide-identical is already handled inside flattenVisible
// (it skips a folder with no descendant passing `accept` — Pitfall 6). We just pass accept.
//
// Cross-filter caveat (frozen-contract constraint): /compare/files rows carry NO per-file
// archive field (only drill-in exposes winning_archive). So the set-delta → tree scope is a
// best-effort PATH match on the archive basename stem — the only archive association derivable
// in-browser without a backend change the frozen scope forbids. Documented in the SUMMARY.

import { useMemo, useRef, useState } from "react";
import { useVirtualizer } from "@tanstack/react-virtual";
import { ChevronDown, ChevronRight } from "lucide-react";
import type { DiffRow, FileStatus, SetRow } from "@/lib/types";
import { buildFolderTree, flattenVisible } from "@/lib/tree";
import { fileStatusBadge } from "@/lib/status";
import { StatusBadge } from "@/components/StatusBadge";
import { Input } from "@/components/ui/input";
import { cn } from "@/lib/utils";

// A status filter value — "all" or one of the FileStatus literals.
type StatusFilter = "all" | FileStatus;

const STATUS_FILTERS: { value: StatusFilter; label: string }[] = [
  { value: "all", label: "all" },
  { value: "added", label: "added" },
  { value: "removed", label: "removed" },
  { value: "changed", label: "changed" },
  { value: "identical-by-metadata", label: "≈ metadata" },
  { value: "tombstoned", label: "tombstoned" },
];

// Derive the path-scope token for the set-delta cross-filter from the archive basename:
// strip the .tre/.toc extension and lowercase. Best-effort (see file header caveat).
function scopeToken(scopeArchive: string | null): string | null {
  if (!scopeArchive) return null;
  return scopeArchive.replace(/\.(tre|toc)$/i, "").toLowerCase();
}

export function FileTree({
  rows,
  scopeArchive,
  setRows: _setRows,
  selectedPath,
  onSelect,
}: {
  rows: DiffRow[];
  scopeArchive: string | null;
  setRows: SetRow[];
  selectedPath: string | null;
  onSelect: (path: string) => void;
}) {
  // Build the nested tree ONCE per response (Pitfall 5).
  const tree = useMemo(() => buildFolderTree(rows), [rows]);

  // Filters (all in-browser over the one payload).
  const [expanded, setExpanded] = useState<Set<string>>(new Set()); // collapsed default
  const [search, setSearch] = useState("");
  const [statusFilter, setStatusFilter] = useState<StatusFilter>("all");
  const [hideIdentical, setHideIdentical] = useState(true);

  const token = scopeToken(scopeArchive);

  // The accept predicate — recomputed when any filter input changes so flattenVisible
  // (and its empty-folder suppression) re-derives the visible set.
  const accept = useMemo(() => {
    const q = search.trim().toLowerCase();
    return (row: DiffRow): boolean => {
      if (
        hideIdentical &&
        (row.status === "identical-by-metadata" || row.status === "tombstoned")
      ) {
        return false;
      }
      if (statusFilter !== "all" && row.status !== statusFilter) return false;
      if (q && !row.path.toLowerCase().includes(q)) return false;
      // set-delta cross-filter: best-effort path scope on the archive basename stem.
      if (token && !row.path.toLowerCase().includes(token)) return false;
      return true;
    };
  }, [search, statusFilter, hideIdentical, token]);

  const visible = useMemo(
    () => flattenVisible(tree, expanded, accept),
    [tree, expanded, accept],
  );

  const parentRef = useRef<HTMLDivElement>(null);

  const virtualizer = useVirtualizer({
    count: visible.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 28, // LOCKED 28px (UI-SPEC §Spacing) — do NOT round to 24/32.
    getItemKey: (i) => visible[i].path,
    overscan: 12,
  });

  const toggle = (path: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(path)) next.delete(path);
      else next.add(path);
      return next;
    });
  };

  return (
    <div className="flex h-full flex-col">
      {/* Filter bar */}
      <div className="flex flex-wrap items-center gap-3 border-b border-border px-4 py-2">
        <Input
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          placeholder="Filter by name or path…"
          className="h-8 max-w-xs font-mono text-sm"
        />
        <select
          value={statusFilter}
          onChange={(e) => setStatusFilter(e.target.value as StatusFilter)}
          className="h-8 rounded-md border border-input bg-secondary px-2 text-sm text-foreground outline-none focus-visible:ring-2 focus-visible:ring-ring/50"
        >
          {STATUS_FILTERS.map((f) => (
            <option key={f.value} value={f.value}>
              status: {f.label}
            </option>
          ))}
        </select>
        <label className="flex items-center gap-1.5 text-sm text-foreground">
          <input
            type="checkbox"
            checked={hideIdentical}
            onChange={(e) => setHideIdentical(e.target.checked)}
          />
          Hide identical
        </label>
        <span className="ml-auto font-mono text-xs text-muted-foreground">
          {visible.length} rows visible
        </span>
      </div>

      {/* Virtualized tree */}
      <div ref={parentRef} className="min-h-0 flex-1 overflow-auto">
        {visible.length === 0 ? (
          <div className="flex h-full flex-col items-center justify-center gap-2 px-8 text-center">
            <h2 className="text-xl font-semibold text-foreground">
              No differing files.
            </h2>
            <p className="max-w-md text-sm text-muted-foreground">
              Every file matched by metadata. Turn off &lsquo;Hide
              identical&rsquo; to see all entries, or clear your search.
            </p>
          </div>
        ) : (
          <div
            style={{ height: virtualizer.getTotalSize(), position: "relative" }}
          >
            {virtualizer.getVirtualItems().map((vi) => {
              const item = visible[vi.index];
              const indent = item.depth * 16; // 16px per depth (UI-SPEC §Spacing)
              return (
                <div
                  key={vi.key}
                  data-index={vi.index}
                  className={cn(
                    "absolute left-0 flex w-full items-center gap-2 px-2 text-sm hover:bg-accent/40",
                    item.kind === "file" &&
                      item.path === selectedPath &&
                      "bg-primary/20 ring-1 ring-primary/40",
                  )}
                  style={{
                    height: 28,
                    transform: `translateY(${vi.start}px)`,
                    paddingLeft: indent + 8,
                  }}
                  onClick={() =>
                    item.kind === "folder"
                      ? toggle(item.path)
                      : onSelect(item.path)
                  }
                  role="button"
                  tabIndex={0}
                >
                  {item.kind === "folder" ? (
                    <>
                      {item.expanded ? (
                        <ChevronDown className="size-4 shrink-0 text-muted-foreground" />
                      ) : (
                        <ChevronRight className="size-4 shrink-0 text-muted-foreground" />
                      )}
                      <span className="font-mono text-foreground">
                        {item.name}
                      </span>
                      {/* folder roll-up badge counts */}
                      <span className="flex items-center gap-1.5 font-mono text-xs text-muted-foreground">
                        {item.rollup.added > 0 && (
                          <span className="text-emerald-400">
                            +{item.rollup.added}
                          </span>
                        )}
                        {item.rollup.removed > 0 && (
                          <span className="text-destructive">
                            -{item.rollup.removed}
                          </span>
                        )}
                        {item.rollup.changed > 0 && (
                          <span className="text-amber-400">
                            ~{item.rollup.changed}
                          </span>
                        )}
                        <span className="text-zinc-600">
                          {item.rollup.total}
                        </span>
                      </span>
                    </>
                  ) : (
                    <>
                      <span className="w-4 shrink-0" />
                      <StatusBadge descriptor={fileStatusBadge(item.row.status)} />
                      <span className="truncate font-mono text-foreground">
                        {item.name}
                      </span>
                    </>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
