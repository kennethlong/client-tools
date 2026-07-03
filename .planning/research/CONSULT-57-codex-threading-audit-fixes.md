ROLE: repo tracer / call-graph auditor. Read-only. Cite file:line for every claim.

Read the evidence file first: .planning/research/CONSULT-57-EVIDENCE-threading-audit-fixes.md
Then the diff: .planning/research/CONSULT-57-threading-audit-fixes.diff
(The diff is already APPLIED to the working tree you are reading.)

YOUR ANGLE (trace, do not opine on design):

A. COMPLETENESS of Change 1. Enumerate EVERY read or write of TreeFile::ms_searchNodes in the
   tree (src/engine/shared/library/sharedFile/). For each: is it (a) under ms_criticalSection,
   (b) converted to the copySearchNodes snapshot, or (c) NEITHER? Category (c) is a finding.
   Include TreeFile::install's own accesses and any tool/editor TUs that reach the private
   internals. Also check TreeFile_SearchNode.{h,cpp} for anything that walks the node list.

B. LOCK-ORDER CYCLES for Change 2. Build the caller inventory of Texture::fetch and
   Texture::release across src/engine (grep '->fetch ()' / '->release ()' on Texture pointers,
   plus TextureList and StaticShader paths). For each call context, list which locks can
   already be held at that point (terrain ShaderCache m_nodeListLock? ShaderTemplateList
   ms_criticalSection? AsynchronousLoader internals? Direct3d11/Direct3d9 device or
   StateCache locks?). Then trace what ~Texture can acquire (delete m_graphicsData ->
   Direct3d{9,11}_TextureData dtor -> ?). Report any pair of locks acquired in BOTH orders
   anywhere in the tree (a cycle), or state the resulting partial order if none.

C. For Change 3: find any OTHER site besides ClientChunk::create's tile loop that calls
   ShaderCache::createBlendedShader or findCachedShader (grep the whole tree), and any other
   site that deletes ShaderSet objects or clears m_shaderSetList besides ~ClientChunk. Any
   extra producer/consumer breaks the once-per-primitive accounting -- report it.

OUTPUT: three sections (A/B/C), each a table of findings with file:line, and a final
DISSENT/CONFIRM line per change: does the trace support the change as written, or did you
find a concrete gap? Do not review style. Do not suggest alternative designs unless a trace
finding requires it.
