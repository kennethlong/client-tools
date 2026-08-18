# CONSULT-76 — Recipe & Layer Ecosystem Design (lateral / ecosystem angle)

Scope note: this covers recipe format, layer authoring, the recipe-not-bytes distribution
model, UX, and ecosystem risk/opportunity. It deliberately does not touch binary format specs,
byte-level tables, or formal composition algebra — those are other consultants' angles. Where I
reference the "consistency enforcement" or "IFF repair" mechanisms, I treat them as black boxes
per the evidence pack and design the ecosystem around their existence, not their internals.

---

## 1. The recipe format

### Format choice: TOML, not YAML/JSON/XML

TOML is the right choice for a hand-authored, community-shared, diffable config that has an
ordered list of layers with per-layer nested options:

- **YAML** is the default reflex for "config file that gets shared on forums," but its
  whitespace sensitivity and the `NO`/`off`/`3.10` auto-coercion footguns are a bad fit for a
  community with a decade of copy-paste-from-forum-post muscle memory (this is literally the
  SWG private-server community — cfg files, forum posts, Notepad edits). A malformed YAML
  recipe fails in confusing ways.
- **JSON** has no comments, and a recipe without the ability to say `# why this pin exists`
  next to a version pin is a recipe nobody can safely fork.
- **TOML** gives ordered tables, native comments, unambiguous types, and — critically — reads
  like a `.cfg` file, which is the one file format this community already trusts and
  hand-edits without fear (see AGENTS.md's own `.cfg` BOM-safety notes: these users edit
  config text directly). Low ceremony, low new-syntax tax.
- Reject XML outright: too heavy for a file real people hand-edit on a forum.

### Concrete example

```toml
# my-server.recipe.toml
# A community recipe for the "Ord Mantell RP" cluster.
# Share this file, NOT the archives it produces.

[recipe]
name            = "ord-mantell-rp"
version         = "1.3.0"          # recipe's own version, semver
schema          = "1"              # builder schema version this file targets
description     = "JTL content + ILM lighting, no ILM sound (server has custom ambience)"
author          = "kenny"
source          = "https://github.com/ord-mantell/recipe"   # optional, for update-checking

[base]
# What the builder resolves against on the user's own machine.
# Not a URL to bytes -- a description of what local install must be presented.
kind            = "retail-install"
patch-level     = "14.1"           # the base your layers were authored/tested against
verify          = "manifest"       # "manifest" | "hash" | "none" (see section 3)

[[layer]]
id              = "swgsource-data-bugfixes"
source          = "registry:swgsource/data-bugfixes"
version         = "^2.0"           # caret range: compatible-with pin, not exact-pin
required        = true

  [layer.options]
  include-quest-fixes   = true
  include-collision-fixes = true

[[layer]]
id              = "jtl-space-content"
source          = "registry:swgsource/jtl-space"
version         = "=1.4.2"         # exact pin: this layer is fragile, pin hard
required        = true

[[layer]]
id              = "ilm-lighting"
source          = "registry:community/ilm-lighting"
version         = "^3.1"
required        = false            # opt-in, composition still valid if user skips it

  [layer.options]
  interior-fog    = "off"          # explicit override of a known preference-kill
  intensity       = "reduced"      # layer-defined enum knob

# ILM sound deliberately omitted -- this recipe overrides it out.

[[layer]]
id              = "community/npc-skeleton-lod-fix"
source          = "registry:community/npc-skeleton-lod-fix"
version         = "^1.0"
required        = true
after           = ["ilm-lighting"] # explicit ordering hint; builder still validates the DAG

[output]
form            = "loose-override"   # "loose-override" | "packed-tre" | "packed-tre+searchtoc"
target          = "./stage-x64/override"
cfg-fragment    = "./stage-x64/recipe.cfg.inc"   # searchPath lines the builder emits/updates

[output.packed]     # only consulted if form starts with "packed-"
archive-name    = "zzz_ord_mantell_rp_00.tre"
compression     = "zlib"
```

Design choices worth calling out:

- **`version` uses Cargo/npm-style ranges** (`^`, `=`, bare) rather than inventing a new
  scheme — this community already understands semver from every other modding ecosystem they
  touch (Minecraft, Vortex/Nexus mods, etc.). `=` pins hard for known-fragile layers (JTL);
  `^` allows a layer maintainer to ship bugfix bumps without breaking every downstream recipe.
- **`base.verify` is a mode, not a hash**, because the base is the user's own retail install
  and the tool cannot ship a hash of bytes it never possessed — see section 3.
- **`after` is an escape hatch, not the primary ordering mechanism.** Primary order is
  array order in `[[layer]]` (TOML preserves array order, unlike a YAML map). `after` exists
  only for the rare cross-cutting fixup layer (like the skeleton/LOD pairing fix) that must
  apply after a specific content layer regardless of where a user pastes it into their list.
- **`output.form` defaulting to `loose-override`** matches the evidence pack's stated v1 gap
  (no TRE writer yet) without baking that limitation into the schema — packed output is a
  first-class future value, not a bolt-on.
- **`registry:` scheme in `source`** deliberately abstracts over "where do I fetch the layer
  package from" (could resolve to a git repo, a CDN, a local path in dev) — see section 3.

### Why not a lockfile-and-manifest split like npm?

Worth flagging as a deliberate deferral, not an oversight: a mature version of this ecosystem
would split `recipe.toml` (human-edited, ranges) from `recipe.lock.toml` (machine-generated,
exact resolved versions + content hashes of the layer *packages*, not the base bytes). That's
the same shape as `Cargo.toml`/`Cargo.lock` or `package.json`/`package-lock.json`. For v1, given
the tiny initial layer set (4-5 layers, single maintainer group), the lockfile is over-
engineering — fold it in once `registry:` sources are resolved from more than one place (a
git-pinned commit SHA in `source` doubles as the lock today). Flag this as the natural v2 line.

---

## 2. Layer authoring: what a layer package looks like on disk

A layer is a directory (or a versioned tag in a git repo) with this shape:

```
layers/ilm-lighting/
  layer.toml            # manifest (below)
  provenance.toml        # byte-origin declarations, one entry per asset group
  data/                   # the loose asset tree this layer contributes (TREE, not archive)
    appearance/mob/...
    texture/...
  patches/                # optional: binary/text patches against base-layer files instead
    of shipping full replacements (keeps the layer package small and keeps it from
    ever accidentally containing retail bytes it doesn't own)
  README.md               # human-facing: what this is, why you'd want it, screenshots
```

### `layer.toml`

```toml
[layer]
id              = "ilm-lighting"
version         = "3.1.0"
schema          = "1"
display-name    = "ILM Interior/Exterior Lighting Enhancements"
homepage        = "https://github.com/community/ilm-lighting"

[compat]
base-patch-levels = ["14.0", "14.1"]   # bases this layer is known to compose against
builder-schema    = ">=1"

[[provides]]
# Declares which "paired asset classes" this layer supplies as a complete set.
# The consistency-enforcement pass (owned by the composition engine, not this file)
# consumes this to decide whether a partial replacement is legal.
class           = "npc-skeleton-lod"
members         = ["*.skt", "*.lmg", "*.lod", "*.sat"]
completeness    = "full"        # "full" | "partial-augment" (adds to base set, doesn't replace it)

[[conflicts]]
with            = "legacy-ilm-lighting"
reason          = "supersedes; same asset paths, older revision"
severity        = "hard"        # hard = compose fails; soft = warn and let user override

[[conflicts]]
with            = "no-fog-interiors"
reason          = "both set interior fog state; last-applied wins silently otherwise"
severity        = "soft"

[[requires]]
layer           = "swgsource-data-bugfixes"
version         = ">=2.0"
reason          = "assumes the datatable schema fixes in 2.0"

[options]
  [options.interior-fog]
  type          = "enum"
  values        = ["on", "off", "reduced"]
  default       = "on"

  [options.intensity]
  type          = "enum"
  values        = ["standard", "reduced"]
  default       = "standard"
```

### `provenance.toml` — the piece that makes recipe-not-bytes auditable

This is the file a reviewer (or a skeptical downloader) reads to answer "am I about to run a
tool that's smuggling copyrighted retail bytes into a git repo." Every asset group the layer's
`data/` or `patches/` directory touches gets an entry:

```toml
[[asset]]
path-glob       = "appearance/mob/**"
origin          = "authored-original"
description     = "New lighting rigs authored from scratch for this pack"

[[asset]]
path-glob       = "texture/env/interior_*.dds"
origin          = "derived-from-user-install"
description     = "Generated by tool X (ilm-relight) from the user's own retail texture at compose time; this repo ships the TOOL and PARAMETERS, not the derived texture"
tool            = "registry:tools/ilm-relight@1.2"

[[asset]]
path-glob       = "datatables/lighting/*.iff"
origin          = "authored-original"
description     = "Hand-tuned datatable values, no retail-derived content"
```

Three closed-vocabulary `origin` values map directly onto the trust boundary the ecosystem
must maintain:

- `authored-original` — safe to distribute as bytes in the layer package itself.
- `derived-from-user-install` — the layer package must NOT contain the output bytes; it ships
  a transform (tool + params) that runs against the user's own local base at compose time.
  This is the enforcement point for recipe-not-bytes: a CI check on the layer registry can
  mechanically reject a submitted layer whose `data/` tree contains a path also declared
  `derived-from-user-install`, because that's a smuggling signature.
- `authored-original-patch` (patches dir) — a binary diff against a named base file; also
  safe to distribute (a diff is not the copyrighted whole), and doubles as a smaller download.

This is the one piece of the ecosystem design I'd flag as load-bearing rather than nice-to-
have: without a machine-checkable provenance declaration, "recipe-not-bytes" degrades from a
policy into an honor system, and an honor system is exactly what gets a repo DMCA'd or gets
the project's relationship with the original IP holder poisoned. Reference existing prior art
for this pattern below (section 5).

---

## 3. Recipe-not-bytes distribution model in practice

**Publicly shared / committed to a repo:**
- `recipe.toml` (the user's compose instructions)
- Every `layers/*/layer.toml`, `provenance.toml`, README, and any `authored-original` or
  `authored-original-patch` bytes
- The registry index itself (a flat list of `id -> {git url, version tags}`)

**Never shared, always local:**
- The user's base retail install
- Anything the builder emits under `origin = "derived-from-user-install"`
- The final composed output (packed .tre or loose override tree) — this is a *build artifact*,
  not a distributable; the ecosystem norm should explicitly say "don't upload your built
  override folder," the same norm Minecraft modpack culture enforces around Forge/Fabric jars
  that bundle Mojang assets.

**Local verification that the user's base matches what the recipe expects.** The evidence pack
already gives the tool a version-aware TRE/TOC reader and a same-path content-diff tool
(`tools/tre-compare`), so the verification design should reuse those rather than invent a new
mechanism:

1. **Manifest mode (default, cheap).** The registry ships a small "patch-level manifest" per
   known base version: a list of `{path, size, crc32}` for a representative sample of files
   that are known to differ across patch levels (patch-marker files), not a full hash of the
   entire install. The builder reads these through the existing TRE reader without extracting
   anything, and reports "base looks like patch 14.1" or "doesn't match any known patch level
   — proceeding at your own risk" (never a hard fail — the user owns their bytes, the tool
   doesn't get to refuse to touch them, it just can't promise the layers will compose cleanly).
2. **Hash mode (opt-in, thorough).** Full per-file hash of every path a layer is about to
   touch, before touching it — catches "you have a third-party mod already overwriting this
   exact file" and "your local file is already corrupted per the IFF-repair validator." This
   is the mode a layer author runs before publishing a layer, and the mode a paranoid user
   runs before a big compose.
3. **Layer-package hash (always, cheap).** Every layer package pinned by the lockfile-
   equivalent (the exact `source` + resolved version) is content-hashed at fetch time and
   compared against what the registry index advertises — this is standard supply-chain
   integrity (same shape as npm's `integrity` field), and it's the ONE piece of hashing that
   protects against a compromised registry/CDN rather than against the user's own install
   drifting, so it should not be optional.

The manifest/hash split matters for UX: patch-level detection is inherently fuzzy (a base
install could be legitimately hand-patched, have a third-party mod already in it, etc.), so it
must degrade to a warning, never a hard block — but layer-package integrity is exact and must
hard-block, because that's the ecosystem's actual attack surface (a malicious or broken
registry entry, not a divergent retail install).

---

## 4. User experience

### First run

```
$ tresmith compose my-server.recipe.toml

Resolving layers...
  swgsource-data-bugfixes  ^2.0  -> 2.3.1   (registry: swgsource, cached)
  jtl-space-content        =1.4.2 -> 1.4.2  (registry: swgsource, fetched)
  ilm-lighting              ^3.1  -> 3.1.0   (registry: community, fetched)
  community/npc-skeleton-lod-fix ^1.0 -> 1.0.4 (registry: community, fetched)

Checking your base install (D:\...\SWGSource Client v3.0)...
  Patch-level fingerprint: matches 14.1 (recipe expects 14.1) [OK]

Validating layer graph...
  4 layers, 1 explicit ordering hint, 0 hard conflicts, 1 soft conflict:
    - ilm-lighting sets interior-fog=off; no-fog-interiors layer not present, ok.
  Paired-asset-class check:
    - npc-skeleton-lod: fully covered by community/npc-skeleton-lod-fix [OK]

Dry run (--dry-run to stop here) — 1,847 files will be touched:
  1,203 new  |  611 overridden  |  33 IFF headers auto-repaired  |  0 unresolved conflicts

Proceed? [y/N]
```

Key UX commitments:

- **Dry-run is not a separate mode, it's a phase every run passes through** — the tool always
  shows the plan before touching disk, `--yes` just skips the confirmation prompt. This
  matches the project's own "reversible local changes, verify before declaring done" ethos and
  is the standard trust-building move in every mod manager that survived contact with an
  ecosystem (Mod Organizer 2's plugin conflict pre-check, Vortex's collection install preview).
- **Every warning names the layer, the file class, and the mitigating layer if one is missing**
  — never a bare "conflict detected." The evidence pack's worked example (skeleton/LOD
  invisible-NPC failure) is exactly the class of bug that a silent partial-apply produces; the
  UX contract is that this class of failure becomes a *loud pre-compose error*, not a runtime
  mystery three weeks later.

### Error reporting on consistency-enforcement failure

```
ERROR: paired-asset-class violation

  Layer "some-cosmetics-pack" replaces:
    appearance/mob/rebel_trooper.skt

  but does NOT provide the paired members of class "npc-skeleton-lod":
    appearance/mob/rebel_trooper.lmg   (missing — not in this layer, not in any prior layer)
    appearance/mob/rebel_trooper.lod   (missing)
    appearance/mob/rebel_trooper.sat   (present, from base)

  This exact failure mode has previously caused: NPCs invisible beyond ~10m.

  Fix options:
    1. Add a layer that supplies the missing members (search: `tresmith search --class npc-skeleton-lod`)
    2. Set layer.some-cosmetics-pack.strict = false in your recipe to force-apply anyway (NOT RECOMMENDED)
    3. Remove this layer from your recipe

  Compose aborted. No files were written.
```

The specific move here — **naming the known historical failure mode in the error text** — is
cheap to implement (it's just a string next to the class declaration in whatever registry
holds "known paired classes") and disproportionately valuable for a community-run tool with no
paid support line: it turns "the compose engine yelled at me" into "the compose engine
prevented the exact bug someone already hit," which is the difference between a tool people
trust and a tool people route around with `--force`.

### Reproducibility guarantee

A given `recipe.toml` + its resolved lock state + a given base-install fingerprint must produce
byte-identical output, every time, on every machine. This is what makes "share the recipe, not
the bytes" actually work as a support model — two players on a Discord can diff their
`recipe.lock` (or resolved-versions block) instead of diffing 40,000 files to find out why one
person's client crashes and the other's doesn't. Concretely: pin the compose engine's own
version into the lock too (`schema` in the example above), because a builder upgrade that
changes IFF-repair behavior is itself a reproducibility break.

---

## 5. Risks and novel ideas

### What could kill this ecosystem

- **Registry rot / single point of failure.** If `registry:` resolves to one GitHub org, that
  org going dark kills every recipe that pins to it. Mitigate by making `source` support a
  literal git URL + tag as a first-class alternative to the shorthand registry scheme (already
  implied by the schema above — `registry:` is sugar, not the only form) — so a recipe still
  resolves even if the central index disappears, as long as the author (or a mirror) is
  reachable.
- **Silent drift between "what the recipe says" and "what actually got built"** if the builder
  itself changes behavior between versions without a version pin in the lock. Already
  addressed above by pinning `schema`/engine version into the reproducibility contract — flag
  it here because it's the single most common way this class of tool quietly breaks trust
  (Docker images that aren't actually reproducible across `docker build` runs are the
  cautionary tale from an adjacent ecosystem).
- **Provenance honor-system decay.** If layer submission has no automated provenance check,
  the field will drift to "everyone just says `authored-original`" within a year, because
  that's the path of least resistance and there's no reviewer with the original retail files
  on hand to catch a lie. This is why provenance declarations need a mechanical, not
  documentary, enforcement point (CI check on submitted layers, section 2) — a rule nobody
  can violate by accident is worth ten rules that rely on a maintainer reading carefully.
- **Conflict fatigue.** If every recipe with more than 3 layers throws soft-conflict walls of
  text, users will train themselves to click through warnings, which defeats the loud-failure
  design goal for the one warning that actually matters. Keep the soft/hard split strict and
  resist the temptation to add a third "info" tier that gets lumped in visually with real
  conflicts.

### Ideas worth stealing from other ecosystems

1. **Nexus Mods "Collections" (Vortex)** — a recipe is essentially a Collection manifest. Steal
   their explicit **install-order visualization** (a literal numbered list UI showing what
   overrides what) and their **"revalidate on game update"** flow: when the user's base patch
   level changes, the tool re-runs the manifest check and flags exactly which layers are now
   unverified, rather than silently trusting last time's result.
2. **Cargo's `Cargo.lock` committed-to-repo convention** — for community server operators
   running a shared recipe across a guild/staff team, committing the *lock* (not the recipe's
   loose ranges) to a private ops repo is the reproducibility unlock; steal the norm that the
   lockfile is committed for "applications" (a specific server's live data set) but left
   uncommitted/range-only for "libraries" (a shared community layer meant to be composed into
   many different recipes).
3. **Homebrew's formula "bottle" + build-from-source fallback** — separate the concept of a
   layer's *transform* (script/tool + params, always shippable) from its *cached derived
   output* (only valid for one exact base fingerprint, never shippable, safe to cache locally
   keyed by that fingerprint so a second compose against the same base doesn't re-run an
   expensive relight/regeneration step). This directly reuses the `derived-from-user-install`
   provenance class from section 2 as a caching key, not just a distribution rule.
4. **Debian's `debsums` / package integrity checking** — steal the UX of "verify what's
   installed matches what should be installed" as a standing command
   (`tresmith verify`, runnable anytime, not just at compose time), so a user who suspects a
   third-party tool clobbered a file after the fact has a one-command way to check drift
   against the recipe that's supposed to be authoritative — reusing the exact same tre-compare
   /content-diff infrastructure the evidence pack already has on the shelf.
5. **Minecraft modpack culture's CurseForge/Modrinth split** — a two-tier registry (an open,
   low-friction "anyone can publish" tier for experimental community layers, and a curated
   "verified" tier for the small set of load-bearing layers like SWG-Source bugfixes and the
   paired-asset fixups) lets the ecosystem stay permissive at the edges while keeping the core
   recipes people actually run in production held to the provenance/CI bar. Gate the
   "verified" badge on passing the provenance CI check plus a maintainer with push access to
   the base engine repo, not on popularity — popularity contests are how modpack ecosystems
   accumulate abandoned load-bearing mods.

A sixth idea, smaller but cheap: **steal the "resource pack" naming convention of prefixing
generated pack IDs with a sortable namespace** (Minecraft's `zzz_` trick to force a data pack
last in load order) — the example recipe above already names its packed output
`zzz_ord_mantell_rp_00.tre`, which is exactly this trick, and it's worth codifying as the
default naming template for `output.packed.archive-name` so users don't have to relearn the
override-priority-via-filename convention by trial and error.
