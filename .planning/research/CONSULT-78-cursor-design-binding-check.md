You are reviewing a DESIGN DOCUMENT against the repository at D:\Code\SWG-Toolkit (Electron +
TS + C++ N-API). Read-only. The design proposes a new workspace package. Your job is a
BINDING CHECK, not an opinion on the design's goals: verify that every claim the design makes
about the EXISTING codebase is true, and that its integration points are feasible as written.

The design document: D:\Code\swg-client-v2\.planning\research\TRE-BUILDER-DESIGN-draft.md
Read it first, in full.

Then verify, with file:line evidence per item:

1. Every file, symbol, and pattern the design cites (policyRuleset/policyEvaluator pattern,
   packPatch Path-B require pattern, fixtureRegistry + its enforcing sweep test,
   looseOverrideDeploy, cfgActivator, clientSearchOrder, tocReader, changesetService,
   stageSnapshot, TreVfsBrowser/InspectorStack/MountedArchivesList surfaces, parseIff
   result.defects[], native TreMount resolveChain/vfsEntriesColumnar, buildTre/repackTre
   post-1ecb559 state). Does each exist and behave as the design describes? Flag every
   mischaracterization, however small.
2. The package-layout claim: can a new pure-Node workspace package depend on
   @swg/native-core and @swg/contracts and run under plain node? Check how native-core's
   index.js resolves the .node binary (prebuilds path), whether contracts is consumable
   from Node without a build step (dist/ vs src/ exports), how pnpm workspace + hoisted
   nodeLinker affects a new package, and whether any existing package defines a CLI bin
   (precedent or lack thereof). What build/config files would a new package minimally need
   to match conventions (tsconfig, vitest config, package.json fields)?
3. The "renderer services wrap compose" direction: check whether the named candidate
   services (crc32.ts, pathSafety.ts, tocReader.ts, clientSearchOrder.ts) are actually pure
   (no store/Electron imports) as the design assumes. List their imports.
4. The UI-wiring table (§9): for each row, is the named existing element real and does the
   proposed wiring fit its current props/data flow? (e.g. does the VFS browser actually
   render per-entry chips today; does the inspector card stack accept new card types; is
   the enumerate-only chip per-payload as claimed post-04.3?)
5. The fixture/harness claims: does fixtureRegistry's sweep really fail CI on a format with
   zero fixtures or missing loaderSource? Is there an existing fixtures-real skipIf lane
   pattern to copy for the running-client gate?
6. The .toc writer section: check tocReader.ts's actual record layout constants against the
   design's "uint8,uint8,uint16,5×uint32, 24-byte, CRC-sorted" claim, and whether the
   toolkit has ANY .toc write code the design missed.
7. Anything in the repo the design SHOULD have used but didn't (an existing service,
   pattern, or planned todo that overlaps) — name it neutrally.

Output: a numbered findings list — CONFIRMED / WRONG (with the correct fact) / RISK (true
but fragile) — followed by a short "minimal corrections to the design text" list. Do not
rewrite the design.
