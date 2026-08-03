[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$TaskId,
    [Parameter(Mandatory)]
    [string]$ToState,
    [Parameter(Mandatory)]
    [string]$ActorSessionId,
    [Parameter(Mandatory)]
    [string]$Reason,
    [string]$ReplacementTaskId,
    [string]$Timestamp
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$python = (Get-Command python -ErrorAction Stop).Source
$arguments = @(
    (Join-Path $PSScriptRoot 'orchestration.py'),
    '--repo-root',
    $repoRoot,
    'transition',
    '--task-id',
    $TaskId,
    '--to-state',
    $ToState,
    '--actor-session-id',
    $ActorSessionId,
    '--reason',
    $Reason
)
if ($ReplacementTaskId) {
    $arguments += @('--replacement-task-id', $ReplacementTaskId)
}
if ($Timestamp) {
    $arguments += @('--timestamp', $Timestamp)
}
if (-not $PSCmdlet.ShouldProcess($TaskId, "transition to $ToState")) {
    $arguments += '--dry-run'
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Task transition failed with exit code $LASTEXITCODE."
}
