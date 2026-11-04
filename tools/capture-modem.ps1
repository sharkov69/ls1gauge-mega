param(
    [string]$Port,
    [ValidateRange(1,3600)][int]$Seconds = 180,
    [string]$OutputFile
)
$ErrorActionPreference = 'Stop'
if (-not ('System.IO.Ports.SerialPort' -as [type])) { Add-Type -AssemblyName System.IO.Ports }
if (-not $Port) {
    Write-Host 'Dostepne porty:'
    [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    Write-Host 'Uruchom ponownie z -Port COM5 (wybierz port Arduino).'
    return
}
if (-not $OutputFile) {
    $OutputFile = Join-Path (Get-Location) ('modem-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
}
$logPath = [IO.Path]::GetFullPath($OutputFile)
$serial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.DtrEnable = $true
$serial.ReadBufferSize = 65536
$serial.ReadTimeout = 200
$writer = $null
$stream = $null
try {
    # CreateNew prevents accidental replacement of an earlier diagnostic log.
    $stream = [IO.File]::Open($logPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    $writer = [IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
    $writer.WriteLine('# PC start: ' + (Get-Date -Format o) + ' port=' + $Port + ' baud=115200')
    $serial.Open()
    Write-Host "Zapis do: $logPath"
    Write-Host 'Otwarcie portu moze zresetowac Mega. Jesli nie, nacisnij RESET Arduino teraz.'
    Write-Host "Zbieranie przez $Seconds s. Nie otwieraj rownoczesnie Monitora portu szeregowego."
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $serial.ReadExisting()
        if ($chunk.Length) { $writer.Write($chunk); $writer.Flush(); [Console]::Write($chunk) }
        Start-Sleep -Milliseconds 20
    }
    $writer.Write($serial.ReadExisting())
    $writer.WriteLine('# PC end: ' + (Get-Date -Format o))
} finally {
    try {
        try { if ($serial.IsOpen) { $serial.Close() } }
        finally { $serial.Dispose() }
    } finally {
        if ($writer) { $writer.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}
Write-Host "Gotowe: $logPath"
