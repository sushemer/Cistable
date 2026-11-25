$ErrorActionPreference = 'Stop'

$testsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$masterDir = Split-Path -Parent $testsDir
$gpsDir = Join-Path $masterDir 'main/modules/gps'
$driverDir = Join-Path $gpsDir 'driver'
$testSource = Join-Path $testsDir 'gps_imu_fusion_test.c'
$fusionSource = Join-Path $gpsDir 'gps_imu_fusion.c'
$validationSource = Join-Path $driverDir 'gps_validation.c'
$gccCommand = Get-Command gcc -ErrorAction Stop
$binaryPath = Join-Path $env:TEMP "gps_imu_fusion_test_$PID.exe"

try {
    $gccArgs = @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
        '-I', $gpsDir, '-I', $driverDir,
        $testSource, $fusionSource, $validationSource,
        '-lm', '-o', $binaryPath
    )
    & $gccCommand.Source @gccArgs
    if ($LASTEXITCODE -ne 0) { throw "gcc terminó con código $LASTEXITCODE" }

    & $binaryPath
    if ($LASTEXITCODE -ne 0) { throw "Las pruebas terminaron con código $LASTEXITCODE" }
}
finally {
    if (Test-Path -LiteralPath $binaryPath) {
        Remove-Item -LiteralPath $binaryPath -Force
    }
}

