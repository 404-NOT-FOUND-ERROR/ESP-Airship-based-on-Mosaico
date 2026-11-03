$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$workspaceRoot = (Resolve-Path (Join-Path $projectRoot '..\..')).Path
$zig = if ($env:AIRBOAT_ZIG) { $env:AIRBOAT_ZIG } else { Join-Path $workspaceRoot '.tools\host-compiler\ziglang\zig.exe' }
if (-not (Test-Path -LiteralPath $zig)) {
    throw "Zig compiler not found: $zig"
}

$outputDir = Join-Path $workspaceRoot '.tools\airboat-host-test'
$cacheDir = Join-Path $workspaceRoot '.tools\airboat-zig-cache'
New-Item -ItemType Directory -Force -Path $outputDir, $cacheDir | Out-Null
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $cacheDir 'local'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $cacheDir 'global'
$exe = Join-Path $outputDir 'app_motor_safety_test.exe'
$console = Join-Path $outputDir 'airboat_motor_console.exe'
$source = Join-Path $projectRoot 'components\app_motor_safety\app_motor_safety.c'
$tests = Join-Path $projectRoot 'components\app_motor_safety\host\test_app_motor_safety.c'
$consoleSource = Join-Path $projectRoot 'components\app_motor_safety\host\motor_console.c'
$include = Join-Path $projectRoot 'components\app_motor_safety\include'

& $zig cc -std=c11 -O2 -Wall -Wextra -Werror "-I$include" $source $tests -o $exe
if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed with exit code $LASTEXITCODE"
}
& $exe
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed with exit code $LASTEXITCODE"
}

& $zig cc -std=c11 -O2 -Wall -Wextra -Werror "-I$include" $source $consoleSource -o $console
if ($LASTEXITCODE -ne 0) {
    throw "Console build failed with exit code $LASTEXITCODE"
}
$uiRoot = Join-Path $projectRoot 'components\app_airboat_ui'
$clockTest = Join-Path $uiRoot 'host\test_app_airboat_clock.c'
$clockSource = Join-Path $uiRoot 'host\platform_clock_pc.c'
$uiPrivate = Join-Path $uiRoot 'private'
$clockTestExe = Join-Path $outputDir 'app_airboat_clock_test.exe'
& $zig cc -std=c11 -O2 -Wall -Wextra -Werror "-I$uiPrivate" "-I$include" $source $clockSource $clockTest -o $clockTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Clock integration test compilation failed with exit code $LASTEXITCODE"
}
& $clockTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Clock integration test failed with exit code $LASTEXITCODE"
}
Write-Output "Dry-run console: $console"
