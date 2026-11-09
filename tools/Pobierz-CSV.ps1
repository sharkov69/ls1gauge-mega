#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateRange(1,2147483647)][int]$DeviceId = 9,
    [string]$OutputDirectory = (Join-Path ([Environment]::GetFolderPath('UserProfile')) ('Downloads\Arduino-CSV-' + (Get-Date -Format 'yyyyMMdd-HHmmss')))
)
$ErrorActionPreference = 'Stop'
$sshDirectory = Join-Path ([Environment]::GetFolderPath('UserProfile')) '.ssh'
$keyPath = Join-Path $sshDirectory 'p59-log-server'
$hostsPath = Join-Path $sshDirectory 'p59-log-server-known_hosts'
if (!(Test-Path -LiteralPath $keyPath) -or !(Test-Path -LiteralPath $hostsPath)) {
    throw 'Brak skonfigurowanego klucza SSH serwera na tym komputerze.'
}
$destination = [IO.Path]::GetFullPath($OutputDirectory)
[void][IO.Directory]::CreateDirectory($destination)
# Token administratora pozostaje w kontenerze; eksport korzysta z istniejącego API.
$remote = @'
import base64,hashlib,json,sys,urllib.request
from receiver import settings_from_env
settings=settings_from_env()
device=int(sys.argv[1])
def get(path):
    request=urllib.request.Request('http://127.0.0.1:8000'+path,
        headers={'Authorization':'Bearer '+settings.admin_token})
    return urllib.request.urlopen(request,timeout=120)
after_device=0
after_file='0'*32
while True:
    with get('/v1/files?after_device=%d&after_file=%s'%(after_device,after_file)) as response:
        files=json.load(response)
    if not files:
        break
    for item in files:
        if item['device_id']!=device:
            continue
        with get('/v1/files/%d/%s'%(device,item['file_id'])) as response:
            body=response.read()
            assert len(body)==int(response.headers['Content-Length'])
        print(json.dumps({'name':'%d-%s.csv'%(device,item['file_id']),
            'sha256':hashlib.sha256(body).hexdigest(),
            'data':base64.b64encode(body).decode('ascii')}),flush=True)
    after_device=files[-1]['device_id']
    after_file=files[-1]['file_id']
'@
$count = 0
$remote | & ssh -o BatchMode=yes -o StrictHostKeyChecking=yes -o "UserKnownHostsFile=$hostsPath" -i $keyPath p59admin@192.168.30.42 "sudo -n docker compose --project-directory /opt/p59-log-server -f /opt/p59-log-server/compose.yaml exec -T api python - $DeviceId" | ForEach-Object {
    $item = $_ | ConvertFrom-Json
    if ($item.name -notmatch '^\d+-[0-9a-f]{32}\.csv$') { throw 'Nieprawidlowy identyfikator pliku.' }
    $bytes = [Convert]::FromBase64String($item.data)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $digest = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) }
    finally { $sha.Dispose() }
    if ($digest -cne $item.sha256) { throw 'Blad sumy kontrolnej pobranego pliku.' }
    $path = Join-Path $destination $item.name
    $stream = [IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
    $count++
    Write-Host ('Zapisano {0} ({1} B)' -f $item.name,$bytes.Length)
}
if ($LASTEXITCODE -ne 0) { throw 'Pobieranie przerwane; zapisane pliki pozostaja w katalogu wynikowym.' }
if (!$count) { throw "Brak plikow dla urzadzenia $DeviceId." }
Write-Host "Gotowe: $destination"
Write-Host 'CSV zawiera dokladnie odebrane bajty. Przy trwajacej wysylce plik moze konczyc sie niepelnym wierszem; pobierz go ponownie pozniej.'
