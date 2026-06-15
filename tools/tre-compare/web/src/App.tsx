// The D-01 linked master-detail layout shell + the shared compare state.
//
// Layout (top → down, linked lenses — NOT tabs):
//   InstallPicker (top bar)  →  SetDeltaStrip (collapsible)  →  SummaryStats  →
//   FileTree (focal center pane)  →  DetailPanel (right Sheet, slides over)
//
// App owns all the shared compare state so the lenses link:
//   - leftCfg / rightCfg          : the picked installs (drive every compare call)
//   - setRows                     : /compare/set rows (the set-delta strip)
//   - files                       : /compare/files {rows, summary} (the heavy payload)
//   - scopeArchive                : the set-delta → tree cross-filter scope
//   - selectedPath                : the file whose detail is open in the Sheet
//
// Fetches use TanStack Query keyed on the cfg pair so a re-compare is cache-correct and the
// keyed detail query (DetailPanel) can dedup/discard-stale (Pitfall 4). Compare is gated on a
// manual trigger: picking installs does not auto-fetch; the user clicks Compare Installations.

import { useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { compareFiles, compareSet } from "@/lib/api";
import type { SetRow } from "@/lib/types";
import { InstallPicker } from "@/components/InstallPicker";
import { SetDeltaStrip } from "@/components/SetDeltaStrip";
import { SummaryStats } from "@/components/SummaryStats";
import { FileTree } from "@/components/FileTree";
import { DetailPanel } from "@/components/DetailPanel";

function App() {
  // ── shared compare state ──────────────────────────────────────────────────
  const [leftCfg, setLeftCfg] = useState<string | null>(null);
  const [rightCfg, setRightCfg] = useState<string | null>(null);
  // The committed pair (set on Compare click) — distinct from the picker selection so the
  // results don't churn while the user re-picks. null until the first Compare.
  const [pair, setPair] = useState<{ left: string; right: string } | null>(null);
  const [scopeArchive, setScopeArchive] = useState<string | null>(null);
  const [selectedPath, setSelectedPath] = useState<string | null>(null);

  // Reset the selection + scope when picking a NEW comparison.
  const onCompare = () => {
    if (!leftCfg || !rightCfg) return;
    setScopeArchive(null);
    setSelectedPath(null);
    setPair({ left: leftCfg, right: rightCfg });
  };

  const enabled = !!pair;

  // /compare/set — the set-delta strip (cheap, runs alongside files).
  const setQuery = useQuery({
    queryKey: ["compare/set", pair?.left, pair?.right],
    queryFn: () =>
      compareSet({ left_cfg: pair!.left, right_cfg: pair!.right }),
    enabled,
  });

  // /compare/files — the heavy payload that feeds the tree + summary.
  const filesQuery = useQuery({
    queryKey: ["compare/files", pair?.left, pair?.right],
    queryFn: () =>
      compareFiles({ left_cfg: pair!.left, right_cfg: pair!.right }),
    enabled,
  });

  const comparing = enabled && (setQuery.isLoading || filesQuery.isLoading);
  const setRows: SetRow[] = setQuery.data?.rows ?? [];
  const files = filesQuery.data ?? null;

  return (
    <div className="flex h-svh flex-col bg-background text-foreground">
      <header className="flex items-baseline gap-3 border-b border-border bg-secondary px-4 py-2">
        <h1 className="text-xl font-semibold tracking-tight">TRE Compare</h1>
        <span className="text-sm text-muted-foreground">
          read-only TRE diff — localhost
        </span>
      </header>

      <InstallPicker
        leftCfg={leftCfg}
        rightCfg={rightCfg}
        onLeftChange={setLeftCfg}
        onRightChange={setRightCfg}
        onCompare={onCompare}
        comparing={!!comparing}
      />

      {enabled && (
        <SetDeltaStrip
          rows={setRows}
          scopeArchive={scopeArchive}
          onScopeChange={setScopeArchive}
          loading={setQuery.isLoading}
        />
      )}

      {files && <SummaryStats summary={files.summary} />}

      {/* Focal center pane */}
      <div className="min-h-0 flex-1">
        {!enabled && (
          <EmptyState
            heading="Pick two installations and Compare."
            body="Choose a left and right install above, then Compare to diff their TRE sets."
          />
        )}
        {enabled && filesQuery.isLoading && (
          <EmptyState heading="Comparing installations…" body="" spinner />
        )}
        {enabled && filesQuery.isError && (
          <EmptyState
            heading="Compare failed."
            body={(filesQuery.error as Error)?.message ?? "unknown error"}
          />
        )}
        {files && (
          <FileTree
            rows={files.rows}
            scopeArchive={scopeArchive}
            setRows={setRows}
            selectedPath={selectedPath}
            onSelect={setSelectedPath}
          />
        )}
      </div>

      {pair && (
        <DetailPanel
          leftCfg={pair.left}
          rightCfg={pair.right}
          path={selectedPath}
          onClose={() => setSelectedPath(null)}
        />
      )}
    </div>
  );
}

function EmptyState({
  heading,
  body,
  spinner,
}: {
  heading: string;
  body: string;
  spinner?: boolean;
}) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-2 px-8 text-center">
      {spinner && (
        <div className="size-6 animate-spin rounded-full border-2 border-muted border-t-foreground" />
      )}
      <h2 className="text-xl font-semibold text-foreground">{heading}</h2>
      {body && <p className="max-w-md text-sm text-muted-foreground">{body}</p>}
    </div>
  );
}

export default App;
