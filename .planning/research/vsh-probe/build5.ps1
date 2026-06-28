$ErrorActionPreference = "Continue"
Get-Process MSBuild,cl,link -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
$msb = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$sln = "D:\Code\swg-client-v2\src\build\win32\swg.sln"
$log = "D:\Code\swg-client-v2\.planning\research\vsh-probe\build5.log"
$exe = "D:\Code\swg-client-v2\src\compile\win32\SwgClient\Debug\SwgClient_d.exe"
if (Test-Path $exe) { Remove-Item $exe -Force; Write-Host "deleted staged exe to force relink" }
$targets = "Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient"
& $msb $sln /t:$targets /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false /m /v:m 2>&1 | Tee-Object -FilePath $log
Write-Host "MSBUILD_EXIT=$LASTEXITCODE"
