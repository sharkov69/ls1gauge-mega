param([string]$VsDevCmd = 'C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Auxiliary/Build/vcvars64.bat')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force '.build/host' | Out-Null
    $names=@('vpw_tests','modem_tests','storage_tests')
    foreach($name in $names) {
        @"
@echo off
call "$VsDevCmd" >nul
cl /nologo /std:c++17 /EHsc /W4 /I tests/host "tests/host/$name.cpp" /Fo".build/host/$name.obj" /Fe".build/host/$name.exe"
if errorlevel 1 exit /b 1
".build/host/$name.exe"
"@ | Set-Content -LiteralPath '.build/host/run.cmd' -Encoding ascii
        & $env:ComSpec /d /c .build\host\run.cmd
        if($LASTEXITCODE -ne 0){throw "$name failed ($LASTEXITCODE)"}
    }
} finally { Pop-Location }
