# CONSULT-74 / Cursor — x64 correctness + toolchain/build, side by side

READ FIRST (locked facts, do not re-derive):
  D:\Code\swg-client-v2\.planning\research\CONSULT-74-EVIDENCE-two-fork-comparison.md

  Repo A: D:\Code\swg-client-v2
  Repo B: D:\Code\Galaxies-Reborn\client-tools  (see branch table in the evidence pack;
          origin/x64-dx9-vanilla is the base x64 line, origin/x64-dx11-vanilla builds on it)

You are the crew's most detailed code reader. Give me file:line TABLES, not prose opinions.

1. POINTER-WIDTH / 64-BIT CORRECTNESS. How does each tree handle the classic hazards:
   pointer-to-int casts, `unsigned long` assumptions, `%d`-on-size_t format strings,
   hand-rolled alignment/padding, struct layout assumptions, hashes keyed on pointers,
   inline asm removal. Enumerate the sites each one touched and note where they touched the
   SAME site DIFFERENTLY.
2. NETWORK/ARCHIVE SERIALIZATION. Repo B added sharedUtility/PackedArchive.{h,cpp} and split
   several sharedNetworkMessages *Archive files into explicit .cpp/.h. Determine exactly what
   problem that solves and how Repo A solved (or did not solve) the same problem. This is
   wire-format-adjacent: flag ANY risk of client/server protocol divergence in either tree.
3. THIRD-PARTY / LEGACY SDK. Repo A removed D3DX and the legacy DirectX SDK from the shipping
   path; Repo B's README lists the DirectX SDK (June 2010) as a build prerequisite and keeps
   d3dx9.lib on gl05. Establish factually: what each tree still depends on at BUILD time and at
   RUNTIME (imports), for both platforms. Include dpvs, Miles/JUCE, Qt, SDL3, Vivox, libMozilla.
4. BUILD SYSTEM. Repo B has Directory.Build.props/.targets, .clang-format, and
   scripts/Build-Client.ps1 (matrix build + PE machine-type verification). Repo A uses documented
   manual MSBuild with a 5-target convention. Compare: toolset/std level per project, how each
   handles per-platform staging/deployment, and which projects actually build in each tree
   (note: A's Qt editor tools are pre-broken on MSB8066; B vendored Qt3 for win64 — verify
   whether B's editor tools genuinely build).

Finish with: the 10 highest-value things Repo A should take from Repo B, and the 10 highest-value
things Repo B should take from Repo A, each with file:line anchors.
