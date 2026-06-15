import { describe, expect, it } from "vitest";

import type { DiffRow, FileStatus } from "./types";
import {
  buildFolderTree,
  flattenVisible,
  type FolderNode,
  type VisibleRow,
} from "./tree";

// ── helpers ───────────────────────────────────────────────────────────────────────────

function row(path: string, status: FileStatus): DiffRow {
  return { path, status, left: null, right: null };
}

// Accept every row (no filtering).
const acceptAll = (_r: DiffRow) => true;

// Hide-identical accept fn: drop the cheap metadata-only "identical" rows.
const hideIdentical = (r: DiffRow) => r.status !== "identical-by-metadata";

// Pull the visible folder rows / file rows out of a flatten result.
const folderPaths = (vis: VisibleRow[]) =>
  vis.filter((v) => v.kind === "folder").map((v) => v.path);
const filePaths = (vis: VisibleRow[]) =>
  vis.filter((v) => v.kind === "file").map((v) => v.path);

// ── buildFolderTree ─────────────────────────────────────────────────────────────────

describe("buildFolderTree", () => {
  it("builds nested folder nodes from a flat path array", () => {
    const rows: DiffRow[] = [
      row("a/b/x.dds", "added"),
      row("a/b/y.dds", "changed"),
      row("a/z.dds", "removed"),
      row("top.dds", "added"),
    ];
    const root = buildFolderTree(rows);

    // top-level: folder "a" + file "top.dds"
    const a = root.children.get("a") as FolderNode;
    expect(a).toBeDefined();
    expect(a.kind).toBe("folder");
    expect(a.path).toBe("a");
    expect(root.children.get("top.dds")?.kind).toBe("file");

    // a/b nesting
    const b = a.children.get("b") as FolderNode;
    expect(b).toBeDefined();
    expect(b.kind).toBe("folder");
    expect(b.path).toBe("a/b");
    expect(b.children.get("x.dds")?.kind).toBe("file");
    expect(b.children.get("y.dds")?.kind).toBe("file");
    expect(a.children.get("z.dds")?.kind).toBe("file");
  });

  it("rolls up descendant status counts into every folder", () => {
    // a/b has one added + one changed file -> rollup {added:1, changed:1, total:2}
    const rows: DiffRow[] = [
      row("a/b/x.dds", "added"),
      row("a/b/y.dds", "changed"),
      row("a/z.dds", "removed"),
    ];
    const root = buildFolderTree(rows);
    const a = root.children.get("a") as FolderNode;
    const b = a.children.get("b") as FolderNode;

    expect(b.rollup).toMatchObject({ added: 1, changed: 1, total: 2 });
    expect(b.rollup.removed).toBe(0);

    // folder "a" rolls up b's two files + its own z.dds (removed) -> total 3
    expect(a.rollup).toMatchObject({ added: 1, changed: 1, removed: 1, total: 3 });

    // root rolls up everything
    expect(root.rollup.total).toBe(3);
  });

  it("counts identical-by-metadata files in the rollup `identical` bucket", () => {
    const rows: DiffRow[] = [
      row("a/keep.dds", "identical-by-metadata"),
      row("a/changed.dds", "changed"),
    ];
    const root = buildFolderTree(rows);
    const a = root.children.get("a") as FolderNode;
    expect(a.rollup.identical).toBe(1);
    expect(a.rollup.changed).toBe(1);
    expect(a.rollup.total).toBe(2);
  });
});

// ── flattenVisible ──────────────────────────────────────────────────────────────────

describe("flattenVisible", () => {
  const rows: DiffRow[] = [
    row("a/b/x.dds", "added"),
    row("a/z.dds", "changed"),
    row("top.dds", "added"),
  ];

  it("with an empty expanded set yields only top-level folders + files (collapsed)", () => {
    const root = buildFolderTree(rows);
    const vis = flattenVisible(root, new Set(), acceptAll);

    // top level: folder "a" (collapsed) + file "top.dds" — NOT a/z.dds or a/b
    expect(folderPaths(vis)).toEqual(["a"]);
    expect(filePaths(vis)).toEqual(["top.dds"]);

    // the "a" folder row reports collapsed
    const aRow = vis.find((v) => v.kind === "folder" && v.path === "a");
    expect(aRow?.kind).toBe("folder");
    if (aRow?.kind === "folder") expect(aRow.expanded).toBe(false);
  });

  it("walks into an expanded folder and emits its children at depth+1", () => {
    const root = buildFolderTree(rows);
    const vis = flattenVisible(root, new Set(["a"]), acceptAll);

    // expanding "a" reveals nested folder "a/b" and file "a/z.dds"
    expect(folderPaths(vis)).toEqual(["a", "a/b"]);
    expect(filePaths(vis)).toContain("a/z.dds");
    expect(filePaths(vis)).toContain("top.dds");
    // a/b still collapsed -> its child x.dds is NOT visible
    expect(filePaths(vis)).not.toContain("a/b/x.dds");

    // depths: a=0, a/b=1, a/z.dds=1
    const aRow = vis.find((v) => v.path === "a");
    const bRow = vis.find((v) => v.path === "a/b");
    const zRow = vis.find((v) => v.path === "a/z.dds");
    expect(aRow?.depth).toBe(0);
    expect(bRow?.depth).toBe(1);
    expect(zRow?.depth).toBe(1);
  });

  it("fully expanded emits the deepest file at the correct depth", () => {
    const root = buildFolderTree(rows);
    const vis = flattenVisible(root, new Set(["a", "a/b"]), acceptAll);
    const xRow = vis.find((v) => v.path === "a/b/x.dds");
    expect(xRow).toBeDefined();
    expect(xRow?.depth).toBe(2);
  });

  it("every emitted row carries its full path as a stable key", () => {
    const root = buildFolderTree(rows);
    const vis = flattenVisible(root, new Set(["a", "a/b"]), acceptAll);
    for (const v of vis) {
      expect(typeof v.path).toBe("string");
      expect(v.path.length).toBeGreaterThan(0);
    }
    // unique keys
    const paths = vis.map((v) => v.path);
    expect(new Set(paths).size).toBe(paths.length);
  });

  it("hide-identical drops identical-by-metadata file rows", () => {
    const mixed: DiffRow[] = [
      row("a/keep.dds", "identical-by-metadata"),
      row("a/changed.dds", "changed"),
    ];
    const root = buildFolderTree(mixed);
    const vis = flattenVisible(root, new Set(["a"]), hideIdentical);

    expect(filePaths(vis)).toContain("a/changed.dds");
    expect(filePaths(vis)).not.toContain("a/keep.dds");
  });

  it("hide-identical suppresses a folder whose entire rollup is identical (Pitfall 6)", () => {
    // folder "allsame" contains ONLY identical-by-metadata files; folder "real" has a change.
    const mixed: DiffRow[] = [
      row("allsame/a.dds", "identical-by-metadata"),
      row("allsame/b.dds", "identical-by-metadata"),
      row("real/c.dds", "changed"),
    ];
    const root = buildFolderTree(mixed);
    // expand both so that, WITHOUT suppression, both folder rows would appear.
    const vis = flattenVisible(root, new Set(["allsame", "real"]), hideIdentical);

    // the all-identical folder must NOT appear (no empty expandable folder)
    expect(folderPaths(vis)).not.toContain("allsame");
    // the folder with a real change still appears, with its changed file
    expect(folderPaths(vis)).toContain("real");
    expect(filePaths(vis)).toContain("real/c.dds");
    // and the suppressed folder leaves zero visible rows whose children are all filtered
    expect(filePaths(vis)).not.toContain("allsame/a.dds");
    expect(filePaths(vis)).not.toContain("allsame/b.dds");
  });

  it("suppresses nested empty folders too (deep all-identical subtree)", () => {
    const mixed: DiffRow[] = [
      row("x/y/z/only.dds", "identical-by-metadata"),
      row("keep/real.dds", "changed"),
    ];
    const root = buildFolderTree(mixed);
    const vis = flattenVisible(
      root,
      new Set(["x", "x/y", "x/y/z", "keep"]),
      hideIdentical,
    );
    expect(folderPaths(vis)).not.toContain("x");
    expect(folderPaths(vis)).not.toContain("x/y");
    expect(folderPaths(vis)).not.toContain("x/y/z");
    expect(folderPaths(vis)).toEqual(["keep"]);
  });
});
