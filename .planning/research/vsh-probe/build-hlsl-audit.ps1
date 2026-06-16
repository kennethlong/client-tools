# Build + run the Phase 32-02 Task-2(G) HLSL audit probe (x86).
$ErrorActionPreference = 'Continue'
$root   = "D:\Code\swg-client-v2"
$probe  = "$root\.planning\research\vsh-probe"
$rewSrc = "$root\src\engine\client\application\Direct3d9\src\win32"
$dx9inc = "$root\src\external\3rd\library\directx9\include"
$dx9lib = "$root\src\external\3rd\library\directx9\lib"

# vcvars32 for the x86 cl/link toolchain (VS18 BuildTools)
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"

$out = "$probe\hlsl-audit-probe.exe"
$cmd = @"
call "$vcvars"
cl /EHsc /nologo /Fe:"$out" ^
  /I"$probe" /I"$dx9inc" ^
  "$probe\hlsl_audit_probe.cpp" "$probe\_rew_copy.cpp" ^
  /link /LIBPATH:"$dx9lib" d3d9.lib d3dx9.lib d3dcompiler.lib legacy_stdio_definitions.lib user32.lib gdi32.lib advapi32.lib
"@
$bat = "$probe\_build_hlsl_audit.bat"
Set-Content -Path $bat -Value $cmd -Encoding Ascii
cmd /c $bat
Write-Host "BUILD EXITCODE=$LASTEXITCODE"

if (Test-Path $out) {
    Push-Location "$root\stage"
    & $out "$root\.planning\research\vsh-extract\vertex_shader_constants.inc"
    Pop-Location
} else {
    Write-Host "PROBE EXE NOT BUILT"
}
