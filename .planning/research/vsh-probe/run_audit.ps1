$fxc = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"
$base = "D:\Code\swg-client-v2\.planning\research\vsh-probe"
function C($tree,$vsh,$label){
  Set-Location "$base\$tree"
  $out = & $fxc /nologo /T vs_2_0 /E main /Gec /I "." "$vsh" 2>&1 | Out-String
  $err = ($out -split "`n" | Where-Object { $_ -match 'error X' }) -join '; '
  Write-Host ("{0,-32} {1,-22} EXIT={2}  {3}" -f $label,$vsh,$LASTEXITCODE,$err)
}
Write-Host "--- Rule E ON (candidate fix) ---"
C "audit_ruleE_on" "vertex_program\c_simple.vsh" "override c_simple"
C "audit_ruleE_on" "vertex_program\tfcl.vsh"     "override tfcl"
Write-Host "--- Rule E OFF (pre-fix, expect X4016) ---"
C "audit_ruleE_off" "vertex_program\c_simple.vsh" "override c_simple"
C "audit_ruleE_off" "vertex_program\tfcl.vsh"     "override tfcl"
