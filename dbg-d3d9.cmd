@echo off
REM Phase 19 — D3D9 FP-invalid-op (c0000090) first-chance debug launcher.
REM Launches the Release D3D9 stack under WinDbg x86, breaks first-chance on the FP trap.
set "_NT_SYMBOL_PATH=srv*C:\symbols*https://msdl.microsoft.com/download/symbols;D:\Code\swg-client-v2\stage"
cd /d "D:\Code\swg-client-v2\stage"
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" -c "sxe 0xc0000090; g" SwgClient_r.exe
