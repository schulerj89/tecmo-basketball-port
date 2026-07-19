param(
    [string]$ProjectRoot,
    [string]$DecompRoot,
    [switch]$Build,
    [int]$StartupTimeoutSeconds = 10,
    [int]$AliveMilliseconds = 1000
)

$ErrorActionPreference = "Stop"

if ($StartupTimeoutSeconds -le 0) {
    throw "StartupTimeoutSeconds must be greater than zero."
}
if ($AliveMilliseconds -lt 0) {
    throw "AliveMilliseconds must be zero or greater."
}

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

if (!$DecompRoot) {
    $ProjectsRoot = Split-Path -Parent $ProjectRoot
    $DecompRoot = Join-Path $ProjectsRoot "disassem\tecmo-basketball-decompilation"
}
if (!(Test-Path $DecompRoot)) {
    throw "Decompilation root was not found: $DecompRoot"
}
$DecompRoot = (Resolve-Path $DecompRoot).Path

if ($Build) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    try {
        $env:TECMO_SKIP_SHORTCUT = "1"
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    } finally {
        if ($null -eq $PreviousSkipShortcut) {
            Remove-Item Env:TECMO_SKIP_SHORTCUT -ErrorAction SilentlyContinue
        } else {
            $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
        }
    }
}

$ExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"
if (!(Test-Path $ExePath)) {
    throw "Executable was not found: $ExePath"
}

$Process = $null
try {
    $Process = Start-Process -FilePath $ExePath `
        -ArgumentList @("--root", "`"$DecompRoot`"", "--play") `
        -WorkingDirectory $ProjectRoot `
        -PassThru

    $Deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    $WindowCreated = $false
    while ([DateTime]::UtcNow -lt $Deadline -and !$Process.HasExited) {
        $Process.Refresh()
        if ($Process.MainWindowHandle -ne 0 -and
            $Process.MainWindowTitle -eq "Tecmo Basketball Native Port Prototype") {
            $WindowCreated = $true
            break
        }
        Start-Sleep -Milliseconds 50
    }

    if (!$WindowCreated) {
        if ($Process.HasExited) {
            throw "Native window exited during startup with code $($Process.ExitCode)."
        }
        throw "Native game window was not created within $StartupTimeoutSeconds seconds."
    }

    Start-Sleep -Milliseconds $AliveMilliseconds
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "Native window exited during the $AliveMilliseconds ms stability interval with code $($Process.ExitCode)."
    }

    if (!$Process.CloseMainWindow()) {
        throw "Could not request a clean close for the native game window."
    }
    if (!$Process.WaitForExit(5000)) {
        throw "Native game window did not close within five seconds."
    }
    if ($Process.ExitCode -ne 0) {
        throw "Native game window returned exit code $($Process.ExitCode) after closing."
    }

    Write-Host "Win32 shortcut launch smoke test passed: window created, remained alive for $AliveMilliseconds ms, and closed cleanly."
} finally {
    if ($null -ne $Process -and !$Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}
