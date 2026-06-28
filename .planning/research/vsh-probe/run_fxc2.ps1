$fxc = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"
$base = "D:\Code\swg-client-v2\.planning\research\vsh-probe"
function Try-Compile($tree,$label){
  Write-Host "=== $label ==="
  Set-Location "$base\$tree"
  & $fxc /nologo /T vs_2_0 /E main /Gec /I "." /Fc out.asm "vertex_program\2d_texture.vsh" 2>&1 | Out-String | Write-Host
  Write-Host "EXIT=$LASTEXITCODE"
  Write-Host ""
}
Try-Compile "tree_ruleE_off" "Rule E OFF (current build) — #pragma def PRESENT, vs_2_0 /Gec"
Try-Compile "tree_ruleE_on"  "Rule E ON  (candidate fix)  — #pragma def STRIPPED, vs_2_0 /Gec"
