$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $repoRoot 'firmware\airboat_baseline'

function Find-Zig {
    if ($env:AIRBOAT_ZIG -and (Test-Path -LiteralPath $env:AIRBOAT_ZIG)) { return $env:AIRBOAT_ZIG }
    $candidates = @(
        (Join-Path $repoRoot '.tools\host-compiler\ziglang\zig.exe'),
        (Join-Path $repoRoot '.mosaico-workspace\.tools\host-compiler\ziglang\zig.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

$zig = Find-Zig
if (-not $zig) {
    throw 'Zig is required for the portable C checks. Install the Mosaico host toolchain or put zig on PATH, then run this command again.'
}
$env:AIRBOAT_ZIG = $zig

$testScripts = @(
    'components\app_motor_safety\host\test-host.ps1',
    'components\app_motor_control\host\test-host.ps1',
    'components\app_motor_output_gate\host\test-host.ps1',
    'components\app_imu\host\test-host.ps1'
)
foreach ($relative in $testScripts) {
    & pwsh -NoProfile -File (Join-Path $project $relative)
    if ($LASTEXITCODE -ne 0) { throw "Host test failed: $relative" }
}

& python (Join-Path $PSScriptRoot 'check-arrival.py') --self-test
if ($LASTEXITCODE -ne 0) { throw 'Arrival parameter self-test failed' }
Write-Output 'AirBoat public host checks passed. No device operation was performed.'
