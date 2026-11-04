$ErrorActionPreference = 'Stop'
$tokens = $null; $errors = $null
$path = Join-Path $PSScriptRoot '../tools/capture-modem.ps1'
$ast = [Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
# Evaluate only functions: no hardware port is opened.
$ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false) |
    ForEach-Object { Invoke-Expression $_.Extent.Text }
$writer = $null; $showText = $false; $lastDebugState = ''
$serial = [pscustomobject]@{ Queue = [Collections.Generic.Queue[string]]::new(); Writes = [Collections.Generic.List[string]]::new(); Mode = 'ok'; State = 'ON' }
$serial | Add-Member ScriptMethod ReadExisting { if ($this.Queue.Count) { return $this.Queue.Dequeue() }; return '' }
$serial | Add-Member ScriptMethod Write {
    param($value)
    $this.Writes.Add($value)
    if ($this.Mode -eq 'error') { $this.Queue.Enqueue("DEBUG SAVE ERROR`n") }
    elseif ($this.Mode -eq 'ok' -or $this.Mode -eq 'late') {
        if ($this.Mode -eq 'late') { $this.Queue.Enqueue("DEBUG OFF SAVED`n") }
        if ($value -match '^DEBUG OFF') { $this.State = 'OFF' }
        if ($value -match '^DEBUG ON') { $this.State = 'ON' }
        $this.Queue.Enqueue("123 M0 S1 R0 RX OK`nDE")
        $this.Queue.Enqueue("BUG $($this.State) SA")
        $this.Queue.Enqueue("VED`n")
    }
}
if (-not (Send-DebugCommand 'DEBUG ON' 'ON' -TimeoutMs 500)) { throw 'Fragmented ON ACK not recognized' }
if (-not (Send-DebugCommand 'DEBUG OFF' 'OFF' -TimeoutMs 500)) { throw 'OFF ACK not recognized' }
if (-not (Send-DebugCommand 'DEBUG STATUS' 'ON|OFF' -TimeoutMs 500)) { throw 'STATUS ACK not recognized' }
$serial.Mode = 'silent'; $serial.Queue.Enqueue("DEBUG ON SAVED`n")
if (Send-DebugCommand 'DEBUG ON' 'ON' -TimeoutMs 80) { throw 'Stale ACK accepted' }
$serial.Mode = 'error'; $caught = $false
try { $null = Send-DebugCommand 'DEBUG ON' 'ON' -TimeoutMs 200 } catch { $caught = $true }
if (-not $caught) { throw 'EEPROM error ignored' }
$serial.Mode = 'late'
if (-not (Send-DebugCommand 'DEBUG ON' 'ON' -TimeoutMs 500)) { throw 'Late opposite ACK hides current ACK' }
$serial.Mode = 'ok'; $serial.Writes.Clear()
Invoke-DebugMenuChoice '1'
if ($serial.State -ne 'ON') { throw 'Menu 1 did not enable' }
$Port = 'FAKE'; $Seconds = 0
$OutputFile = Join-Path $PSScriptRoot '../.build/capture-menu-test.log'
if (Test-Path -LiteralPath $OutputFile) { Remove-Item -LiteralPath $OutputFile }
Invoke-DebugMenuChoice '2'
Invoke-DebugMenuChoice '0'
if ($serial.State -ne 'ON' -or ($serial.Writes | Where-Object { $_ -match '^DEBUG OFF' })) { throw 'Capture/exit changed the flag' }
if (-not (Test-Path -LiteralPath $OutputFile)) { throw 'Capture did not save file' }
Invoke-DebugMenuChoice '3'
if ($serial.State -ne 'OFF') { throw 'Menu 3 did not disable' }
Write-Host 'Capture/menu helper tests passed (no hardware port opened).'
