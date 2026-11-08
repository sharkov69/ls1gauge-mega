param(
    [string]$Port,
    [ValidateRange(1,3600)][int]$Seconds = 180,
    [string]$OutputFile,
    [switch]$CaptureOnly
)
$ErrorActionPreference = 'Stop'

function Read-ModemCapture {
    $chunk = $serial.ReadExisting()
    if ($chunk.Length) {
        if ($writer) { $writer.Write($chunk); $writer.Flush() }
        if ($showText) { [Console]::Write($chunk) }
    }
    return $chunk
}
function Send-DebugCommand {
    param([string]$Command,[string]$Expected,[int]$TimeoutMs = 20000)
    $null = Read-ModemCapture # Old input is not an acknowledgement of this request.
    $received = ''; $nextSend = 0
    $deadline = [Diagnostics.Stopwatch]::StartNew()
    while ($deadline.ElapsedMilliseconds -lt $TimeoutMs) {
        if ($deadline.ElapsedMilliseconds -ge $nextSend) {
            $serial.Write($Command + "`n")
            $nextSend = $deadline.ElapsedMilliseconds + 750
        }
        $received += Read-ModemCapture
        if ($received -match '(?m)^DEBUG SAVE ERROR\r?$') { throw 'Arduino zglosilo blad zapisu EEPROM.' }
        if ($received -match ('(?m)^DEBUG (?<state>' + $Expected + ') SAVED\r?$')) {
            $script:lastDebugState = $Matches.state; return $true
        }
        if ($received.Length -gt 512) { $received = $received.Substring($received.Length-512) }
        Start-Sleep -Milliseconds 20
    }
    return $false
}
function Start-ModemCapture {
    if (-not $CaptureOnly) {
        if (-not (Send-DebugCommand 'DEBUG STATUS' 'ON|OFF')) { throw 'Brak odpowiedzi DEBUG STATUS. Sprawdz firmware i port.' }
        if ($lastDebugState -ne 'ON') { Write-Host 'Debug jest OFF. Najpierw wybierz 1. Nie zmieniam flagi.'; return }
    }
    $capturePath = $OutputFile
    if (-not $capturePath) { $capturePath = Join-Path (Get-Location) ('modem-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log') }
    $capturePath = [IO.Path]::GetFullPath($capturePath)
    $stream = $null
    try {
        $stream = [IO.File]::Open($capturePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        $script:writer = [IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
        $writer.WriteLine('# PC start: ' + (Get-Date -Format o) + ' port=' + $Port)
        Write-Host "Zapis do: $capturePath"
        Write-Host "Zbieranie przez $Seconds s. Q konczy zapis i wraca do menu, bez zmiany flagi."
        Write-Host 'Aby zapisac start Arduino, nacisnij teraz RESET na Mega. Modem moze pozostac zasilany.'
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $checkKeyboard = -not [Console]::IsInputRedirected
        while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
            $null = Read-ModemCapture
            if ($checkKeyboard) {
                try {
                    if ([Console]::KeyAvailable -and [Console]::ReadKey($true).Key -eq [ConsoleKey]::Q) { break }
                } catch { $checkKeyboard = $false }
            }
            Start-Sleep -Milliseconds 20
        }
        $null = Read-ModemCapture
        $writer.WriteLine("`n# PC end: " + (Get-Date -Format o))
    } finally {
        if ($writer) { try { $writer.Dispose() } finally { $script:writer = $null } }
        elseif ($stream) { $stream.Dispose() }
    }
    Write-Host "Log zapisany: $capturePath. Flaga debug pozostaje bez zmian."
}
function Invoke-DebugMenuChoice {
    param([string]$Choice)
    switch ($Choice) {
        '1' {
            if (-not (Send-DebugCommand 'DEBUG ON' 'ON')) { throw 'Brak potwierdzenia ON SAVED; stan niepotwierdzony.' }
            Write-Host 'DEBUG ON zapisane w EEPROM. Wybierz 2, aby zbierac log.'
        }
        '2' { Start-ModemCapture }
        '3' {
            if (-not (Send-DebugCommand 'DEBUG OFF' 'OFF')) { throw 'Brak potwierdzenia OFF SAVED; debug moze nadal byc wlaczony.' }
            Write-Host 'DEBUG OFF zapisane w EEPROM.'
        }
        '0' { return }
        default { Write-Host 'Wybierz 1, 2, 3 lub 0.' }
    }
}

if (-not ('System.IO.Ports.SerialPort' -as [type])) { Add-Type -AssemblyName System.IO.Ports }
if (-not $Port) {
    Write-Host 'Dostepne porty:'
    [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    $Port = Read-Host 'Podaj port Arduino, np. COM5 (Enter anuluje)'
    if (-not $Port) { return }
}
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.DtrEnable = $true
$serial.ReadBufferSize = 65536
$serial.ReadTimeout = 200
$serial.WriteTimeout = 500
$writer = $null; $showText = $true; $lastDebugState = ''
try {
    $serial.Open()
    Write-Host 'Port otwarty. Otwarcie USB moze zresetowac Mega, ale nie zmienia flagi EEPROM.'
    if ($CaptureOnly) {
        Start-ModemCapture
        return
    }
    do {
        Write-Host "`n1. Wlacz debug i zapisz flage w EEPROM"
        Write-Host '2. Rozpocznij zapis logu do pliku (Q konczy)'
        Write-Host '3. Wylacz debug i zapisz flage w EEPROM'
        Write-Host '0. Zamknij skrypt bez zmiany flagi'
        $choice = Read-Host 'Wybor'
        try { Invoke-DebugMenuChoice $choice }
        catch { Write-Warning $_ }
    } while ($choice -ne '0')
} finally {
    # No DEBUG OFF here: only explicit menu option 3 may change the flag to OFF.
    try { if ($serial.IsOpen) { $serial.Close() } }
    finally { $serial.Dispose() }
}
