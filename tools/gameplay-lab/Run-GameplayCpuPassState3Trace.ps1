[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [Parameter(Mandatory = $true)]
    [string]$FceuxPath,
    [switch]$RequirePass
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedRomSha256 = '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'
$ExpectedFceuxSha256 = 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B'
$ExpectedMapSchema = 'TGLPASS3-1'
$ExpectedOutputSchema = 'TGLPASS3-TRACE-1'
$ExpectedMaxFrames = 6200
$ExpectedMaxLiveFrames = 1800
$SessionCapBytes = 64MB
$StartupSeconds = 5
$WatchdogSeconds = 5
$HardTimeoutSeconds = 180

function Get-SessionBytes {
    param([string]$Path)
    $Sum = (Get-ChildItem -LiteralPath $Path -File -Recurse -ErrorAction Stop |
        Measure-Object -Property Length -Sum).Sum
    if ($null -eq $Sum) { return [int64]0 }
    return [int64]$Sum
}

function Get-ProgressSnapshot {
    param([string]$Path)
    $Stream = $null
    $Reader = $null
    try {
        $Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $Reader = [IO.StreamReader]::new($Stream, [Text.Encoding]::UTF8, $true)
        $Buffer = New-Object char[] 4096
        $Builder = [Text.StringBuilder]::new()
        [int]$Count = 0
        while (($Read = $Reader.Read($Buffer, 0, $Buffer.Length)) -gt 0) {
            $Count += $Read
            if ($Count -gt 65536) { return $null }
            [void]$Builder.Append($Buffer, 0, $Read)
        }
        $Text = $Builder.ToString()
    } catch {
        return $null
    } finally {
        if ($null -ne $Reader) { $Reader.Dispose() }
        elseif ($null -ne $Stream) { $Stream.Dispose() }
    }
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $Values = [ordered]@{}
    foreach ($Line in [regex]::Split($Text, '\r?\n')) {
        if ([string]::IsNullOrWhiteSpace($Line)) { continue }
        if ($Line -notmatch '^([a-z0-9_]+)=(.*)$' -or $Values.Contains($Matches[1])) {
            return $null
        }
        $Values[$Matches[1]] = $Matches[2]
    }
    foreach ($Name in @('schema','sequence','emu_frame','stage','speedmode_ok')) {
        if (-not $Values.Contains($Name)) { return $null }
    }
    return [pscustomobject]@{ values = $Values; marker = $Text }
}

function Get-Status {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing trace status: $Path" }
    $Status = [ordered]@{}
    foreach ($Line in Get-Content -LiteralPath $Path) {
        if ($Line -notmatch '^([a-z0-9_]+)=(.*)$' -or $Status.Contains($Matches[1])) {
            throw "Malformed or duplicate pass-state-3 status row: $Line"
        }
        $Status[$Matches[1]] = $Matches[2]
    }
    return $Status
}

function Get-StatusInt {
    param([object]$Status, [string]$Name)
    [int]$Value = 0
    if (-not $Status.Contains($Name) -or
        -not [int]::TryParse([string]$Status[$Name], [ref]$Value)) {
        throw "Pass-state-3 status '$Name' is not an integer."
    }
    return $Value
}

function Get-SanitizedCommand {
    param([string]$Command, [string[]]$Arguments)
    $Text = ((@($Command) + @($Arguments)) -join ' ')
    foreach ($Pair in @(
        @{ value = $ProjectRoot; replacement = '[PROJECT]' },
        @{ value = $RomPath; replacement = '[LOCAL_REV1_ROM]' },
        @{ value = $FceuxPath; replacement = '[LOCAL_FCEUX]' },
        @{ value = $OutputRoot; replacement = '[IGNORED_TRACE_OUTPUT]' }
    )) {
        if ($Pair.value) { $Text = $Text.Replace([string]$Pair.value, $Pair.replacement) }
    }
    return $Text
}

function Add-ProcessLogMetadata {
    param(
        [string]$Path,
        [string]$Label,
        [string]$Command,
        [string]$StartUtc,
        [string]$EndUtc,
        [int]$ExitCode
    )
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        Set-Content -LiteralPath $Path -Value '' -Encoding UTF8
    }
    $Existing = [IO.File]::ReadAllText($Path) -replace '^\uFEFF', ''
    $Rows = [Collections.Generic.List[string]]::new()
    [void]$Rows.Add("[runner] label=$Label start_utc=$StartUtc command=$Command")
    if ([string]::IsNullOrWhiteSpace($Existing)) {
        [void]$Rows.Add("[runner] no stdout/stderr emitted; exit=$ExitCode")
    }
    [void]$Rows.Add("[runner] end_utc=$EndUtc exit=$ExitCode")
    if ($Existing.Length -gt 0 -and $Existing -notmatch '(\r\n|\r|\n)$') {
        [void]$Rows.Insert(0, '')
    }
    $Rows | Add-Content -LiteralPath $Path -Encoding UTF8
}

function Assert-StatusContract {
    param([object]$Status)
    $Required = @(
        'schema','schema_version','map_schema','result','rom_sha256','fceux_sha256',
        'max_frames','max_live_frames','speedmode_ok','setup_seen','tip_started',
        'clock_stopped_seen','clock_running_seen','live_seen','cpu_possession_seen',
        'state3_captured','state3_source_frame','state3_source_event','state3_prior',
        'state3_new','writer_callback_pc','writer_callback_raw_bank','writer_offense_control',
        'a214_after_state3_event','b074_event','b1e7_event','b500_event','b228_event',
        'b24f_event','holder_changed_through_b24f','alternate_count','nonzero_route_seen',
        'pass_route_confirmed','trace_rows','actor_rows','screenshot_count',
        'final_progress_written','ram_writes','cheats','savestates','fm2',
        'final_pads_neutral','stop_reason'
    )
    foreach ($Name in $Required) {
        if (-not $Status.Contains($Name)) { throw "Pass-state-3 trace omitted status '$Name'." }
    }
    if ($Status.schema -ne $ExpectedOutputSchema -or $Status.schema_version -ne '1' -or
        $Status.map_schema -ne $ExpectedMapSchema -or
        $Status.rom_sha256 -ne $ExpectedRomSha256 -or
        $Status.fceux_sha256 -ne $ExpectedFceuxSha256 -or
        $Status.max_frames -ne [string]$ExpectedMaxFrames -or
        $Status.max_live_frames -ne [string]$ExpectedMaxLiveFrames -or
        $Status.result -notin @('pass','abort') -or
        $Status.speedmode_ok -ne 'true' -or $Status.final_progress_written -ne 'true' -or
        $Status.ram_writes -ne '0' -or $Status.cheats -ne '0' -or
        $Status.savestates -ne '0' -or $Status.fm2 -ne '0' -or
        $Status.final_pads_neutral -ne 'true') {
        throw 'Pass-state-3 trace failed its revision, safety, or completion contract.'
    }
}

function Assert-PassEvidence {
    param([object]$Status)
    foreach ($Gate in @('setup_seen','tip_started','clock_stopped_seen','clock_running_seen',
                         'live_seen','cpu_possession_seen','state3_captured',
                         'holder_changed_through_b24f','pass_route_confirmed')) {
        if ($Status[$Gate] -ne 'true') { throw "Pass-state-3 trace did not prove '$Gate'." }
    }
    foreach ($Name in @('state3_source_event','a214_after_state3_event','b074_event',
                         'b1e7_event','b500_event','b228_event','b24f_event')) {
        if ((Get-StatusInt $Status $Name) -lt 0) { throw "Pass-state-3 trace lacks '$Name'." }
    }
    if ($Status.state3_prior -eq 'NA' -or $Status.state3_new -ne '03' -or
        $Status.writer_callback_pc -eq 'NA' -or $Status.writer_callback_raw_bank -eq 'NA' -or
        $Status.writer_offense_control -eq '00' -or
        $Status.alternate_count -ne '0' -or $Status.nonzero_route_seen -ne 'false' -or
        $Status.actor_rows -ne '33' -or $Status.screenshot_count -ne '2') {
        throw 'Pass-state-3 trace did not satisfy the direct CPU B074/B24F pass predicate.'
    }
}

$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$LuaPath = Join-Path $PSScriptRoot 'tecmo_cpu_pass_state3.lua'
$MapPath = Join-Path $PSScriptRoot 'tecmo_cpu_pass_state3_rev1_map.lua'
foreach ($Path in @($LuaPath, $MapPath, $RomPath, $FceuxPath)) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing pass-state-3 trace input: $Path" }
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$FceuxPath = (Resolve-Path -LiteralPath $FceuxPath).Path
if (@(Get-Process -Name fceux -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'FCEUX is already running; pass-state-3 trace refuses concurrent emulator state.'
}
$RomHash = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash.ToUpperInvariant()
$FceuxHash = (Get-FileHash -LiteralPath $FceuxPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($RomHash -ne $ExpectedRomSha256) { throw "Unexpected Rev1 ROM SHA-256: $RomHash" }
if ($FceuxHash -ne $ExpectedFceuxSha256) { throw "Unexpected FCEUX SHA-256: $FceuxHash" }

$OutputBase = Join-Path $ProjectRoot 'temp-videos\gameplay-lab\cpu-pass-state3'
$RelativeOutputBase = 'temp-videos/gameplay-lab/cpu-pass-state3'
& git -C $ProjectRoot check-ignore -q -- $RelativeOutputBase
if ($LASTEXITCODE -ne 0) { throw 'Pass-state-3 trace output base is not ignored.' }
[void](New-Item -ItemType Directory -Force -Path $OutputBase)
$SessionStem = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutputRoot = Join-Path $OutputBase $SessionStem
$Suffix = 1
while (Test-Path -LiteralPath $OutputRoot) {
    $OutputRoot = Join-Path $OutputBase ('{0}-{1:D2}' -f $SessionStem, $Suffix++)
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$OutputPrefix = $OutputBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (!$OutputRoot.StartsWith($OutputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Pass-state-3 session escaped its ignored output base.'
}
[void](New-Item -ItemType Directory -Force -Path $OutputRoot)
$IncompletePath = Join-Path $OutputRoot '.incomplete'
Set-Content -LiteralPath $IncompletePath -Value 'CPU pass-state-3 trace incomplete' -NoNewline -Encoding ASCII

$ScriptHash = (Get-FileHash -LiteralPath $LuaPath -Algorithm SHA256).Hash.ToUpperInvariant()
$MapHash = (Get-FileHash -LiteralPath $MapPath -Algorithm SHA256).Hash.ToUpperInvariant()
$EnvironmentNames = @(
    'TECMO_CPU_PASS_STATE3_OUTPUT','TECMO_CPU_PASS_STATE3_MAP',
    'TECMO_CPU_PASS_STATE3_ROM_SHA256','TECMO_CPU_PASS_STATE3_FCEUX_SHA256'
)
$Previous = @{}
foreach ($Name in $EnvironmentNames) {
    $Previous[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process')
}

try {
    $env:TECMO_CPU_PASS_STATE3_OUTPUT = $OutputRoot
    $env:TECMO_CPU_PASS_STATE3_MAP = $MapPath
    $env:TECMO_CPU_PASS_STATE3_ROM_SHA256 = $RomHash
    $env:TECMO_CPU_PASS_STATE3_FCEUX_SHA256 = $FceuxHash
    $Stdout = Join-Path $OutputRoot 'fceux.stdout.log'
    $Stderr = Join-Path $OutputRoot 'fceux.stderr.log'
    $Arguments = @('-sound', '0', '-lua', $LuaPath, $RomPath)
    $Start = @{
        FilePath = $FceuxPath
        ArgumentList = ('-sound 0 -lua "{0}" "{1}"' -f $LuaPath, $RomPath)
        WorkingDirectory = (Split-Path -Parent $FceuxPath)
        PassThru = $true
        WindowStyle = 'Hidden'
        RedirectStandardOutput = $Stdout
        RedirectStandardError = $Stderr
    }
    $Process = $null
    $ProcessExitCode = -1
    $ProcessStartUtc = (Get-Date).ToUniversalTime().ToString('o')
    try {
        $Process = Start-Process @Start
        $Watch = [Diagnostics.Stopwatch]::StartNew()
        $SentinelSeen = $false
        $LastProgressMarker = ''
        $LastProgressSeconds = 0.0
        $ProgressPath = Join-Path $OutputRoot 'progress.txt'
        while (-not $Process.WaitForExit(250)) {
            if ((Get-SessionBytes $OutputRoot) -gt $SessionCapBytes) {
                $Process.Kill(); $Process.WaitForExit()
                throw 'Pass-state-3 trace exceeded the 64 MiB session-output cap.'
            }
            $Snapshot = Get-ProgressSnapshot $ProgressPath
            if (-not $SentinelSeen) {
                if ($null -ne $Snapshot) {
                    $SentinelSeen = $true
                    $LastProgressMarker = $Snapshot.marker
                    $LastProgressSeconds = $Watch.Elapsed.TotalSeconds
                } elseif ($Watch.Elapsed.TotalSeconds -gt $StartupSeconds) {
                    $Process.Kill(); $Process.WaitForExit()
                    throw 'Pass-state-3 trace missed the five-second startup sentinel.'
                }
            } elseif ($null -ne $Snapshot -and $Snapshot.marker -cne $LastProgressMarker) {
                $LastProgressMarker = $Snapshot.marker
                $LastProgressSeconds = $Watch.Elapsed.TotalSeconds
            } elseif ($Watch.Elapsed.TotalSeconds - $LastProgressSeconds -gt $WatchdogSeconds) {
                $Process.Kill(); $Process.WaitForExit()
                throw 'Pass-state-3 trace stopped publishing bounded progress.'
            }
            if ($Watch.Elapsed.TotalSeconds -gt $HardTimeoutSeconds) {
                $Process.Kill(); $Process.WaitForExit()
                throw 'Pass-state-3 trace exceeded the 180-second hard timeout.'
            }
        }
    } finally {
        if ($null -ne $Process) {
            if (-not $Process.HasExited) { $Process.Kill() }
            $Process.WaitForExit()
            $Process.Refresh()
            $ProcessExitCode = [int]$Process.ExitCode
            $ProcessEndUtc = (Get-Date).ToUniversalTime().ToString('o')
            $Sanitized = Get-SanitizedCommand $FceuxPath $Arguments
            Add-ProcessLogMetadata $Stdout 'fceux-stdout' $Sanitized $ProcessStartUtc $ProcessEndUtc $ProcessExitCode
            Add-ProcessLogMetadata $Stderr 'fceux-stderr' $Sanitized $ProcessStartUtc $ProcessEndUtc $ProcessExitCode
            $Process.Dispose()
        }
    }
    if ($ProcessExitCode -ne 0) { throw "FCEUX exited $ProcessExitCode." }
    if ((Get-SessionBytes $OutputRoot) -gt $SessionCapBytes) {
        throw 'Pass-state-3 trace exceeded the 64 MiB session-output cap after shutdown.'
    }
    foreach ($Name in @('metadata.txt','events.csv','actors.csv','sequence.txt','progress.txt','status.txt',
                         'fceux.stdout.log','fceux.stderr.log')) {
        $Path = Join-Path $OutputRoot $Name
        if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -le 0) {
            throw "Pass-state-3 trace omitted or emptied '$Name'."
        }
    }
    if (Test-Path -LiteralPath (Join-Path $OutputRoot 'progress.txt.tmp') -PathType Leaf) {
        throw 'Pass-state-3 trace left a progress temporary file.'
    }
    $Progress = Get-ProgressSnapshot (Join-Path $OutputRoot 'progress.txt')
    if ($null -eq $Progress -or $Progress.values.schema -ne 'TGLPASS3-PROGRESS-1' -or
        $Progress.values.stage -ne 'finished' -or $Progress.values.speedmode_ok -ne 'true') {
        throw 'Pass-state-3 trace did not publish a valid final progress record.'
    }
    $Status = Get-Status (Join-Path $OutputRoot 'status.txt')
    Assert-StatusContract $Status
    if ($Status.result -eq 'pass') { Assert-PassEvidence $Status }
    $SummaryPath = Join-Path $OutputRoot 'runner-summary.txt'
    @(
        "result=$($Status.result)",
        "rom_sha256=$RomHash",
        "fceux_sha256=$FceuxHash",
        "lua_sha256=$ScriptHash",
        "map_sha256=$MapHash",
        "writer_callback_pc=$($Status.writer_callback_pc)",
        "writer_callback_raw_bank=$($Status.writer_callback_raw_bank)",
        "writer_offense_control=$($Status.writer_offense_control)",
        "state3_source_event=$($Status.state3_source_event)",
        "a214_after_state3_event=$($Status.a214_after_state3_event)",
        "b074_event=$($Status.b074_event)",
        "b1e7_event=$($Status.b1e7_event)",
        "b500_event=$($Status.b500_event)",
        "b228_event=$($Status.b228_event)",
        "b24f_event=$($Status.b24f_event)",
        "holder_changed_through_b24f=$($Status.holder_changed_through_b24f)",
        "pass_route_confirmed=$($Status.pass_route_confirmed)",
        "alternate_count=$($Status.alternate_count)",
        "nonzero_route_seen=$($Status.nonzero_route_seen)",
        "stop_reason=$($Status.stop_reason)"
    ) | Set-Content -LiteralPath $SummaryPath -Encoding UTF8
    if ((Get-SessionBytes $OutputRoot) -gt $SessionCapBytes) {
        throw 'Pass-state-3 trace exceeded the 64 MiB session-output cap after summary.'
    }
    Remove-Item -LiteralPath $IncompletePath -Force
    Write-Output ("CPU PASS STATE3 TRACE {0}: {1}" -f $Status.result.ToUpperInvariant(), $OutputRoot)
    if ($RequirePass -and $Status.result -ne 'pass') {
        throw 'Pass-state-3 trace reached its fixed bound without validated direct pass evidence.'
    }
} finally {
    foreach ($Name in $EnvironmentNames) {
        [Environment]::SetEnvironmentVariable($Name, $Previous[$Name], 'Process')
    }
}
