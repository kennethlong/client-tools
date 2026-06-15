// Collapsible set-delta summary strip (D-01): a SECONDARY lens over /compare/set, COLLAPSED
// by default so it does not compete with the focal file-tree. Renders each archive row with
// `setStatusBadge` (the SET-level map — NEVER fileStatusBadge; the two enums are distinct,
// RESEARCH Pitfall 3) and a `fault` row is shown with its fault_left/right text, NEVER dropped
// (UI-SPEC §Copywriting — the backend never 500s, it returns faults).
//
// Cross-filter (D-01): clicking an archive row sets the tree's scopeArchive on App state and
// shows the accent indicator + a clear-scope affordance. Clicking the active row again clears.

import { useState } from "react";
import { ChevronDown, ChevronRight, X } from "lucide-react";
import type { SetRow } from "@/lib/types";
import { setStatusBadge } from "@/lib/status";
import { StatusBadge } from "@/components/StatusBadge";
import { cn } from "@/lib/utils";

function fmtDelta(n: number | undefined): string {
  if (n === undefined || n === 0) return "";
  return n > 0 ? `+${n}` : `${n}`;
}

export function SetDeltaStrip({
  rows,
  scopeArchive,
  onScopeChange,
  loading,
}: {
  rows: SetRow[];
  scopeArchive: string | null;
  onScopeChange: (archive: string | null) => void;
  loading: boolean;
}) {
  const [open, setOpen] = useState(false); // collapsed by default (D-01)

  const changedCount = rows.filter((r) => r.status !== "identical").length;

  return (
    <section className="border-b border-border bg-secondary">
      <div className="flex items-center gap-2 px-4 py-2">
        <button
          type="button"
          onClick={() => setOpen((o) => !o)}
          className="flex items-center gap-1.5 text-base font-semibold text-foreground"
          aria-expanded={open}
        >
          {open ? (
            <ChevronDown className="size-4" />
          ) : (
            <ChevronRight className="size-4" />
          )}
          Set delta
          <span className="font-mono text-sm font-normal text-muted-foreground">
            ({changedCount} of {rows.length} archives differ)
          </span>
        </button>
        {scopeArchive && (
          <button
            type="button"
            onClick={() => onScopeChange(null)}
            className="ml-auto flex items-center gap-1 rounded-md border border-primary/50 bg-primary/10 px-2 py-0.5 text-xs text-foreground"
            title="Clear the set-delta cross-filter"
          >
            <span className="font-mono">scoped: {scopeArchive}</span>
            <X className="size-3" />
          </button>
        )}
      </div>

      {open && (
        <div className="max-h-56 overflow-auto border-t border-border px-2 pb-2">
          {loading && (
            <p className="px-2 py-2 text-sm text-muted-foreground">
              Comparing installations…
            </p>
          )}
          {!loading && rows.length === 0 && (
            <p className="px-2 py-2 text-sm text-muted-foreground">
              No archives in the set diff.
            </p>
          )}
          <ul className="flex flex-col">
            {rows.map((row) => {
              const active = scopeArchive === row.basename;
              const fault =
                row.status === "fault"
                  ? [row.fault_left, row.fault_right]
                      .filter(Boolean)
                      .join(" · ")
                  : null;
              return (
                <li key={`${row.basename}:${row.kind}`}>
                  <button
                    type="button"
                    onClick={() =>
                      onScopeChange(active ? null : row.basename)
                    }
                    className={cn(
                      "flex w-full items-center gap-2 rounded px-2 py-1 text-left hover:bg-accent/50",
                      active && "bg-primary/15 ring-1 ring-primary/40",
                    )}
                    title="Scope the file-tree to this archive's files"
                  >
                    <StatusBadge descriptor={setStatusBadge(row.status)} />
                    <span className="font-mono text-sm text-foreground">
                      {row.basename}
                    </span>
                    <span className="font-mono text-xs uppercase text-muted-foreground">
                      {row.kind}
                    </span>
                    {fmtDelta(row.entry_count_delta) && (
                      <span className="font-mono text-xs text-muted-foreground">
                        {fmtDelta(row.entry_count_delta)} entries
                      </span>
                    )}
                    {fmtDelta(row.size_delta) && (
                      <span className="font-mono text-xs text-muted-foreground">
                        {fmtDelta(row.size_delta)} B
                      </span>
                    )}
                    {fault && (
                      <span className="font-mono text-xs text-destructive">
                        Could not read archive: {fault}
                      </span>
                    )}
                  </button>
                </li>
              );
            })}
          </ul>
        </div>
      )}
    </section>
  );
}
