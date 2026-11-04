param([string]$Executable='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousRoot = $env:OPENJKDF2_ROOT
$previousPath = $env:PATH
$previousTest = $env:GEOSITH_SELFTEST
$previousVerify = $env:GEOSITH_VERIFY_RELOAD
$previousStairs = $env:GEOSITH_VERIFY_STAIRS
$previousLights = $env:GEOSITH_VERIFY_LIGHTS
$previousEpisode = $env:GEOSITH_VERIFY_EPISODE
$previousCogs = $env:GEOSITH_VERIFY_COGS
$previousFrames = $env:GEOSITH_VERIFY_FRAMES
$cogLoose = Join-Path $projectRoot 'JK/cog/01_cargoelev1.cog'
$cogBackup = Join-Path $projectRoot 'build/cog-reload-source.bak'
$restoreCogSource = $false
Push-Location $projectRoot
try {
    & ./build-tests/Release/editor_tests.exe JK/editor-resources.jkl JK/Episode/sithed_smoke.gob
    if ($LASTEXITCODE) { throw 'Geometry tests failed.' }
    $env:OPENJKDF2_ROOT = Join-Path $projectRoot 'JK'
    $env:PATH = (Join-Path $projectRoot 'build') + ';' + $previousPath
    $env:GEOSITH_SELFTEST = '1'
    $env:GEOSITH_VERIFY_STAIRS = $null
    $env:GEOSITH_VERIFY_LIGHTS = $null
    $env:GEOSITH_VERIFY_EPISODE = $null
    $env:GEOSITH_VERIFY_COGS = $null
    $env:GEOSITH_VERIFY_FRAMES = $null
    $env:GEOSITH_VERIFY_RELOAD = $null
    $exe = if($Executable){$Executable}else{Join-Path $projectRoot 'build/Release/openjkdf2-64.exe'}
    $stdout = Join-Path $projectRoot 'build/integration.stdout.log'
    $stderr = Join-Path $projectRoot 'build/integration.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_smoke','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) {
        Stop-Process -Id $process.Id
        throw "Integration test timed out. See $stdout"
    }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_SELFTEST PASS' -SimpleMatch -Quiet)) {
        throw "Integration test failed. See $stdout"
    }
    Remove-Item Env:GEOSITH_SELFTEST
    $env:GEOSITH_VERIFY_RELOAD = '1'
    $stdout = Join-Path $projectRoot 'build/reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_runtime_test','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'Exported level reload timed out.' }
    $output = Get-Content $stdout -Raw
    if ($process.ExitCode -ne 0 -or -not $output.Contains('GEOSITH_RELOAD PASS') -or $output.Contains('Parse problem') -or $output.Contains('ERROR:')) {
        throw "Exported level reload failed. See $stdout"
    }
    $env:GEOSITH_VERIFY_RELOAD = $null
    $env:GEOSITH_VERIFY_STAIRS = '1'
    $stdout = Join-Path $projectRoot 'build/stairs-reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/stairs-reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_stairs_test','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'Stairs reload timed out.' }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_STAIRS_RELOAD PASS' -SimpleMatch -Quiet)) { throw "Stairs reload failed. See $stdout" }
    $env:GEOSITH_VERIFY_STAIRS = $null
    $env:GEOSITH_VERIFY_LIGHTS = '1'
    $stdout = Join-Path $projectRoot 'build/lights-reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/lights-reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_lights_test','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'Lights reload timed out.' }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_LIGHTS_RELOAD PASS' -SimpleMatch -Quiet)) { throw "Lights reload failed. See $stdout" }
    $env:GEOSITH_VERIFY_LIGHTS = $null
    $env:GEOSITH_VERIFY_EPISODE = '1'
    $stdout = Join-Path $projectRoot 'build/episode-reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/episode-reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_episode_test','-map','episode_test.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'Episode reload timed out.' }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_EPISODE_RELOAD PASS' -SimpleMatch -Quiet)) { throw "Episode reload failed. See $stdout" }
    $env:GEOSITH_VERIFY_EPISODE = $null
    $env:GEOSITH_VERIFY_COGS = '1'
    if (Test-Path -LiteralPath $cogBackup) { throw 'COG reload backup already exists.' }
    if (Test-Path -LiteralPath $cogLoose) { Move-Item -LiteralPath $cogLoose -Destination $cogBackup; $restoreCogSource = $true }
    $stdout = Join-Path $projectRoot 'build/cogs-reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/cogs-reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_cogs_test','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'COG reload timed out.' }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_COGS_RELOAD PASS' -SimpleMatch -Quiet)) { throw "COG reload failed. See $stdout" }
    $env:GEOSITH_VERIFY_COGS = $null
    $env:GEOSITH_VERIFY_FRAMES = '1'
    $stdout = Join-Path $projectRoot 'build/frames-reload.stdout.log'
    $stderr = Join-Path $projectRoot 'build/frames-reload.stderr.log'
    $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode','sithed_frames_test','-map','sithed.jkl' -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id; throw 'Frames reload timed out.' }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_FRAMES_RELOAD PASS' -SimpleMatch -Quiet)) { throw "Frames reload failed. See $stdout" }
    Write-Host 'PASS: geometry, editor tools, saved GOB, stairs, lights, episode, COG and frame reload.'

} finally {
    $env:OPENJKDF2_ROOT = $previousRoot
    $env:PATH = $previousPath
    $env:GEOSITH_SELFTEST = $previousTest
    $env:GEOSITH_VERIFY_RELOAD = $previousVerify
    $env:GEOSITH_VERIFY_STAIRS = $previousStairs
    $env:GEOSITH_VERIFY_LIGHTS = $previousLights
    $env:GEOSITH_VERIFY_EPISODE = $previousEpisode
    $env:GEOSITH_VERIFY_COGS = $previousCogs
    $env:GEOSITH_VERIFY_FRAMES = $previousFrames
    if ($restoreCogSource -and (Test-Path -LiteralPath $cogBackup)) { Move-Item -LiteralPath $cogBackup -Destination $cogLoose }
    Pop-Location
}
