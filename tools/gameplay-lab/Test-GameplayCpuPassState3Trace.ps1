[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$LuaPath = Join-Path $PSScriptRoot 'tecmo_cpu_pass_state3.lua'
$MapPath = Join-Path $PSScriptRoot 'tecmo_cpu_pass_state3_rev1_map.lua'
$RunnerPath = Join-Path $PSScriptRoot 'Run-GameplayCpuPassState3Trace.ps1'
$ReadmePath = Join-Path $PSScriptRoot 'README.md'
foreach ($Path in @($LuaPath, $MapPath, $RunnerPath, $ReadmePath)) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing CPU pass-state-3 trace file: $Path"
    }
}

$Lua = Get-Content -Raw -LiteralPath $LuaPath
$Map = Get-Content -Raw -LiteralPath $MapPath
$Runner = Get-Content -Raw -LiteralPath $RunnerPath
$Readme = Get-Content -Raw -LiteralPath $ReadmePath
$All = $Lua + "`n" + $Map + "`n" + $Runner
$Failures = [Collections.Generic.List[string]]::new()
function Assert-Trace {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { [void]$script:Failures.Add($Message) }
}

foreach ($Pattern in @(
    'memory\s*\.\s*write(?:byte|word|dword)?\s*\(',
    'memory\s*\.\s*writebyterange\s*\(',
    '\bcheat\s*\.',
    '\bsavestate\s*\.',
    '\bmovie\s*\.\s*[A-Za-z_]',
    '\bFCEU\s*\.\s*load',
    '\bemu\s*\.\s*load'
)) {
    Assert-Trace ($Lua -notmatch $Pattern) "Forbidden Lua capability matched: $Pattern"
}
Assert-Trace ($All -notmatch '(?i)[A-Z]:\\Users\\|[A-Z]:\\Games\\|Tecmo NBA Basketball \(USA\).*\.nes') `
    'A private path or canonical ROM filename was hard-coded.'
Assert-Trace ($Map -match 'schema = "TGLPASS3-1"' -and
    $Map -match 'output_schema = "TGLPASS3-TRACE-1"' -and
    $Map -match 'base_sha = "5a750ed7af05b18058ff1c2bc0c048118758475a"') `
    'CPU pass-state-3 map schema/base lock is missing.'
Assert-Trace ($Map -match '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4' -and
    $Map -match 'F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B') `
    'CPU pass-state-3 ROM/FCEUX fingerprint lock is missing.'
Assert-Trace ($Map -match 'max_frames = 6200' -and $Map -match 'max_live_frames = 1800' -and
    $Map -match 'chain_settle_frames = 240' -and $Map -match 'session_bytes = 64 \* 1024 \* 1024' -and
    $Map -match 'trace_rows = 12000' -and $Map -match 'actor_rows = 44' -and
    $Map -match 'screenshots = 2') 'CPU pass-state-3 fixed cap map is incomplete.'
Assert-Trace ($Map -match 'bank05 = \{ \[0x0A\] = true, \[0x0B\] = true \}' -and
    $Map -match 'bank06 = \{ \[0x0C\] = true, \[0x0D\] = true \}' -and
    $Map -match 'fixed = \{ \[0x0F\] = true \}') 'CPU pass-state-3 raw mapper gates are missing.'
foreach ($Address in @('0xF024','0xF059','0xA214','0xB074','0xB1E7','0xB500','0xB228','0xB24F')) {
    Assert-Trace ($Map.Contains($Address)) "Required exact source hook $Address is missing."
}
foreach ($Field in @('offense_actor','defense_actor','offense_side','defense_side',
                     'control0','control1','side_holder_0e','candidate_037f',
                     'object_slot10_state','saved_object_horizontal_velocity_lo',
                     'saved_object_vertical_velocity_lo','flight_target_x_lo',
                     'flight_target_y_lo','object_slot10_count_lo')) {
    Assert-Trace ($Map -match ("\b{0}\b" -f [regex]::Escape($Field))) "Required raw map field '$Field' is missing."
}
Assert-Trace ($Lua -match 'memory\.registerwrite\(M\.mapper\.select' -and
    $Lua -match 'memory\.registerwrite\(M\.mapper\.data' -and
    $Lua -match 'memory\.registerwrite\(R\.object_slot10_state') 'Mapper or $0478 write hooks are missing.'
Assert-Trace ($Lua -match 'memory\.registerexec\(hook\.address' -and
    $Lua -match 'source_hook_accepted\(hook\)' -and
    $Lua -match 'mapped_raw_bank\(hook\.address\)') 'Mapper-gated exact entry hooks are missing.'
Assert-Trace ($Lua -match 'new == 3 and prior ~= 3' -and
    $Lua -match 'S\.cpu_possession_frames >= 8' -and $Lua -match 'q\.control ~= 0' -and
    $Lua -match 'start_source_transition') 'CPU-controlled first state-3 transition gate is incomplete.'
Assert-Trace ($Lua -match 'slot10_state_dispatch_A214' -and
    $Lua -match 'slot10_state3_consume_B074' -and
    $Lua -match 'slot10_state4_settle_entry_B1E7' -and
    $Lua -match 'slot10_state_countdown_B500' -and
    $Lua -match 'slot10_state4_settle_gate_B228' -and
    $Lua -match 'slot10_receiver_settle_B24F' -and
    $Lua -match 'holder_changed_after_B24F' -and $Lua -match 'direct_chain_complete') `
    'Ordered B074/B24F direct-chain proof is incomplete.'
Assert-Trace ($Lua -match 'alternate_count == 0' -and $Lua -match 'not c\.nonzero_route_seen' -and
    $Lua -match 'contact_anchor_seen' -and $Lua -match 'append_alternate') `
    'Shot/rebound/tip/contact exclusion evidence is incomplete.'
foreach ($Button in @('A','B','up','down','left','right','start','select')) {
    Assert-Trace ($Lua -match ([regex]::Escape($Button) + ' = source\.' + [regex]::Escape($Button))) `
        "Complete controller field '$Button' is missing."
}
Assert-Trace ($Lua -match 'joypad\.set\(1, S\.active_p1\)' -and
    $Lua -match 'joypad\.set\(2, S\.active_p2\)' -and
    $Lua -match 'apply_pads\(\{\}, \{\}\)' -and
    $Lua -match 'S\.final_pads_neutral') 'Complete per-frame pads or final neutralization is missing.'
Assert-Trace ($Runner -match 'Get-Process -Name fceux' -and
    $Runner -match 'WindowStyle = .Hidden.' -and
    $Runner -match 'RedirectStandardOutput' -and $Runner -match 'RedirectStandardError' -and
    $Runner -match 'WaitForExit\(250\)' -and $Runner -match '\$HardTimeoutSeconds = 180' -and
    $Runner -match '\$SessionCapBytes = 64MB' -and $Runner -match '\.incomplete' -and
    $Runner -match 'Get-SessionBytes' -and $Runner -match 'finally' -and
    $Runner -match '\$Process\.Kill\(\)') 'Bounded hidden FCEUX runner safety is incomplete.'
Assert-Trace ($Runner -notmatch '\[string\]\$MaxFrames' -and
    $Runner -notmatch '\[switch\]\$Visible' -and
    $Runner -match 'temp-videos\\gameplay-lab\\cpu-pass-state3' -and
    $Runner -match 'check-ignore -q') 'Runner exposes a widened bound or an unignored output path.'
Assert-Trace ($Runner -match 'Assert-StatusContract' -and $Runner -match 'Assert-PassEvidence' -and
    $Runner -match 'writer_callback_pc' -and $Runner -match 'writer_offense_control' -and
    $Runner -match 'Pass-state-3 trace reached its fixed bound') 'Runner does not distinguish compact abort evidence from a validated pass.'
Assert-Trace ($Readme -match 'CPU pass-state-3' -and $Readme -match 'Run-GameplayCpuPassState3Trace\.ps1' -and
    $Readme -match 'Test-GameplayCpuPassState3Trace\.ps1') 'README does not document the separate CPU pass-state-3 tracer.'

$Tokens = $null
$Errors = $null
[void][Management.Automation.Language.Parser]::ParseFile($RunnerPath, [ref]$Tokens, [ref]$Errors)
Assert-Trace ($Errors.Count -eq 0) ('CPU pass-state-3 runner PowerShell parse errors: ' +
    (($Errors | ForEach-Object Message) -join '; '))

if ($Failures.Count -ne 0) {
    $Failures | ForEach-Object { Write-Error $_ }
    throw "$($Failures.Count) CPU pass-state-3 static test(s) failed."
}
Write-Host 'CPU PASS STATE3 STATIC TEST PASS: exact Rev1/FCEUX locks, bounded read-only input, mapper-gated state3/B074/B24F evidence, and neutral cleanup'
