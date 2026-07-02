# CONSULT-55 (fresh Opus) — Concurrency spec: derive the interleaving; design the minimal correct fix

Read `.planning/research/CONSULT-55-EVIDENCE-BRIEF.md` first (facts are GIVEN). Repo: `D:\Code\swg-client-v2`.
You have read access to the tree.

## Your angle (concurrency / lifetime reasoning):
Model the system precisely and derive how a garbage IFF tag arises:
- Actors: the AsynchronousLoader BACKGROUND thread (`threadRoutine`, guarded by `ms_mutex`) and the MAIN
  thread (completed-request block at AsynchronousLoader.cpp:600-660, then the `Iff` parse in the callback).
- Locks: `AsynchronousLoader::ms_mutex`, `TreeFile::ms_criticalSection` (Mutex over `cachedFilesMap`),
  `ShaderTemplateList::ms_criticalSection` (RecursiveMutex over the shader map + create()).
- The published cached `AbstractFile*` buffers and their ownership/lifetime.

Produce:
1. The precise interleaving(s) — cross-thread AND/OR single-thread re-entrant — that let the bytes the
   main-thread `Iff` parses be freed, overwritten, or decompressed-in-place mid-read, yielding a garbage
   `getCurrentName()`. State any assumption you rely on and how to check it in the source.
2. Rank them by consistency with "intermittent + worse after a rebuild (cold-cache/slower loads)".
3. The MINIMAL correct fix (extend a lock scope / add a refcount or ownership handoff / defer the free /
   copy-on-publish) — and prove it does NOT deadlock given the RecursiveMutex + the two other locks, and
   does not serialize the async loader into uselessness.
4. Also propose a cheap DETECTION instrument (validate the IFF tag/magic right after open, capture buffer
   provenance + thread id) that would confirm the mechanism on the next repro.

Write your final answer to `.planning/research/CONSULT-55-opus.out` and return it.
