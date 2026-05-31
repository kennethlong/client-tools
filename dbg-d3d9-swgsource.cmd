@echo off
REM Control experiment: run the PRISTINE SWG Source client (MSVC 7.1 / retail D3DX,
REM built 2026-05-05) under the same first-chance c0000090 break. Same assets, same
REM machine, OLD toolchain. If it does NOT trap at the Tatooine spot -> our MSVC-2022
REM D3DX is the regression. If it DOES trap -> assets/environment, not our build.
set "_NT_SYMBOL_PATH=srv*C:\symbols*https://msdl.microsoft.com/download/symbols"
cd /d "D:\Code\SWGSource Client v3.0"
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" -c "sxe 0xc0000090; g" SwgClient_r.exe
