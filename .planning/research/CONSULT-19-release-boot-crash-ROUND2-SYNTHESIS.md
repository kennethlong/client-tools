# Synthesis ROUND 2: D3D11 release boot-crash — VERIFIED root cause

**Date:** 2026-06-02. **Inputs:** `…-ROUND2-codex.out`, `…-ROUND2-cursor.out`, in-dump verification of `release-gl11-crash.dmp`.

## Both consults converged (independently) + I verified in-dump
**The crash is an in-place overwrite ("stomp") of `program->m_graphicsData`, NOT a UAF, teardown, or creation gap.**

### The methodology flaw both AIs caught (and they were right)
My 8 runtime experiments concluded "the assignment never happens / slot never written." That was **unsound**:
- I armed the hardware write-watch *inside* the gl11 PS-data ctor — **before** the line-2770 store (which is in the *Program* ctor, after the call returns) — and **never sampled the slot after 2770**.
- cdb's scripted `ba w4` in 32-bit/Release can silently miss a single `mov [ecx+20h],eax`. So "zero watch hits" ≠ "no store."
- `slotval-at-ctor = 0x21250000` (non-NULL when source says NULL) is **mundane**: Release elides the init-list NULL store (`m_graphicsData(NULL)`) when it's provably written before any read; I sampled recycled adjacent bytes.

### In-dump verification (independent of the AIs — searched `release-gl11-crash.dmp` for refs to program `0x21250010`)
A **live `Direct3d11_PixelShaderProgramData` exists at `0x0a96dee0`**: vtable `= gl11_r!Direct3d11_PixelShaderProgramData::vftable` (0x6106b9f0), and `m_program (+0x04) = 0x21250010` — it **round-trips back to the 2d_texture program**.

→ The PS-data **was created, linked, and is still alive** (never freed; matches zero-dtor finding). But `program(0x21250010)->m_graphicsData` (slot `0x21250030`) holds `0x21250000` (garbage) instead of `0x0a96dee0` (the real PS-data). **The 4-byte slot was overwritten in place** after the legitimate assignment.

### What the stomped value tells us
`0x21250000` = `program_base − 0x10` — a **structured, arena-local value** (looks like a freelist/list-node link), not random garbage. Strong hint the stomper is a list/freelist/array write that aliases the program object's arena memory at `+0x20`. (Program is ordinary-`new` heap per CODEX; PS-data uses its own MBM — the arena overlap is heap locality, not a shared allocator.)

## Falsified / superseded
- Round-1 device-restore UAF (flushResources no-op; snapshot==live). DEAD.
- "Per-object freed/reused", "wholesale pool teardown", "creation gap", "never-linked". ALL SUPERSEDED — the PS-data is alive and linked; the bug is a stray write to one field.

## Fix
- **Immediate (ship): vtable-validity guard** in `Direct3d11_StaticShaderData::apply` (+ construct snapshot): before the Plan-17-03 reflection/material loop, require `*(void**)m_graphicsData == &Direct3d11_PixelShaderProgramData::vftable` (optionally also PS-data `m_program` round-trips). Else skip the loop + fall back — **D3D9-parity** (D3D9's `apply` never runs that loop). Reversible, low-risk, un-sticks release boot. Both consults endorse as correct containment.
- **Caveat:** it's containment, not a root fix. The stray write is real corruption that *could* hit other memory; but DEBUG works fully → it's layout/timing-specific to Release, and the crash is the only observed symptom.

## Open: find the stomper (only if the guard reveals problems / for a real fix)
The cdb scripted `ba w4` is unreliable here. Options, best-first:
1. **Working write-watch:** Kenny sets `ba w4 <program+0x20>` **interactively at a clean break** (after line 2770, confirmed slot==valid PS-data), then `g` to the crash — catches the stomp's stack. (Scripted/in-callback `ba` failed; interactive set at a normal stop is the reliable form.)
2. CODEX's angle: catch the first write/free **at the allocator level** near that address.
3. Bisect neighbors of `0x21250030` (the ShaderImplementation `0x2124fb70`, pass `0x2124fbb0`, gl11 std::vectors) for an overflow.

## Next decisive experiment (single run, fixes my blind spot)
bp at `ShaderImplementation.cpp:2773` (just AFTER the 2770 store), filter `m_fileName` contains `2d_texture`: log `this`, `this->m_graphicsData` (expect the valid PS-data). Confirms linked. Then interactive `ba w4 this+0x20` → `g` → the stomp stack. This is the experiment my 8 runs never did (I only ever watched *before* the store).
