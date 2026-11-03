#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Workspace = (Join-Path $PSScriptRoot '..\.mosaico-workspace'),
    [switch]$ForceSync
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$workspacePath = [System.IO.Path]::GetFullPath($Workspace).TrimEnd('\', '/')
$revision = '1872fd88c01d7807556d01fd2a19864e40a307c9'
$remote = 'https://github.com/esp-mosaico/esp-mosaico-vibe.git'

function Normalize-Remote([string]$Value) {
    return (($Value.TrimEnd('/')) -replace '\.git$', '')
}

function Invoke-Git([string[]]$Arguments) {
    & git @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git failed with exit code $LASTEXITCODE" }
}

function Get-SourceHashes([string]$Root) {
    $hashes = @{}
    if (-not (Test-Path -LiteralPath $Root)) { return $hashes }
    Get-ChildItem -LiteralPath $Root -File -Recurse -Force | ForEach-Object {
        $relative = [System.IO.Path]::GetRelativePath($Root, $_.FullName)
        if ($relative -notmatch '(^|[\\/])(build[^\\/]*|managed_components|\.git|\.gateway|\.codex-runs|\.tools|\.venv|__pycache__)([\\/]|$)' -and
            $relative -notmatch '(^|[\\/])(sdkconfig|sdkconfig\.old|compile_commands\.json)$') {
            $hashes[$relative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
    return $hashes
}

if (-not (Test-Path -LiteralPath (Join-Path $workspacePath '.git'))) {
    if ((Test-Path -LiteralPath $workspacePath) -and @(Get-ChildItem -LiteralPath $workspacePath -Force).Count) {
        throw "Choose an empty directory for the Mosaico workspace: $workspacePath"
    }
    Invoke-Git -Arguments @('clone', '--filter=blob:none', $remote, $workspacePath)
}
$origin = & git -C $workspacePath remote get-url origin
if ($LASTEXITCODE -ne 0 -or (Normalize-Remote $origin) -ne (Normalize-Remote $remote)) {
    throw 'The selected workspace must use the official esp-mosaico-vibe origin.'
}
$changes = & git -C $workspacePath status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit or stash tracked changes in the Mosaico workspace before bootstrap.' }
$head = & git -C $workspacePath rev-parse HEAD
if ($head -ne $revision) {
    Invoke-Git -Arguments @('-C', $workspacePath, 'fetch', '--depth', '1', 'origin', $revision)
    Invoke-Git -Arguments @('-C', $workspacePath, 'checkout', '--detach', $revision)
}
Invoke-Git -Arguments @('-C', $workspacePath, 'submodule', 'update', '--init', '--recursive', '--',
    'submodule/esp-mosaico-bsp', 'submodule/esp-mosaico-utils')

$source = Join-Path $repoRoot 'firmware\airboat_baseline'
$destination = [System.IO.Path]::GetFullPath((Join-Path $workspacePath 'projects\airboat_baseline'))
if (-not $destination.StartsWith($workspacePath + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Application path must stay inside the selected workspace.'
}
if ((Test-Path -LiteralPath $destination) -and ((Get-Item -LiteralPath $destination).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'Choose an application directory without a junction or symbolic link.'
}
$sourceHashes = Get-SourceHashes $source
$existingHashes = Get-SourceHashes $destination
$same = $sourceHashes.Count -eq $existingHashes.Count
foreach ($key in $sourceHashes.Keys) { if ($sourceHashes[$key] -ne $existingHashes[$key]) { $same = $false; break } }
if ($same) {
    Write-Output "Application is already synchronized: $destination"
} else {
    if ((Test-Path -LiteralPath $destination) -and -not $ForceSync) {
        throw 'Application differs from the public source. Review it, then use -ForceSync to archive that copy and synchronize.'
    }
    $staging = Join-Path $workspacePath ('.airboat-sync-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging | Out-Null
    foreach ($key in $sourceHashes.Keys) {
        $target = Join-Path $staging $key
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $source $key) -Destination $target
    }
    if (Test-Path -LiteralPath $destination) {
        $backup = [System.IO.Path]::GetFullPath((Join-Path $workspacePath ('.airboat-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))))
        if (-not $backup.StartsWith($workspacePath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Backup must stay inside the selected workspace.' }
        New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
        Move-Item -LiteralPath $destination -Destination $backup
        Write-Output "Previous application saved: $backup"
    }
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Move-Item -LiteralPath $staging -Destination $destination
    Write-Output "Application synchronized: $destination"
}
Write-Output "Mosaico revision: $revision"
Write-Output 'Bootstrap complete.'
