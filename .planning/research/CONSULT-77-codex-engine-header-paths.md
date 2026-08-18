You are tracing binary-format code paths in this repository (a Star Wars Galaxies client
engine, MSBuild C++, source under src/engine/). Read-only task. Report with file:line
citations for every claim.

TASK: Produce a complete map of every code path in src/ that READS or WRITES the header of
a `.tre` archive file or a `.toc` (search TOC) file — the first bytes of the file on disk.

For each path, report:

1. The exact struct definition used for the header (file:line, field list, field types),
   and how the struct is populated from disk bytes (raw read? per-field? any byte swapping?).
2. The exact constants the token/magic field and the version field are compared against
   (give the numeric value of each constant and the file:line where it is defined), and
   what BYTE SEQUENCE at file offsets 0..3 and 4..7 satisfies each comparison on a
   little-endian x86/x64 host.
3. The accepted version set per read path, and the behavior on a version OUTSIDE that set
   (return false? FATAL? silent skip? — cite the default/else branch).
4. Write paths: what bytes each writer emits at offsets 0..7 (cite the assignment and the
   write call).

Specific paths to cover (plus any others you find):
- TreeFile::SearchTree constructor and TreeFile::SearchTree::validate in
  sharedFile/src/shared/TreeFile_SearchNode.cpp — note which of the two is reachable at
  runtime (does validate() have callers anywhere in src/?).
- The SearchTOC path (master .toc index): how a .toc file's own header is read/validated
  (magic, version constants), AND how the container .tre files a .toc references are
  opened for payload reads — does that container-read path validate the container's
  header/version at all, or read payloads by offset without header checks?
- TreeFileBuilder (shared/application/TreeFileBuilder/) — the offline packer's header
  write, and any version constants it can emit besides the default.
- Any other tool/utility in src/ that reads or writes these headers.

Also answer: what is sizeof(TableOfContentsEntry) in TreeFile_SearchNode.h, and is there
ANY code path in this engine that parses a TOC record larger than that (e.g. a 32-byte
record variant)? Cite evidence either way.

Do NOT speculate about other codebases or community tools. This repo only. If a question
cannot be answered from this repo, say so explicitly.
