param([switch]$Render,[string]$Executable='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousRoot = $env:OPENJKDF2_ROOT
$previousPath = $env:PATH
$previousTest = $env:GEOSITH_SELFTEST
$previousStress = $env:GEOSITH_STRESS
Push-Location $projectRoot
try {
    & ./build-tests/Release/editor_tests.exe JK/editor-resources.jkl JK/Episode/sithed_smoke.gob
    if ($LASTEXITCODE) { throw 'Geometry tests failed.' }
    $env:OPENJKDF2_ROOT = Join-Path $projectRoot 'JK'
    $env:PATH = (Join-Path $projectRoot 'build') + ';' + $previousPath
    $env:GEOSITH_SELFTEST = $null
    $env:GEOSITH_STRESS = '1'
    $exe = if($Executable){$Executable}else{Join-Path $projectRoot 'build/Release/openjkdf2-64.exe'}
    $stdout = Join-Path $projectRoot 'build/stress.stdout.log'
    $stderr = Join-Path $projectRoot 'build/stress.stderr.log'
    $arguments = @('-autostart','-sp','-episode','sithed_smoke','-map','sithed.jkl')
    if (-not $Render) { $arguments = @('-headless') + $arguments }
    $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(45000)) {
        Stop-Process -Id $process.Id
        throw "Stress test timed out. See $stdout"
    }
    if ($process.ExitCode -ne 0 -or -not (Select-String -LiteralPath $stdout -Pattern 'GEOSITH_STRESS PASS' -SimpleMatch -Quiet)) {
        throw "Stress test failed (exit $($process.ExitCode)). See $stdout and $stderr"
    }
    if ($Render -and -not (Select-String -LiteralPath $stdout -Pattern ', [1-9][0-9]* rendered frames' -Quiet)) {
        throw "Stress test did not render any frames. See $stdout"
    }
    if ($Render -and $env:GEOSITH_COLORMAP_TEST -and @(Select-String -LiteralPath $stdout -Pattern 'GEOSITH_COLORMAP PASS:' -SimpleMatch).Count -lt 2) {
        throw "Live colormap test did not verify multiple GPU palette updates. See $stdout"
    }
    Write-Host 'PASS: repeated live moves, extrusions, collision and level updates.'
} finally {
    $env:OPENJKDF2_ROOT = $previousRoot
    $env:PATH = $previousPath
    $env:GEOSITH_SELFTEST = $previousTest
    $env:GEOSITH_STRESS = $previousStress
    Pop-Location
}
