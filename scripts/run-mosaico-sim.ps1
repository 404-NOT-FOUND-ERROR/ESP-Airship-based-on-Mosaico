param([switch]$Headless)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspace = Join-Path $repoRoot '.mosaico-workspace'
if (-not (Test-Path -LiteralPath (Join-Path $workspace 'mosaico.py'))) {
    & (Join-Path $PSScriptRoot 'bootstrap-mosaico-workspace.ps1')
}

Push-Location $workspace
try {
    $arguments = @('project', 'sim', '--project', 'projects\airboat_baseline')
    if ($Headless) { $arguments += @('--headless', '--duration', '3') }
    else { $arguments += '--interactive' }
    & python mosaico.py @arguments
    if ($LASTEXITCODE -ne 0) { throw "Mosaico simulator failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}
