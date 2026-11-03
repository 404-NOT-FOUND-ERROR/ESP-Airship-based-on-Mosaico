$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$workspaceRoot = (Resolve-Path (Join-Path $projectRoot '..\..')).Path
$zig = if ($env:AIRBOAT_ZIG) { $env:AIRBOAT_ZIG } else { Join-Path $workspaceRoot '.tools\host-compiler\ziglang\zig.exe' }
if (-not (Test-Path -LiteralPath $zig)) { throw "Zig compiler not found: $zig" }
$output = Join-Path $workspaceRoot '.tools\airboat-host-test'
$cache = Join-Path $workspaceRoot '.tools\airboat-zig-cache'
New-Item -ItemType Directory -Force -Path $output, $cache | Out-Null
$savedLocal = $env:ZIG_LOCAL_CACHE_DIR
$savedGlobal = $env:ZIG_GLOBAL_CACHE_DIR
try {
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $cache 'local'
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $cache 'global'
    $component = Join-Path $projectRoot 'components\app_motor_output_gate'
    $safety = Join-Path $projectRoot 'components\app_motor_safety'
    $exe = Join-Path $output 'app_motor_output_gate_test.exe'
    & $zig cc -std=c11 -O2 -UNDEBUG -Wall -Wextra -Werror `
        "-I$component\include" "-I$safety\include" `
        (Join-Path $component 'app_motor_output_gate.c') `
        (Join-Path $safety 'app_motor_safety.c') `
        (Join-Path $component 'host\test_app_motor_output_gate.c') -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Output gate host compilation failed: $LASTEXITCODE" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Output gate host tests failed: $LASTEXITCODE" }
}
finally {
    $env:ZIG_LOCAL_CACHE_DIR = $savedLocal
    $env:ZIG_GLOBAL_CACHE_DIR = $savedGlobal
}
