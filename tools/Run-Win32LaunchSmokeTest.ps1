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

if ($DecompRoot) {
    if (!(Test-Path $DecompRoot -PathType Container)) {
        throw "Decompilation root was not found: $DecompRoot"
    }
    $DecompRoot = (Resolve-Path $DecompRoot).Path
}

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

$ExePath = Join-Path $ProjectRoot "build\tecmo_port_game.exe"
if (!(Test-Path $ExePath)) {
    throw "Executable was not found: $ExePath"
}

function Get-PeSubsystem {
    param([string]$Path)

    $Stream = [System.IO.File]::OpenRead($Path)
    $Reader = New-Object System.IO.BinaryReader($Stream)
    try {
        if ($Reader.ReadUInt16() -ne 0x5A4D) {
            throw "Executable does not have an MZ header: $Path"
        }
        $Stream.Position = 0x3C
        $PeOffset = $Reader.ReadUInt32()
        if ($PeOffset -gt $Stream.Length - 92) {
            throw "Executable has an invalid PE header offset: $Path"
        }
        $Stream.Position = $PeOffset
        if ($Reader.ReadUInt32() -ne 0x00004550) {
            throw "Executable does not have a PE signature: $Path"
        }
        $Stream.Position = $PeOffset + 24 + 68
        return $Reader.ReadUInt16()
    } finally {
        $Reader.Dispose()
        $Stream.Dispose()
    }
}

$ConsoleExePath = Join-Path $ProjectRoot "build\tecmo_port.exe"
if (!(Test-Path $ConsoleExePath)) {
    throw "Console executable was not found: $ConsoleExePath"
}
$ConsoleSubsystem = Get-PeSubsystem -Path $ConsoleExePath
$GameSubsystem = Get-PeSubsystem -Path $ExePath
if ($ConsoleSubsystem -ne 3) {
    throw "tecmo_port.exe must retain the console subsystem for CLI tests (found $ConsoleSubsystem)."
}
if ($GameSubsystem -ne 2) {
    throw "tecmo_port_game.exe must use the Windows GUI subsystem (found $GameSubsystem)."
}

$ShortcutScript = Join-Path $ProjectRoot "tools\Update-DesktopShortcut.ps1"
$IconPath = Join-Path $ProjectRoot "build\tecmo_port.ico"
$ShortcutTestPath = Join-Path $ProjectRoot ("build\tecmo_port_launch_smoke_{0}.lnk" -f [Guid]::NewGuid().ToString("N"))
$ExpectedArguments = "--root `"$ProjectRoot`" --play"
$ShortcutShell = $null
$Shortcut = $null
$Process = $null
$PreviousDecompRoot = $env:TECMO_DECOMP_ROOT
try {
    & $ShortcutScript `
        -ExePath $ExePath `
        -ProjectRoot $ProjectRoot `
        -ShortcutPath $ShortcutTestPath

    $ShortcutShell = New-Object -ComObject WScript.Shell
    $Shortcut = $ShortcutShell.CreateShortcut($ShortcutTestPath)
    if (![string]::Equals([System.IO.Path]::GetFullPath($Shortcut.TargetPath),
                          [System.IO.Path]::GetFullPath($ExePath),
                          [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Shortcut target is not the GUI game executable: $($Shortcut.TargetPath)"
    }
    if ($Shortcut.Arguments -cne $ExpectedArguments) {
        throw "Shortcut arguments were '$($Shortcut.Arguments)'; expected '$ExpectedArguments'."
    }
    if (![string]::Equals([System.IO.Path]::GetFullPath($Shortcut.WorkingDirectory),
                          [System.IO.Path]::GetFullPath($ProjectRoot),
                          [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Shortcut working directory is not the port project root: $($Shortcut.WorkingDirectory)"
    }
    if ($Shortcut.IconLocation -cne "$IconPath,0") {
        throw "Shortcut icon was '$($Shortcut.IconLocation)'; expected '$IconPath,0'."
    }

    if ($DecompRoot) {
        $DeveloperFlowOutput = @(& $ConsoleExePath --root $DecompRoot --flow-test 2>&1)
        if ($LASTEXITCODE -ne 0 -or
            !($DeveloperFlowOutput -match '^FLOW TEST PASS:')) {
            $DeveloperFlowTail = ($DeveloperFlowOutput | Select-Object -Last 20) -join [Environment]::NewLine
            throw "Explicit console --root developer flow failed.`n$DeveloperFlowTail"
        }
        Write-Host "Explicit console --root developer flow test passed."
    }

    $MissingDecompRoot = Join-Path $ProjectRoot "__launch_smoke_missing_decomp_root__"
    if (Test-Path -LiteralPath $MissingDecompRoot) {
        throw "Launch-smoke missing-root sentinel unexpectedly exists: $MissingDecompRoot"
    }
    $env:TECMO_DECOMP_ROOT = $MissingDecompRoot
    $Process = Start-Process -FilePath $Shortcut.TargetPath `
        -ArgumentList $Shortcut.Arguments `
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

    Write-Host "Win32 shortcut launch smoke test passed: GUI/console subsystems, project-root arguments, working directory, icon, roster-independent startup, window lifetime, and clean shutdown verified."
} finally {
    if ($null -eq $PreviousDecompRoot) {
        Remove-Item Env:TECMO_DECOMP_ROOT -ErrorAction SilentlyContinue
    } else {
        $env:TECMO_DECOMP_ROOT = $PreviousDecompRoot
    }
    if ($null -ne $Process -and !$Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
    if ($null -ne $Shortcut) {
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($Shortcut)
    }
    if ($null -ne $ShortcutShell) {
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($ShortcutShell)
    }
    if (Test-Path -LiteralPath $ShortcutTestPath) {
        Remove-Item -LiteralPath $ShortcutTestPath -Force
    }
}
