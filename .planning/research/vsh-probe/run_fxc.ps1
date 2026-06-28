$fxc = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"
$tree = "D:\Code\swg-client-v2\.planning\research\vsh-probe\tree"
Set-Location $tree
Write-Host "=== A: rewritten 2d_texture, vs_2_0, /Gec (backwards-compat), Rule E OFF (#pragma def PRESENT) ==="
& $fxc /nologo /T vs_2_0 /E main /Gec /I "." /Fc 2d_rewritten_A.asm 2d_rewritten.vsh 2>&1
Write-Host "EXIT_A=$LASTEXITCODE"
