@echo off
REM Phase 19 — determine if the invalid-op flag is ALREADY pending at createVertexShader
REM entry (engine NaN upstream) vs set inside D3DXCompileShader (D3DX-internal).
set "_NT_SYMBOL_PATH=srv*C:\symbols*https://msdl.microsoft.com/download/symbols;D:\Code\swg-client-v2\stage"
cd /d "D:\Code\swg-client-v2\stage"
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" -c "$$><D:\Code\swg-client-v2\dbg-d3d9-entry.txt" SwgClient_r.exe
