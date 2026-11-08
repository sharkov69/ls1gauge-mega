#Requires -Version 5.1
<#
.SYNOPSIS
Creates SD provisioning or records completed hardware TLS qualification.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CardPath,
    [switch]$TlsTestsPassed
)
$ErrorActionPreference = 'Stop'
$folder = (Resolve-Path -LiteralPath $CardPath).Path
$configPath = Join-Path $folder 'P59.CFG'
$markerPath = Join-Path $folder 'P59.TLS'
if ($TlsTestsPassed) {
    $config = [IO.File]::ReadAllBytes($configPath)
    if ($config.Length -ne 40) { throw 'P59.CFG must contain exactly 40 bytes.' }
    [byte[]]$marker = [Text.Encoding]::ASCII.GetBytes('TLS1') + $config[32..39]
    [IO.File]::WriteAllBytes($markerPath, $marker)
    [Array]::Clear($config, 0, $config.Length)
    Write-Output 'Created P59.TLS. Remove this marker after changing modem firmware or TLS trust settings.'
    return
}
if (Test-Path -LiteralPath $configPath) { throw 'P59.CFG already exists. Keep it to preserve file identities.' }
$secret = Read-Host 'Device token (64 hex characters; NOT the Cloudflare token)' -AsSecureString
$pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secret)
try {
    $hex = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer).Trim()
    if ($hex -notmatch '\A[0-9a-fA-F]{64}\z' -or $hex -match '\A0{64}\z') { throw 'Invalid device token.' }
    $config = New-Object byte[] 40
    for ($i = 0; $i -lt 32; $i++) { $config[$i] = [Convert]::ToByte($hex.Substring($i * 2, 2), 16) }
    $namespace = New-Object byte[] 8
    $random = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { do { $random.GetBytes($namespace) } while (($namespace | Where-Object { $_ -ne 0 }).Count -eq 0) }
    finally { $random.Dispose() }
    [Array]::Copy($namespace, 0, $config, 32, 8)
    # CreateNew prevents a race from overwriting an existing installation identity.
    $stream = [IO.File]::Open($configPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try { $stream.Write($config, 0, $config.Length); $stream.Flush() }
    finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $markerPath) { Remove-Item -LiteralPath $markerPath }
    Write-Output 'Created P59.CFG. Uploads remain disabled until hardware TLS tests pass.'
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
    $secret.Dispose()
    $hex = $null
    if ($config) { [Array]::Clear($config, 0, $config.Length) }
}
