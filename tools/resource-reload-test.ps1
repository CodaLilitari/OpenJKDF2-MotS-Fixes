param([Parameter(Mandatory=$true)][string]$Gob)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$testRoot=Join-Path $root ('build/resource-reload-'+[guid]::NewGuid().ToString('N'))
$saved=@{}
foreach($name in @('PATH','OPENJKDF2_ROOT','GEOSITH_MODS_DIR','GEOSITH_STRESS','GEOSITH_RESOURCE_RELOAD_TEST')){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$process=$null
try {
    New-Item -ItemType Directory -Path (Join-Path $testRoot 'Episode') | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $testRoot 'Resource') -Target (Join-Path $root 'JK/Resource') | Out-Null
    Copy-Item -LiteralPath $Gob -Destination (Join-Path $testRoot 'Episode/sithed_smoke.gob')
    foreach($file in @('JK_.CD','editor-resources.jkl','editor-materials.tsv','registry.json')){Copy-Item -LiteralPath (Join-Path $root "JK/$file") -Destination $testRoot}
    $env:PATH=(Join-Path $root 'build')+';'+$env:PATH
    $env:OPENJKDF2_ROOT=$testRoot
    $env:GEOSITH_MODS_DIR=Join-Path $testRoot 'absent-mods'
    $env:GEOSITH_STRESS='1'
    $env:GEOSITH_RESOURCE_RELOAD_TEST='1'
    $log=Join-Path $testRoot 'stdout.log'
    $process=Start-Process (Join-Path $root 'build/Release/openjkdf2-64.exe') -ArgumentList '-headless','-autostart','-sp','-episode','sithed_smoke','-map','sithed.jkl' -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError (Join-Path $testRoot 'stderr.log')
    $null=$process.Handle
    if(!$process.WaitForExit(45000)){throw "Resource reload timed out. Logs: $testRoot"}
    if($process.ExitCode -ne 0 -or !(Select-String -LiteralPath $log -Pattern 'GEOSITH_RESOURCE_RELOAD PASS' -Quiet)){throw "Resource reload failed. Logs: $testRoot"}
    Write-Host "PASS: saved GOB resources reload without mods. Logs: $testRoot"
} finally {
    if($process -and !$process.HasExited){Stop-Process -Id $process.Id}
    foreach($name in $saved.Keys){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
}
