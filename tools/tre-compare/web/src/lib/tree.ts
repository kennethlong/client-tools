// Pure flat-array -> virtualized-folder-tree transforms (D-03; RESEARCH Pattern 1).
//
// THE #1 implementation risk per RESEARCH: turning the flat /compare/files row array into a
// nested, collapsible, virtualizable folder tree WITHOUT hanging at 10k-100k rows. The model:
//   1. buildFolderTree(rows): build the nested tree ONCE per response. O(N * avgDepth).
//   2. computeRollup: a single post-order pass summing descendant status counts into every
//      folder (so folder badge counts + empty-folder suppression are O(1) per folder).
//   3. flattenVisible(root, expanded, accept): re-flatten ONLY the currently-visible rows on
//      every expand/collapse/filter change. O(visible) — walks expanded branches only.
//
// This module is PURE — no React, no DOM, no server. The component memoizes buildFolderTree
// on the response and calls flattenVisible per interaction; tree.ts itself stays framework-free.

import type { DiffRow, FileStatus } from "./types";

// ── node model ────────────────────────────────────────────────────────────────────────

export interface Rollup {
  added: number;
  removed: number;
  changed: number;
  identical: number;
  total: number;
}

export interface FolderNode {
  kind: "folder";
  name: string; // segment name
  path: string; // full folder path (stable key)
  children: Map<string, TreeNode>;
  rollup: Rollup;
}

export interface FileNode {
  kind: "file";
  name: string;
  row: DiffRow;
}

export type TreeNode = FolderNode | FileNode;

// A single flattened, currently-visible row (the array fed to TanStack Virtual).
export type VisibleRow =
  | {
      kind: "folder";
      depth: number;
      path: string;
      name: string;
      rollup: Rollup;
      expanded: boolean;
    }
  | {
      kind: "file";
      depth: number;
      path: string;
      name: string;
      row: DiffRow;
    };

// ── rollup bucketing ────────────────────────────────────────────────────────────────

function emptyRollup(): Rollup {
  return { added: 0, removed: 0, changed: 0, identical: 0, total: 0 };
}

// Map a FILE status onto the rollup bucket it counts toward. `identical-by-metadata` and
// `tombstoned` both count toward `identical` (the "not a real diff add/remove/change" bucket)
// so hide-identical can suppress a folder whose entire rollup is non-changing.
function bumpRollup(rollup: Rollup, status: FileStatus): void {
  rollup.total += 1;
  switch (status) {
    case "added":
      rollup.added += 1;
      break;
    case "removed":
      rollup.removed += 1;
      break;
    case "changed":
      rollup.changed += 1;
      break;
    case "identical-by-metadata":
    case "tombstoned":
      rollup.identical += 1;
      break;
  }
}

function addRollup(into: Rollup, from: Rollup): void {
  into.added += from.added;
  into.removed += from.removed;
  into.changed += from.changed;
  into.identical += from.identical;
  into.total += from.total;
}

// Single post-order pass: sum every descendant's counts into each folder's rollup.
export function computeRollup(folder: FolderNode): Rollup {
  for (const child of folder.children.values()) {
    if (child.kind === "folder") {
      addRollup(folder.rollup, computeRollup(child));
    } else {
      bumpRollup(folder.rollup, child.row.status);
    }
  }
  return folder.rollup;
}

// ── build ─────────────────────────────────────────────────────────────────────────────

// Build the nested folder tree ONCE from the flat row array. The component memoizes this on
// the /compare/files response; tree.ts stays pure (no caching here).
export function buildFolderTree(rows: DiffRow[]): FolderNode {
  const root: FolderNode = {
    kind: "folder",
    name: "",
    path: "",
    children: new Map(),
    rollup: emptyRollup(),
  };

  for (const row of rows) {
    const segs = row.path.split("/");
    const fileName = segs.pop()!;
    let node = root;
    let acc = "";
    for (const seg of segs) {
      acc = acc ? `${acc}/${seg}` : seg;
      let child = node.children.get(seg) as FolderNode | undefined;
      if (!child || child.kind !== "folder") {
        child = {
          kind: "folder",
          name: seg,
          path: acc,
          children: new Map(),
          rollup: emptyRollup(),
        };
        node.children.set(seg, child);
      }
      node = child;
    }
    node.children.set(fileName, { kind: "file", name: fileName, row });
  }

  computeRollup(root);
  return root;
}

// ── flatten (visible rows only) ───────────────────────────────────────────────────────

// Does this folder contain ANY descendant file that passes `accept`? Used to suppress
// now-empty folders under hide-identical (RESEARCH Pitfall 6). Walks the subtree but
// short-circuits on the first accepted file, so it stays cheap.
function folderHasVisibleFile(
  folder: FolderNode,
  accept: (row: DiffRow) => boolean,
): boolean {
  for (const child of folder.children.values()) {
    if (child.kind === "file") {
      if (accept(child.row)) return true;
    } else if (folderHasVisibleFile(child, accept)) {
      return true;
    }
  }
  return false;
}

// Re-flatten ONLY the visible rows whenever `expanded` or the `accept` filter changes.
// O(visible) — only walks expanded branches. A folder with no descendant passing `accept`
// is suppressed entirely (no empty expandable folders — Pitfall 6).
export function flattenVisible(
  root: FolderNode,
  expanded: Set<string>,
  accept: (row: DiffRow) => boolean,
): VisibleRow[] {
  const out: VisibleRow[] = [];

  const walk = (node: FolderNode, depth: number): void => {
    const folders: FolderNode[] = [];
    const files: FileNode[] = [];
    for (const child of node.children.values()) {
      if (child.kind === "folder") folders.push(child);
      else files.push(child);
    }

    for (const f of folders) {
      // Empty-folder suppression: skip a folder whose entire subtree is filtered out.
      if (!folderHasVisibleFile(f, accept)) continue;
      const isExpanded = expanded.has(f.path);
      out.push({
        kind: "folder",
        depth,
        path: f.path,
        name: f.name,
        rollup: f.rollup,
        expanded: isExpanded,
      });
      if (isExpanded) walk(f, depth + 1);
    }

    for (const file of files) {
      if (accept(file.row)) {
        out.push({
          kind: "file",
          depth,
          path: file.row.path,
          name: file.name,
          row: file.row,
        });
      }
    }
  };

  walk(root, 0);
  return out;
}
