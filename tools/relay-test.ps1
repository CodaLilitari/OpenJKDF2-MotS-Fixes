param([int]$RelayPort=27939,[int]$ProxyPort=27940,[double]$Loss=0.03,[string]$Python='python',[switch]$DirectLinks,[int]$TimeoutSeconds=120)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$previousRelay=$env:GEOSITH_RELAY
$previousOnly=$env:GEOSITH_RELAY_ONLY
$previousLog=$env:OPENJKDF2_DPLAY_LOG
$relayProcess=$null
$proxyProcess=$null
$logs=Join-Path $root ('build/relay-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $logs | Out-Null
Push-Location $root
try {
    $relayProcess=Start-Process (Join-Path $root 'build-tests/Release/jkrelay_server.exe') -ArgumentList '-p',"$RelayPort",'-v' -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logs 'server.log') -RedirectStandardError (Join-Path $logs 'server-error.log')
    $null=$relayProcess.Handle
    $env:GEOSITH_RELAY="127.0.0.1:$RelayPort"
    $env:GEOSITH_RELAY_ONLY=if($DirectLinks){$null}else{'1'}
    $env:OPENJKDF2_DPLAY_LOG='1'
    if(!$DirectLinks){
        $proxyProcess=Start-Process $Python -ArgumentList 'tests/relay_proxy.py','--port',"$ProxyPort",'--target-port',"$RelayPort",'--loss',"$Loss" -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logs 'proxy.log') -RedirectStandardError (Join-Path $logs 'proxy-error.log')
        $null=$proxyProcess.Handle
        $env:GEOSITH_RELAY="127.0.0.1:$ProxyPort"
    }
    Start-Sleep -Milliseconds 500
    if($relayProcess.HasExited -or ($proxyProcess -and $proxyProcess.HasExited)){throw "Relay/proxy startup failed. Logs: $logs"}
    & "$PSScriptRoot/network-test.ps1" -SkipNative -TimeoutSeconds $TimeoutSeconds
    if($proxyProcess -and $Loss -gt 0 -and !(Select-String -LiteralPath (Join-Path $logs 'proxy.log') -Pattern 'dropped=[1-9][0-9]*' -Quiet)){throw 'The proxy did not verify dropped packets.'}
    Write-Host "PASS: relay integration. Relay/proxy logs: $logs"
} finally {
    foreach($p in @($proxyProcess,$relayProcess)){if($p -and !$p.HasExited){Stop-Process -Id $p.Id}}
    $env:GEOSITH_RELAY=$previousRelay
    $env:GEOSITH_RELAY_ONLY=$previousOnly
    $env:OPENJKDF2_DPLAY_LOG=$previousLog
    Pop-Location
}
