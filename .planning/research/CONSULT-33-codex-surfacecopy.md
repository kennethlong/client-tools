TASK (read-only trace; treat facts below as GIVEN, do not re-derive):

In the SWG client D3D9 plugin (src/engine/client/application/Direct3d9/src/win32/), the function
Direct3d9_TextureData::lock()/unlock() has a code path (Direct3d9_TextureData.cpp ~line 499-609) that
runs when a caller locks a texture in a NON-NATIVE pixel format (lockData.getFormat() != m_destFormat).
That path creates a scratch CreateOffscreenPlainSurface in translationTable[lockData.getFormat()] and
uses D3DXLoadSurfaceFromSurface to CONVERT pixels between the scratch format and the native texture
format (lines :549 read-back, :603 write-back). We must remove all D3DX (no x64 d3dx9 lib).

QUESTIONS (trace the call graph, cite file:line):
1. Who calls Texture::lock / lockReadOnly with a format DIFFERENT from the texture's native/destination
   format? Enumerate the call sites across the engine (clientGraphics, clientTextureRenderer,
   clientSkeletalAnimation, clientGame, sharedImage). Is this non-native-format lock path reached
   during normal boot-to-character-select (login + char-select screen + zone-in to Tatooine), or is it
   a tool/editor/screenshot-only path?
2. What concrete (sourceFormat -> destFormat) D3DFORMAT pairs actually flow through it at runtime?
   (e.g. is it always A8R8G8B8 <-> X8R8G8B8, or does it hit DXT1/3/5 compressed <-> uncompressed?)
3. Direct3d9_TextureData::copyFrom() (:681) uses D3DXLoadSurfaceFromSurface with NON-NULL src+dst rects.
   Who calls copyFrom()? Are src and dst the SAME D3DFORMAT and SAME rect dimensions (point copy) or
   different (scaled/converted)?

Output: a call-site list with file:line and a yes/no on "reached during boot-to-char-select", plus the
concrete format pairs. Do not propose fixes; just trace.
