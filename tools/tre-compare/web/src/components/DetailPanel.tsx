// The drill-in detail panel (D-04): a shadcn Sheet sliding over from the right (~440px) that
// AUTO-FIRES /file/detail when a file is selected.
//
// Stale-select guard (Pitfall 4 / T-30-03-03): the query is KEYED on [left_cfg,right_cfg,path]
// so rapid selection discards the stale prior response — the Sheet always shows the latest
// file's detail. Re-select is instant (server-memoized + query cache).
//
// The honesty distinction (D-04/D-06): the panel's verdict comes from verdictBadge over the
// POST-xxhash `verdict` — `content-confirmed-identical` is the ONLY solid-green "= content".
// The tree's metadata-only "≈ metadata" badge stays visually distinct (neutral/dashed).
//
// CRC labeled-absence (D-04 Option 1, field-gap LOCKED): the four frozen routes expose NO crc
// field; we render ONLY the explanatory note — there is no numeric value read from the
// response, by design (it is a checksum of the PATH, not the contents).

import type { ReactNode } from "react";
import { useQuery } from "@tanstack/react-query";
import { fileDetail, ApiError } from "@/lib/api";
import type { DetailResponse, HashBrief, NodeBrief, SideMeta } from "@/lib/types";
import { verdictBadge } from "@/lib/status";
import { StatusBadge } from "@/components/StatusBadge";
import {
  Sheet,
  SheetContent,
  SheetHeader,
  SheetTitle,
  SheetDescription,
} from "@/components/ui/sheet";
import { Separator } from "@/components/ui/separator";

function FieldRow({ label, children }: { label: string; children: ReactNode }) {
  return (
    <div className="flex items-start gap-3 py-1">
      <span className="w-28 shrink-0 text-[13px] uppercase tracking-wide text-muted-foreground">
        {label}
      </span>
      <div className="min-w-0 flex-1 font-mono text-sm text-foreground">
        {children}
      </div>
    </div>
  );
}

function metaText(m: SideMeta | null): string {
  if (!m) return "—";
  return `len ${m.len} · clen ${m.clen}`;
}

function ShadowedList({ nodes }: { nodes: NodeBrief[] }) {
  if (nodes.length === 0)
    return <span className="text-muted-foreground">none</span>;
  return (
    <ul className="flex flex-col gap-0.5">
      {nodes.map((n, i) => (
        <li key={`${n.archive}:${n.kind}:${i}`} className="text-muted-foreground">
          {n.archive}{" "}
          <span className="text-zinc-600">
            ({n.kind}, prio {n.priority})
          </span>
        </li>
      ))}
    </ul>
  );
}

// Per-side hash brief line. A non-null `error` is the degraded read (content-indeterminate
// driver) and is surfaced in destructive text — never silently hidden.
function HashLine({ label, hash }: { label: string; hash: HashBrief | null }) {
  if (!hash) return null;
  return (
    <div className="flex flex-col">
      <span className="text-[13px] uppercase tracking-wide text-muted-foreground">
        {label}
      </span>
      {hash.error ? (
        <span className="text-destructive">
          read error: {hash.error}
        </span>
      ) : (
        <span className="break-all text-foreground">
          {hash.hexdigest ?? "—"}{" "}
          <span className="text-zinc-600">({hash.source_archive})</span>
        </span>
      )}
    </div>
  );
}

function DetailBody({ detail }: { detail: DetailResponse }) {
  const verdict = verdictBadge(detail.verdict);
  return (
    <div className="flex flex-col gap-2 overflow-auto px-6 pb-6">
      <div className="flex items-center gap-2 py-1">
        <span className="text-[13px] uppercase tracking-wide text-muted-foreground">
          verdict
        </span>
        <StatusBadge descriptor={verdict} />
      </div>

      {detail.verdict === "content-indeterminate" && (
        <p className="text-sm text-amber-400">
          Content indeterminate — a side&rsquo;s bytes could not be read.
        </p>
      )}

      <Separator className="my-1" />

      <FieldRow label="Left">{metaText(detail.left)}</FieldRow>
      <FieldRow label="Right">{metaText(detail.right)}</FieldRow>
      <FieldRow label="Winner L">
        {detail.winning_archive_left ?? "—"}
      </FieldRow>
      <FieldRow label="Winner R">
        {detail.winning_archive_right ?? "—"}
      </FieldRow>
      <FieldRow label="Shadowed L">
        <ShadowedList nodes={detail.shadowed_left} />
      </FieldRow>
      <FieldRow label="Shadowed R">
        <ShadowedList nodes={detail.shadowed_right} />
      </FieldRow>

      <Separator className="my-1" />

      <div className="flex flex-col gap-2">
        <HashLine label="Hash L" hash={detail.hash_left} />
        <HashLine label="Hash R" hash={detail.hash_right} />
      </div>

      <Separator className="my-1" />

      {/* CRC labeled-absence note (D-04 Option 1) — NO numeric value, by design. */}
      <div className="rounded-md border border-dashed border-zinc-700 bg-zinc-900/50 p-3">
        <p className="text-[13px] font-semibold uppercase tracking-wide text-muted-foreground">
          Path-CRC — not a content signal.
        </p>
        <p className="mt-1 text-sm text-muted-foreground">
          Path-CRC is intentionally not shown: it is a checksum of the file{" "}
          <em>path</em>, not its contents, so it cannot indicate a content
          change. See the content verdict above.
        </p>
      </div>
    </div>
  );
}

export function DetailPanel({
  leftCfg,
  rightCfg,
  path,
  onClose,
}: {
  leftCfg: string;
  rightCfg: string;
  path: string | null;
  onClose: () => void;
}) {
  // Auto-fired, KEYED on the cfg pair + path so rapid selection never shows stale (Pitfall 4).
  const query = useQuery({
    queryKey: ["file/detail", leftCfg, rightCfg, path],
    queryFn: () =>
      fileDetail({ left_cfg: leftCfg, right_cfg: rightCfg, path: path! }),
    enabled: !!path,
  });

  const open = !!path;

  // Map the ApiError envelope to the route's error copy (UI-SPEC §Copywriting).
  let errorCopy: string | null = null;
  if (query.isError) {
    const err = query.error;
    if (err instanceof ApiError && err.status === 404) {
      errorCopy = "File not found in either merged tree.";
    } else if (err instanceof ApiError && err.status === 400) {
      errorCopy =
        "Rejected path — this path failed safe-virtual-path validation and cannot be inspected.";
    } else {
      errorCopy = (err as Error)?.message ?? "Detail request failed.";
    }
  }

  return (
    <Sheet open={open} onOpenChange={(o) => !o && onClose()}>
      <SheetContent side="right" className="w-full p-0 sm:max-w-[440px]">
        <SheetHeader className="px-6 pt-6">
          <SheetTitle className="text-base">File detail</SheetTitle>
          <SheetDescription className="break-all font-mono text-xs">
            {path ?? ""}
          </SheetDescription>
        </SheetHeader>

        {query.isLoading && (
          <p className="px-6 text-sm text-muted-foreground">
            Hashing file contents…
          </p>
        )}
        {errorCopy && (
          <p className="px-6 text-sm text-destructive">{errorCopy}</p>
        )}
        {query.data && <DetailBody detail={query.data} />}
      </SheetContent>
    </Sheet>
  );
}
