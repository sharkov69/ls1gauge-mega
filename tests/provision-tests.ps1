$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root ('.build/provision-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder | Out-Null
function Read-Host { param($Prompt, [switch]$AsSecureString) ConvertTo-SecureString ('12' * 32) -AsPlainText -Force }
$scriptPath = Join-Path $root 'tools/New-P59SdConfig.ps1'
& $scriptPath -CardPath $folder
$config = [IO.File]::ReadAllBytes((Join-Path $folder 'P59.CFG'))
if ($config.Length -ne 40 -or ($config[0..31] | Where-Object { $_ -ne 18 }).Count) { throw 'Bad config encoding' }
if (Test-Path (Join-Path $folder 'P59.TLS')) { throw 'Premature TLS qualification' }
$rejected = $false
try { & $scriptPath -CardPath $folder } catch { $rejected = $true }
if (-not $rejected) { throw 'Existing config overwritten' }
& $scriptPath -CardPath $folder -TlsTestsPassed
$marker = [IO.File]::ReadAllBytes((Join-Path $folder 'P59.TLS'))
if ($marker.Length -ne 12 -or [Text.Encoding]::ASCII.GetString($marker, 0, 4) -ne 'TLS1') { throw 'Bad TLS marker' }
if ([Convert]::ToBase64String($marker[4..11]) -ne [Convert]::ToBase64String($config[32..39])) { throw 'Namespace mismatch' }
Write-Output 'SD provisioning tests passed (synthetic token).'
