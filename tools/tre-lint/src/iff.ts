/**
 * iff.ts — IFF structural size-fit validator (validate ONLY — no repair).
 *
 * Ground truth: swg-client-v2 Iff.cpp (BE tags + BE sizes; FORM payload = 4-byte subtype +
 * children). Retail engines never validate block structure (Iff.cpp:386 gate off in retail),
 * which is why defects like the ILM 16-phantom-byte class shipped — this walker is the
 * tolerate-and-report detector for exactly that class.
 *
 * Defect classes:
 *   iff-over-declared   — a block's declared size runs past its parent/file end
 *                         (the 102-file ILM .skt class: root FORM declares actual+16).
 *   iff-under-declared  — children end before the parent's declared end (slack gap).
 *   iff-truncated       — not enough bytes for a block header where one is required.
 *   iff-trailing-bytes  — bytes after the last top-level block.
 *   iff-zero-advance    — malformed size that would loop; walk aborted.
 *
 * ⚠ Repair note (do NOT derive a repairer from this walker): the field-proven repairer is
 * bottom-up with a siblings-sum-to-parent-end invariant; naive clamp-to-parent-end is wrong
 * for middle siblings (swallows the following ones). See CONSULT-76 §1.2b / R9.
 */

export interface IffDefect {
  kind: "iff-over-declared" | "iff-under-declared" | "iff-truncated" | "iff-trailing-bytes" | "iff-zero-advance";
  /** Chunk path, e.g. "FORM SLOD/FORM 0000/DATA" */
  where: string;
  declared?: number;
  actual?: number;
  detail: string;
}

const FORM = 0x464f524d; // "FORM" read BE

function tagAt(buf: Buffer, o: number): string {
  return buf.toString("latin1", o, o + 4);
}

export function looksLikeIff(buf: Buffer): boolean {
  return buf.length >= 12 && buf.readUInt32BE(0) === FORM;
}

/** Walk one container's children in [start,end). Returns defects; recursion capped. */
function walkChildren(buf: Buffer, start: number, end: number, path: string, defects: IffDefect[], depth: number): void {
  if (depth > 64) {
    defects.push({ kind: "iff-zero-advance", where: path, detail: "recursion depth cap hit (64) — aborting this branch" });
    return;
  }
  let pos = start;
  while (pos < end) {
    if (end - pos < 8) {
      defects.push({
        kind: "iff-truncated",
        where: path,
        actual: end - pos,
        detail: `only ${end - pos} bytes left where a child block header (8) is required`,
      });
      return;
    }
    const tag = tagAt(buf, pos);
    const size = buf.readUInt32BE(pos + 4);
    const childEnd = pos + 8 + size;
    const childPath = `${path}/${tag.trim() || "????"}`;
    if (childEnd > end) {
      defects.push({
        kind: "iff-over-declared",
        where: childPath,
        declared: size,
        actual: end - pos - 8,
        detail: `declares ${size} bytes but only ${end - pos - 8} remain in the parent (over by ${childEnd - end})`,
      });
      return; // cannot trust the walk past a bad size
    }
    if (buf.readUInt32BE(pos) === FORM) {
      if (size < 4) {
        defects.push({ kind: "iff-truncated", where: childPath, declared: size, detail: "FORM payload smaller than its 4-byte subtype" });
      } else {
        const sub = tagAt(buf, pos + 8);
        walkChildren(buf, pos + 12, childEnd, `${path}/FORM ${sub.trim()}`, defects, depth + 1);
      }
    }
    if (childEnd <= pos) {
      defects.push({ kind: "iff-zero-advance", where: childPath, declared: size, detail: "block does not advance — malformed size; walk aborted" });
      return;
    }
    pos = childEnd;
  }
  if (pos !== end) {
    defects.push({
      kind: "iff-under-declared",
      where: path,
      declared: end - start,
      actual: pos - start,
      detail: `children end ${end - pos} bytes before the parent's declared end`,
    });
  }
}

/** Validate a whole IFF payload (may have multiple top-level blocks). */
export function validateIff(buf: Buffer): IffDefect[] {
  const defects: IffDefect[] = [];
  walkChildren(buf, 0, buf.length, "", defects, 0);
  // walkChildren reports pos!==end as under-declared at top level; reclassify the top-level
  // gap as trailing bytes (common + benign-ish) for accurate severity mapping downstream.
  for (const d of defects) {
    if (d.kind === "iff-under-declared" && d.where === "") {
      d.kind = "iff-trailing-bytes";
      d.detail = d.detail.replace("children end", "top-level blocks end");
    }
  }
  return defects;
}
