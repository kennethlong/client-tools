$fxc = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"
$base = "D:\Code\swg-client-v2\.planning\research\vsh-probe"
Set-Location "$base\audit_ruleE_on"
& $fxc /nologo /T vs_2_0 /E main /Gec /I "." /Fc c_simple.asm "vertex_program\c_simple.vsh" 2>&1 | Out-Null
Write-Host "=== c_simple.asm Registers section (Rule E ON) ==="
Get-Content c_simple.asm | Select-String -Pattern 'Registers|Name|----|c\d+|cbuffer|packoffset|dcl_' | Select-Object -First 40
