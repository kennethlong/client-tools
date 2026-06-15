// Top install-picker bar (D-01): two install selectors + the primary "Compare Installations"
// button. Populated from GET /installs (api.fetchInstalls). An empty list is a valid empty
// state ("No installations configured."), NOT an error (UI-SPEC §Copywriting Contract).
//
// The Compare button is the screen's single primary action and is DISABLED until both a left
// and a right install are chosen. Visual hierarchy order 2: commands attention before a
// compare, recedes after.

import { useQuery } from "@tanstack/react-query";
import { fetchInstalls } from "@/lib/api";
import type { InstallEntry } from "@/lib/types";
import { Button } from "@/components/ui/button";

// A single native <select> picker. Native select keeps the dense tool simple and is fully
// keyboard/screen-reader accessible without extra wiring. React text-escaping covers the
// option labels (filenames/cfg paths) — only React text children, never raw-HTML injection
// (T-30-03-01: React default text-escaping is the XSS control).
function InstallSelect({
  label,
  installs,
  value,
  onChange,
}: {
  label: string;
  installs: InstallEntry[];
  value: string | null;
  onChange: (cfg: string | null) => void;
}) {
  return (
    <label className="flex flex-col gap-1">
      <span className="text-[13px] font-normal uppercase tracking-wide text-muted-foreground">
        {label}
      </span>
      <select
        className="h-8 min-w-[18rem] rounded-md border border-input bg-secondary px-2 font-mono text-sm text-foreground outline-none focus-visible:ring-2 focus-visible:ring-ring/50"
        value={value ?? ""}
        onChange={(e) => onChange(e.target.value || null)}
      >
        <option value="">— select install —</option>
        {installs.map((inst) => (
          <option key={inst.cfg_path} value={inst.cfg_path}>
            {inst.name}
          </option>
        ))}
      </select>
    </label>
  );
}

export function InstallPicker({
  leftCfg,
  rightCfg,
  onLeftChange,
  onRightChange,
  onCompare,
  comparing,
}: {
  leftCfg: string | null;
  rightCfg: string | null;
  onLeftChange: (cfg: string | null) => void;
  onRightChange: (cfg: string | null) => void;
  onCompare: () => void;
  comparing: boolean;
}) {
  const {
    data: installs,
    isLoading,
    isError,
    error,
  } = useQuery({
    queryKey: ["installs"],
    queryFn: fetchInstalls,
    staleTime: Infinity,
  });

  // Empty-state: GET /installs returned [] (or hasn't loaded). Distinct from an error.
  const list = installs ?? [];
  const noInstalls = !isLoading && !isError && list.length === 0;

  const canCompare = !!leftCfg && !!rightCfg && !comparing;

  if (isError) {
    return (
      <div className="border-b border-border bg-secondary px-4 py-3 text-sm text-destructive">
        Could not load installs: {(error as Error)?.message ?? "unknown error"}
      </div>
    );
  }

  if (noInstalls) {
    return (
      <div className="border-b border-border bg-secondary px-4 py-4">
        <h2 className="text-base font-semibold text-foreground">
          No installations configured.
        </h2>
        <p className="mt-1 text-sm text-muted-foreground">
          Add installs to <code className="font-mono">installs.toml</code> (see{" "}
          <code className="font-mono">installs.toml.example</code>), then reload.
        </p>
      </div>
    );
  }

  return (
    <div className="flex flex-wrap items-end gap-4 border-b border-border bg-secondary px-4 py-3">
      <InstallSelect
        label="Left install"
        installs={list}
        value={leftCfg}
        onChange={onLeftChange}
      />
      <InstallSelect
        label="Right install"
        installs={list}
        value={rightCfg}
        onChange={onRightChange}
      />
      <Button
        onClick={onCompare}
        disabled={!canCompare}
        className="h-8"
        title={
          canCompare
            ? "Diff the two installs' TRE sets"
            : "Pick a left and right install first"
        }
      >
        {comparing ? "Comparing…" : "Compare Installations"}
      </Button>
      {isLoading && (
        <span className="text-sm text-muted-foreground">Loading installs…</span>
      )}
    </div>
  );
}
