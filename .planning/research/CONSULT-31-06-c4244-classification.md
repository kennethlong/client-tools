# Classification question: x64 compile-only sweep survivors

CONTEXT (treat as given facts, do not re-derive):
- A scratch Debug|x64 COMPILE-ONLY harness (cl /c, no link) compiles 2217 in-scope engine TUs of a
  32-bit Windows game client being prepared for an x64 port. It promotes /we4311 /we4312 /we4244
  (pointer-truncation + narrowing) to ERRORS to generate a worklist.
- A LOCKED design decision (call it D-07) says: audit only genuine width-MOVERS on Windows LLP64 x64
  (pointer-derived fields, size_t, ptrdiff_t, time_t at serialization/packing/truncation sites).
  It EXPLICITLY EXCLUDES the ubiquitous `signed int length = container.size()` count paths as
  pre-existing/benign, and DELIBERATELY does NOT promote C4267 (size_t->smaller) for that reason.
- The sweep emitted: 13x C4235 (__asm illegal on x64), ~10 genuine C4311/C4312 pointer-truncations,
  and 80x C4244.

THE 80 C4244 ARE DOMINATED BY THIS SHAPE (representative samples):
  ConfigFile.cpp(162): conversion from '__int64' to 'int' (initializing)        [x many]
  FileManifest.cpp(128): conversion from '__int64' to 'int'
  BoxTree.cpp(321): conversion from '__int64' to 'int'
  BitStream.cpp(70): conversion from '__int64' to 'int' (return)
  ObjectList.cpp(345): conversion from '__int64' to 'int'
  SwgCuiChatWindow.cpp(1060): conversion from '__int64' to 'int'
  GroupObject.cpp(413): conversion from 'time_t' to 'unsigned int' (return)
  SwgCuiGroup.cpp(269): conversion from 'time_t' to 'long'
  Audio.cpp(1323): conversion from '__int64' to 'int'
  <STL internal>: utility(274)/algorithm(4073)/xutility(4924) conversion from int->char etc.

QUESTION (answer crisply, do NOT lead me to a predetermined answer):
1. The `__int64 -> int` C4244 flood: on x64, std::vector::size() returns size_t (unsigned 64-bit) and
   std::distance / iterator-difference returns ptrdiff_t (__int64). When the result is assigned to an
   `int` count/index local, MSVC fires C4244 (__int64->int), NOT C4267 (size_t->int), because the
   intermediate type is signed __int64. Is the `__int64 -> int` from `.size()`/`distance` the SAME
   semantic class D-07 meant to EXCLUDE (container-count narrowing), even though it surfaces as C4244
   rather than C4267? Or is `__int64 -> int` materially different from `size_t -> int` such that it
   should be treated as in-scope?
2. The `time_t -> int/long/unsigned` C4244 (GroupObject, SwgCuiGroup): is this a genuine BITS-03
   width-mover (time_t is 8 bytes on x64) that D-07 says to AUDIT, or a benign display/duration cast?
   What distinguishes the two?
3. The STL-internal C4244 (in <utility>/<algorithm>/<xutility>): these fire INSIDE system headers
   instantiated by our code. Are they actionable defects or noise from promoting /we4244 globally?
