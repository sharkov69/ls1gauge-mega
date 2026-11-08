param([string]$VsDevCmd = 'C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Auxiliary/Build/vcvars64.bat')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force '.build/host' | Out-Null
    $names=@('vpw_tests','modem_tests','modem_debug_tests','storage_tests','four_dpid_tests','dpid_integration_tests','fallback_tests','csv_upload_tests','csv_queue_tests','csv_storage_tests','csv_modem_tests','display_ui_tests')
    foreach($name in $names) {
        $testSource = "tests/host/$name.cpp"
        $testFlags = ''
        if ($name -eq 'display_ui_tests') {
            $testSource = 'tests/graphics/display_trace.cpp'
            $testFlags = '/DUI_TEST /I tests/graphics'
        }
        @"
@echo off
call "$VsDevCmd" >nul
cl /nologo /std:c++17 /EHsc /W4 $testFlags /I tests/host "$testSource" /Fo".build/host/$name.obj" /Fe".build/host/$name.exe"
if errorlevel 1 exit /b 1
".build/host/$name.exe"
"@ | Set-Content -LiteralPath '.build/host/run.cmd' -Encoding ascii
        & $env:ComSpec /d /c .build\host\run.cmd
        if($LASTEXITCODE -ne 0){throw "$name failed ($LASTEXITCODE)"}
    }
} finally { Pop-Location }
