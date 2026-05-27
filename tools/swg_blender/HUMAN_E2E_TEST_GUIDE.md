# Human end-to-end test guide (Phase 7)

**Audience:** Someone who can run a game client and follow checklists, but does **not** need Blender or SWG modding expertise.

**Goal:** Confirm that assets produced by our pipeline look correct **inside the running SWG client**. Automated tests already cover file formats and Blender round-trip; this guide is the last step.

**Time:** About 30–60 minutes the first time (mostly waiting for client restarts).

---

## What you are proving

| Layer | Who runs it | What it checks |
| --- | --- | --- |
| Automated (pytest) | Developer / CI | IFF files parse, export bundles build, Blender import→export matches golden data |
| **You (this guide)** | Human tester | Client actually **loads** those files and **draws** them correctly |

You are **not** expected to use Blender or edit meshes. You **are** expected to launch the client, look at objects in the world, and tick pass/fail boxes.

---

## Words you will see

| Term | Plain meaning |
| --- | --- |
| **Bundle** | A folder of game files (meshes, shaders) laid out like retail `serverdata` |
| **Mount / search path** | Telling the client: “look in this folder first when loading assets” |
| **Static mesh** | A non-moving prop (furniture, crate, building piece) — file ends in `.msh` |
| **Skeletal mesh** | A skinned body part tied to bones — file ends in `.mgn` |
| **Pink / magenta** | Classic “missing texture/shader” color in SWG — **fail** |
| **Spaghetti mesh** | Limbs stretched to wrong places — skinning **fail** |
| **Golden files** | Small retail samples copied into the repo for repeatable tests |

---

## Before you start — get these from a developer

Ask the project dev to confirm or send:

1. **Built client** — `SwgClient.exe` (or their Debug build) that reaches **character select** or in-world play.
2. **Retail game data** — Full SWG install (`.tre` archives) on disk; path already in `client.cfg` / `client_d.cfg`.
3. **Local login** — VM or test server so you can log in (see repo `docs/build.md` Phase 6 notes).
4. **Python 3** on your PC — to run the bundle script (same machine is fine).
5. **Repo clone** — `swg-client-v2` with `tools/swg_blender/tests/golden/` populated **or** `SWG_MAIN` env var pointing at a `swg-main` tree with `serverdata` (dev can run export for you if this is missing).
6. **Admin / slash commands** (optional) — Only needed if the “easy” in-world tests are not enough; see [Skeletal test — dev assist](#skeletal-test--dev-assist-optional).

Copy this template into chat/email:

```text
Please confirm for Phase 7 human E2E:
- Client exe path: ___________
- Active cfg file path: ___________
- Retail SWG data path (searchPath): ___________
- Login works (Y/N): ___
- Can I use admin slash commands (Y/N): ___
- Golden export works on my machine (Y/N) or please send a pre-built bundle zip
```

---

## Part A — Computer checks (no game)

Do this at a PowerShell prompt. You do not need Blender installed.

### A1. Open the tools folder

```powershell
cd D:\Code\swg-client-v2\tools\swg_blender
```

(Change the drive/path if your clone lives elsewhere.)

### A2. Run automated preflight

```powershell
python -m pytest tests/test_export_bundle.py tests/test_phase7_bundle.py -q
```

**Pass:** Last line says `passed` (no `FAILED`).

**Fail:** Send the full terminal output to the dev. Do not continue to in-game tests until this passes.

### A3. Build the Phase 7 test bundle

```powershell
python -m swg_pipeline.export_bundle phase7 --output-dir D:\swg_dev_bundle
```

Optional (copies some texture files into the bundle; not required if retail TREs already have them):

```powershell
python -m swg_pipeline.export_bundle phase7 --output-dir D:\swg_dev_bundle --copy-textures
```

**Pass:** Command finishes with no Python traceback, and this folder exists:

`D:\swg_dev_bundle\`

Check these files exist (File Explorer is fine):

| File | Purpose |
| --- | --- |
| `appearance\mesh\frn_all_bed_sm_s1_l0.msh` | Static bed test |
| `appearance\mesh\abyssin_m_arms_l0.mgn` | Skeletal arms test |
| `appearance\skeleton\all_b.skt` | Humanoid skeleton |
| `shader\frn_all_bed_sm_s1_as9.sht` | Bed shader |
| `shader\abyssin_body.sht` | Body shader |
| `client_search_paths.cfg` | Snippet to paste into client config |
| `PHASE7_SPAWN_NOTES.md` | Auto-generated technical notes |
| `swg_phase7_manifest.json` | Machine-readable list of assets |

**Fail:** `FileNotFoundError` about golden files or `swg-main` — ask dev to copy golden files or set `SWG_MAIN`.

### A4. Optional — Blender round-trip (dev machine only)

If Blender is installed, devs can run:

```powershell
python -m pytest tests/test_e2e_blender_roundtrip.py -q
```

You can **skip** this for human E2E; it does not replace the in-game check.

---

## Part B — Mount the bundle in the client

The client must load **your** `D:\swg_dev_bundle` files **instead of** the copies inside retail `.tre` archives. That is done with a **search path**.

### B1. Find the active config file

Usually next to the executable, e.g.:

- `build\bin\Debug\client.cfg` or `client_d.cfg`
- Or a copy the dev told you to edit

Open it in Notepad (or VS Code). **Back up the file first** (copy to `client.cfg.bak`).

### B2. Add the bundle path

Find the section `[SharedFile]`.

Add a line so the bundle has **high priority** (lower index number = higher priority in many setups). The export script writes a ready-made line in `D:\swg_dev_bundle\client_search_paths.cfg` — **copy that line** into your cfg.

Example (paths must match **your** PC):

```ini
[SharedFile]
searchPath0=D:\swg_dev_bundle
searchPath1=C:\SWG
searchPath2=C:\SWG\patch
```

Rules:

- Use forward slashes or escaped backslashes; Windows paths like `D:\swg_dev_bundle` are OK.
- Put `searchPath0` (or the highest-priority slot your dev uses) **above** retail paths.
- Do **not** delete retail paths — the game still needs them for everything else.

### B3. Restart the client completely

Quit the game fully and start again. A “return to character select” hot reload is **not** enough.

### B4. Sanity check — log file

After boot (or after login), open `log.txt` in the client directory.

Search for (`Ctrl+F`):

- `frn_all_bed_sm_s1_l0`
- `abyssin_m_arms_l0`
- `Could not find`
- `Unable to load`

**Pass:** No errors mentioning those assets during the tests below.

**Fail:** Note the exact error lines and send them to the dev with your cfg snippet.

---

## Part C — Static prop test (small bed)

**Asset:** `frn_all_bed_sm_s1_l0.msh` — a small bed from retail furniture.

### Why this is the easy test

Retail worlds already place furniture that uses this mesh. With the bundle mounted at highest priority, the client should load **your exported copy** of the same file path — you do not need to spawn anything special if you can find a bed in the world.

### C1. Find a bed in the game

Try any of these (dev can tell you what works on your server):

- A player house with a small bed
- A city / cantina interior with bedroom furniture
- Admin-placed furniture if slash commands are enabled

If you cannot find one, ask a dev to spawn or place any object whose template uses mesh path `appearance/mesh/frn_all_bed_sm_s1_l0.msh` (see `PHASE7_SPAWN_NOTES.md` in the bundle).

### C2. Visual checklist — static bed

Stand close enough to see the whole object. Compare to a normal retail bed if you can (same area, before/after mount).

| # | Check | Pass | Fail |
| --- | --- | --- | --- |
| C2a | Object is **visible** (not invisible) | ☐ | ☐ |
| C2b | **Not pink/magenta** | ☐ | ☐ |
| C2c | **Right size** (not tiny giant) | ☐ | ☐ |
| C2d | **Sits on floor** reasonably (not floating far above or buried) | ☐ | ☐ |
| C2e | **Facing** looks normal (not lying on side unless it should) | ☐ | ☐ |
| C2f | **Texture** looks like a bed (fabric/wood), not flat gray | ☐ | ☐ |

**Screenshot tip:** Take one wide shot and one close-up. Name files `phase7-static-pass.png` or `phase7-static-fail.png`.

### C3. Control test (recommended once)

1. Remove or comment out the `searchPath` line for `D:\swg_dev_bundle`.
2. Restart client, view the **same** bed again.
3. Re-enable bundle path and restart.

You should see **no change** if the export matches retail (expected for golden bed). If the bed **only** looks wrong with the bundle mounted, note that — it means the export changed something.

---

## Part D — Skeletal test (humanoid arms)

**Assets:** `abyssin_m_arms_l0.mgn` + skeleton `all_b.skt`

This is **harder** than the bed: it is only the **arms** mesh for the Abyssin body, not a full character. Many testers will need **dev assist** to see it on a character.

### D1. Easy path — ask dev to set up visibility

Ask a developer to do **one** of:

- Spawn an NPC/creature that uses skeleton `appearance/skeleton/all_b.skt` and mesh `appearance/mesh/abyssin_m_arms_l0.mgn`
- Temporarily point an existing humanoid appearance template at those paths
- Provide a test character / mannequin in a test cell

You only need to **look** at the result.

### D2. Visual checklist — skeletal arms

Focus on arms and shoulders (torso may come from other retail parts).

| # | Check | Pass | Fail |
| --- | --- | --- | --- |
| D2a | Arms **visible**, not missing | ☐ | ☐ |
| D2b | **Not pink/magenta** | ☐ | ☐ |
| D2c | **No spaghetti** (vertices stuck to wrong bones / stretched across screen) | ☐ | ☐ |
| D2d | Arms **bend naturally** when character moves (walk, idle fidget) | ☐ | ☐ |
| D2e | **Seams** at shoulders acceptable (minor clipping OK; huge gaps are fail) | ☐ | ☐ |

### D3. Animation (optional)

Bundle includes sample animation `all_b_cbt_pistol_run_ready.ans` (see manifest). Playing a combat idle or pistol-ready anim on the same skeleton is a **bonus** check:

| # | Check | Pass | Fail | Skipped |
| --- | --- | --- | --- | --- |
| D3a | Test anim plays without crash | ☐ | ☐ | ☐ |
| D3b | Arms deform reasonably during anim | ☐ | ☐ | ☐ |

If anim setup is too complex, mark **Skipped** and say why in notes.

### Skeletal test — dev assist (optional)

If slash commands work on your server, dev may give you exact commands. Typical patterns in SWG private servers (names vary):

- `/object createItem ...`
- Admin tool → spawn NPC

Do **not** guess template CRCs; get exact strings from `PHASE7_SPAWN_NOTES.md` or the dev.

---

## Part E — Record your results

Fill this in and send to the team (copy into `PHASE7_VALIDATION.md` or a GitHub issue).

```text
=== Phase 7 human E2E ===
Tester name:
Date:
Client build (exe date or git hash from dev): 
OS:
Bundle path: D:\swg_dev_bundle
Cfg file edited:

Preflight pytest (A2): PASS / FAIL
Bundle export (A3): PASS / FAIL

Static bed (Part C):
  C2a visible: PASS / FAIL
  C2b not pink: PASS / FAIL
  C2c scale: PASS / FAIL
  C2d floor contact: PASS / FAIL
  C2e orientation: PASS / FAIL
  C2f texture: PASS / FAIL
  Notes:

Skeletal arms (Part D):
  Setup: self / dev-assisted / SKIPPED
  D2a–D2e: (list PASS/FAIL per row)
  Animation D3: PASS / FAIL / SKIPPED
  Notes:

Log errors (paste lines): 
Screenshots attached: Y / N
```

### Phase 7 complete when

- [ ] Static bed: all C2 checks **pass** (or dev accepts documented known issues)
- [ ] Skeletal arms: all D2 checks **pass**, or **skipped with dev sign-off**
- [ ] Results recorded and shared

When done, dev updates `docs/research/maya-exporter-parity-checklist.md` milestone **M8** / **L** (client load) items.

---

## Troubleshooting for non-experts

| What you see | Likely cause | What you should do |
| --- | --- | --- |
| Everything pink | Shader or texture not found | Confirm `shader\` folder is inside bundle; confirm search path; send `log.txt` snippet |
| Object invisible | Wrong mesh path or template not using our mesh | Confirm bundle mount; ask dev about spawn template |
| Bed upside-down or sideways | Orientation bug | Note if **only** happens with bundle mounted; send screenshot |
| Arms stretched to sky | Skeleton / mesh mismatch | Tell dev — likely wrong `.skt` paired with `.mgn` |
| Client crashes on login | Unrelated to Phase 7 or bad cfg edit | Restore `client.cfg.bak`; ask dev |
| Export script errors | Missing golden / SWG_MAIN | Ask dev to run export and zip `D:\swg_dev_bundle` for you |
| No bed anywhere to look at | World content | Request dev spawn or place furniture |

---

## What you can skip

| Step | Skip when |
| --- | --- |
| Blender E2E pytest | You are not validating Blender — devs handle that |
| Animation D3 | No one can trigger the test anim without dev tools |
| Skeletal D | Dev agrees static-only sign-off for this milestone (document why) |
| Packing `.tre` | Distribution packaging — not needed to prove load in client |

---

## Quick reference — commands only

```powershell
cd D:\Code\swg-client-v2\tools\swg_blender
python -m pytest tests/test_export_bundle.py tests/test_phase7_bundle.py -q
python -m swg_pipeline.export_bundle phase7 --output-dir D:\swg_dev_bundle
```

Cfg: add `searchPath0=D:\swg_dev_bundle` under `[SharedFile]`, restart client.

---

## Related docs (for developers)

| Doc | Content |
| --- | --- |
| `PHASE7_VALIDATION.md` | Official milestone checklist |
| `CLIENT_SPAWN_CHECKLIST.md` | Technical spawn / bundle layout |
| `PIPELINE.md` | Full CLI reference |
| `AGENTS.md` | Agent gotchas (not needed for human testing) |

---

*Last updated: 2026-05-26 — aligns with Phase 7 bundle (`export_bundle phase7`) and automated Blender E2E tests.*
