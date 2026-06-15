import { describe, expect, it } from "vitest";

import { fileStatusBadge, setStatusBadge, verdictBadge } from "./status";

// ── file-level vs verdict honesty distinction (Kenny's #1 pitfall — D-04 / D-06) ───────

describe("fileStatusBadge", () => {
  it("identical-by-metadata is a NEUTRAL/muted badge labelled '≈ metadata', not content-verified", () => {
    const b = fileStatusBadge("identical-by-metadata");
    expect(b.label).toBe("≈ metadata");
    // muted/neutral, NOT a confident solid green
    expect(b.variant).not.toBe("success");
    expect(b.contentVerified).toBe(false);
  });

  it("added/removed/changed map to distinct variants", () => {
    expect(fileStatusBadge("added").variant).toBe("success");
    expect(fileStatusBadge("removed").variant).toBe("destructive");
    expect(fileStatusBadge("changed").variant).toBe("warning");
  });

  it("tombstoned is its own distinct (violet/neutral) outline badge", () => {
    const b = fileStatusBadge("tombstoned");
    expect(b.label.toLowerCase()).toContain("tombstoned");
    // never the same as a confident content match
    expect(b.variant).not.toBe("success");
  });
});

describe("verdictBadge", () => {
  it("content-confirmed-identical is a SOLID GREEN '= content' badge", () => {
    const b = verdictBadge("content-confirmed-identical");
    expect(b.label).toBe("= content");
    expect(b.variant).toBe("success");
    expect(b.contentVerified).toBe(true);
  });

  it("content-changed is a '≠ content' badge", () => {
    const b = verdictBadge("content-changed");
    expect(b.label).toBe("≠ content");
    expect(b.variant).not.toBe("success");
  });

  it("content-indeterminate is an amber/warning badge", () => {
    const b = verdictBadge("content-indeterminate");
    expect(b.variant).toBe("warning");
  });

  it("null verdict (one side absent) yields a defined neutral fallback", () => {
    const b = verdictBadge(null);
    expect(b.label.length).toBeGreaterThan(0);
    expect(b.variant).not.toBe("success");
  });
});

// THE HARD RULE: the cheap metadata match and the post-hash content match must look DIFFERENT.
describe("honesty distinction (HARD RULE)", () => {
  it("identical-by-metadata and content-confirmed-identical have different variant/className", () => {
    const meta = fileStatusBadge("identical-by-metadata");
    const content = verdictBadge("content-confirmed-identical");
    // they must NOT share the same visual styling
    const sameVariant = meta.variant === content.variant;
    const sameClass = (meta.className ?? "") === (content.className ?? "");
    expect(sameVariant && sameClass).toBe(false);
    // and specifically: only the post-hash verdict earns the confident solid green
    expect(content.variant).toBe("success");
    expect(meta.variant).not.toBe("success");
  });
});

// ── set-level map is separate from the file-level map (Pitfall 3) ──────────────────────

describe("setStatusBadge (separate enum + map)", () => {
  it("set-level identical is a neutral badge (NOT identical-by-metadata)", () => {
    const b = setStatusBadge("identical");
    expect(b.label).toBe("identical");
    expect(b.variant).not.toBe("success");
  });

  it("set-level fault is a red/destructive badge", () => {
    expect(setStatusBadge("fault").variant).toBe("destructive");
  });

  it("added/removed/changed mirror the semantic palette", () => {
    expect(setStatusBadge("added").variant).toBe("success");
    expect(setStatusBadge("removed").variant).toBe("destructive");
    expect(setStatusBadge("changed").variant).toBe("warning");
  });

  it("the two maps do not collide: set 'identical' label differs from file '≈ metadata'", () => {
    // a set-only status like "identical" is handled by setStatusBadge, never by the file map.
    expect(setStatusBadge("identical").label).not.toBe(
      fileStatusBadge("identical-by-metadata").label,
    );
  });
});
