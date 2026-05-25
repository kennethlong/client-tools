@echo off
setlocal
set "PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;%PATH%"
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" -vcvars_ver=14.51 > NUL
if errorlevel 1 (
  echo FAIL: vcvars32.bat returned errorlevel %errorlevel% > "D:\Code\swg-client\.planning\phases\09-stlport-msvc-stl\baseline\wave1-boost-smoke.txt"
  exit /b 1
)
cl /nologo /c /EHsc /std:c++17 /MT /D_HAS_AUTO_PTR_ETC=1 /I "D:\Code\swg-client\src\external\3rd\library\boost" "D:\Code\swg-client\.planning\phases\09-stlport-msvc-stl\baseline\boost-smoke-test.cpp" /Fo:"D:\Code\swg-client\.planning\phases\09-stlport-msvc-stl\baseline\boost-smoke-test.obj" > "D:\Code\swg-client\.planning\phases\09-stlport-msvc-stl\baseline\wave1-boost-smoke.txt" 2>&1
echo exitCode=%errorlevel% >> "D:\Code\swg-client\.planning\phases\09-stlport-msvc-stl\baseline\wave1-boost-smoke.txt"
exit /b %errorlevel%
