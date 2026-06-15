// Renders a pure `BadgeDescriptor` (from lib/status.ts) as a shadcn <Badge>.
//
// status.ts is framework-free and emits a semantic `variant`
// (success/destructive/warning/neutral/tombstone); this is the ONE place that maps
// that semantic variant onto the shadcn Badge variant + the dark-zinc className palette
// (UI-SPEC §Status / Verdict Badge Palette). Keeping the mapping here means the badge
// styling is consistent across the file-tree, set-delta strip, summary stats, and the
// detail panel — and the honesty distinction (≈ metadata vs = content) is structural.
//
// THE HARD RULE (D-04/D-06): solid green is reserved EXCLUSIVELY for `success` (added) and
// the post-xxhash `content-confirmed-identical` verdict. `identical-by-metadata` arrives as
// `neutral` + contentVerified:false → muted/dashed, never green. The descriptor decides;
// this component only paints.

import { Badge } from "@/components/ui/badge";
import {
  Tooltip,
  TooltipContent,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { cn } from "@/lib/utils";
import type { BadgeDescriptor, BadgeVariant } from "@/lib/status";

// Semantic variant → shadcn Badge variant + the dark-zinc semantic color override.
// shadcn ships default/secondary/destructive/outline/ghost/link; we layer explicit
// semantic colors on top so the diagnostic palette is unambiguous and color-blind safe
// (every badge ALSO carries a text label — the className is the secondary differentiator).
const VARIANT_STYLES: Record<
  BadgeVariant,
  { base: "default" | "destructive" | "outline"; className: string }
> = {
  // green — earned only by `added` or the post-hash content match
  success: {
    base: "outline",
    className: "border-emerald-600/60 bg-emerald-950/40 text-emerald-300",
  },
  // red — removed / fault / read error (uses shadcn destructive token)
  destructive: { base: "destructive", className: "" },
  // amber — changed / indeterminate
  warning: {
    base: "outline",
    className: "border-amber-600/60 bg-amber-950/40 text-amber-300",
  },
  // muted grey — metadata-only / set-level identical (must NOT read as green)
  neutral: {
    base: "outline",
    className: "border-zinc-700 bg-zinc-900 text-zinc-400",
  },
  // violet/neutral outline — a deliberate engine removal
  tombstone: {
    base: "outline",
    className: "border-violet-700/60 bg-violet-950/30 text-violet-300",
  },
};

export function StatusBadge({
  descriptor,
  className,
}: {
  descriptor: BadgeDescriptor;
  className?: string;
}) {
  const style = VARIANT_STYLES[descriptor.variant];
  const badge = (
    <Badge
      variant={style.base}
      className={cn(
        "font-mono text-[11px]",
        style.className,
        descriptor.className,
        className,
      )}
    >
      {descriptor.label}
    </Badge>
  );

  // The honesty affordance: a metadata-only match (contentVerified === false) gets a
  // tooltip spelling out that it is NOT content-verified, so the muted badge can never be
  // mistaken for the green post-hash verdict even on a quick scan.
  if (descriptor.contentVerified === false) {
    return (
      <Tooltip>
        <TooltipTrigger asChild>
          <span tabIndex={0}>{badge}</span>
        </TooltipTrigger>
        <TooltipContent>
          Metadata match only (len/clen) — NOT content-verified. Open the file
          to hash its contents.
        </TooltipContent>
      </Tooltip>
    );
  }

  return badge;
}
