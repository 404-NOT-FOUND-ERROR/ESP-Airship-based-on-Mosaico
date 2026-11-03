$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$workspaceRoot = (Resolve-Path (Join-Path $projectRoot '..\..')).Path
$zig = if ($env:AIRBOAT_ZIG) { $env:AIRBOAT_ZIG } else { Join-Path $workspaceRoot '.tools\host-compiler\ziglang\zig.exe' }
$component = Join-Path $projectRoot 'components\app_imu'
$output = Join-Path $workspaceRoot '.tools\airboat-host-test'
$cache = Join-Path $workspaceRoot '.tools\airboat-zig-cache'
New-Item -ItemType Directory -Force -Path $output, $cache | Out-Null
$savedLocal = $env:ZIG_LOCAL_CACHE_DIR
$savedGlobal = $env:ZIG_GLOBAL_CACHE_DIR
try {
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $cache 'local'
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $cache 'global'
    $exe = Join-Path $output 'app_imu_test.exe'
    & $zig cc -std=c11 -O2 -UNDEBUG -Wall -Wextra -Werror "-I$component\include" `
        "-I$component\private" (Join-Path $component 'app_imu_model.c') `
        (Join-Path $PSScriptRoot 'test_app_imu.c') -o $exe
    if ($LASTEXITCODE -ne 0) { throw "IMU host compilation failed: $LASTEXITCODE" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "IMU host tests failed: $LASTEXITCODE" }
}
finally {
    $env:ZIG_LOCAL_CACHE_DIR = $savedLocal
    $env:ZIG_GLOBAL_CACHE_DIR = $savedGlobal
}
