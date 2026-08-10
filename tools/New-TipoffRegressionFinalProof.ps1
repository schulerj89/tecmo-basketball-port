param(
    [string]$ProjectRoot,
    [string]$AssetPackPath,
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'

if (!$ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$AssetPackPath) { $AssetPackPath = Join-Path $ProjectRoot 'build\tecmo.assetpack' }
$AssetPackPath = (Resolve-Path -LiteralPath $AssetPackPath).Path
if (!$OutputRoot) {
    $OutputRoot = Join-Path $ProjectRoot 'build\proof\tipoff-regression-final'
}
$Exe = Join-Path $ProjectRoot 'build\tecmo_port.exe'
if (!(Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "Missing executable: $Exe"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$RunA = Join-Path $OutputRoot 'run-a'
$RunB = Join-Path $OutputRoot 'run-b'
New-Item -ItemType Directory -Force -Path $RunA,$RunB | Out-Null
foreach ($TraceDirectory in @($RunA, $RunB)) {
    # The native tracer includes the exact source frame in each file name.
    # Clear only its dedicated run directory so a changed valid timing cannot
    # leave a stale, similarly labelled PNG in a new contact sheet.
    Get-ChildItem -LiteralPath $TraceDirectory -Force | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Force -Recurse
    }
}
$env:TECMO_ASSETPACK = $AssetPackPath

function Invoke-Trace([string]$Directory) {
    $text = (& $Exe --root $ProjectRoot --tipoff-regression-trace $AssetPackPath $Directory 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or $text -notmatch 'TIP-OFF CONTINUOUS REGRESSION TRACE PASS') {
        throw "Continuous native tip-off trace failed in $Directory.`n$text"
    }
}

function Get-ScenarioRows([string]$Directory, [string]$Scenario) {
    $path = Join-Path $Directory "$Scenario-trace.csv"
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing trace CSV: $path"
    }
    $rows = @(Import-Csv -LiteralPath $path)
    if ($rows.Count -eq 0) { throw "Trace CSV is empty: $path" }
    return $rows
}

function Get-One($Rows, [scriptblock]$Predicate, [string]$Label) {
    $found = @($Rows | Where-Object $Predicate)
    if ($found.Count -ne 1) { throw "Expected exactly one $Label row; got $($found.Count)." }
    return $found[0]
}

function Get-First($Rows, [scriptblock]$Predicate, [string]$Label) {
    $found = @($Rows | Where-Object $Predicate | Select-Object -First 1)
    if ($found.Count -ne 1) { throw "Expected at least one $Label row." }
    return $found[0]
}

function Assert-Equal($Actual, $Expected, [string]$Label) {
    if ([string]$Actual -ne [string]$Expected) {
        throw "$Label expected '$Expected', got '$Actual'."
    }
}

function Assert-True([bool]$Condition, [string]$Label) {
    if (!$Condition) { throw $Label }
}

function Invoke-Bank04SourceMix([int]$Value, [int]$Mixer) {
    $mixed = ($Value -bxor $Mixer) -band 0xFF
    $carry = ($mixed -band 0x80) -ne 0
    $mixed = ($mixed -shl 1) -band 0xFF
    if ($carry) { $mixed = ($mixed -bxor 0x1D) -band 0xFF }
    if ($mixed -eq 0) { $mixed = ($mixed -bxor $Mixer) -band 0xFF }
    return $mixed
}

function Get-Bank04SourceAfter([int]$Initial, [int]$Mixer, [int]$Count) {
    $value = $Initial
    for ($i = 0; $i -lt $Count; ++$i) {
        $value = Invoke-Bank04SourceMix $value $Mixer
    }
    return $value
}

function Get-Bank04Error([int]$Captured) {
    if ($Captured -eq 0) { return 12 }
    $error = [Math]::Abs(0xF9 - $Captured)
    return [Math]::Min($error, 0x0B)
}

function Get-JumpTuple($Row) {
    return @(
        $Row.away_state, $Row.away_phase, $Row.away_velocity_q8,
        $Row.away_altitude_q8, $Row.away_x, $Row.away_y,
        $Row.away_anchor_x, $Row.away_anchor_y, $Row.away_pose,
        $Row.home_state, $Row.home_phase, $Row.home_velocity_q8,
        $Row.home_altitude_q8, $Row.home_x, $Row.home_y,
        $Row.home_anchor_x, $Row.home_anchor_y, $Row.home_pose,
        $Row.jump_active
    ) -join '|'
}

function Assert-RunDeterministic([string]$Left, [string]$Right) {
    $leftFiles = @(Get-ChildItem -LiteralPath $Left -File |
        Where-Object { $_.Extension -in '.png','.csv' } |
        Sort-Object Name)
    $rightFiles = @(Get-ChildItem -LiteralPath $Right -File |
        Where-Object { $_.Extension -in '.png','.csv' } |
        Sort-Object Name)
    if ($leftFiles.Count -ne $rightFiles.Count) {
        throw 'Repeated continuous trace produced a different artifact count.'
    }
    for ($i = 0; $i -lt $leftFiles.Count; ++$i) {
        Assert-Equal $rightFiles[$i].Name $leftFiles[$i].Name 'Repeated trace artifact name'
        $leftHash = (Get-FileHash -LiteralPath $leftFiles[$i].FullName -Algorithm SHA256).Hash
        $rightHash = (Get-FileHash -LiteralPath $rightFiles[$i].FullName -Algorithm SHA256).Hash
        Assert-Equal $rightHash $leftHash "Nondeterministic artifact $($leftFiles[$i].Name)"
    }
}

function Invoke-RimRattle([string]$Directory) {
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $records = @()
    foreach ($frame in @(72,73,74,77,88,103)) {
        $png = Join-Path $Directory ("rim-rattle-frame{0:D3}.png" -f $frame)
        $text = (& $Exe --root $ProjectRoot --render-test-mode "gameplay-jump-rattle-frame$frame" $png 2>&1 | Out-String)
        $expectedShot = if ($frame -eq 103) { 'none' } else { 'jump' }
        if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $png -PathType Leaf) -or
            $text -match 'GAMEPLAY RENDER REJECTED' -or
            $text -notmatch "gameplay-state frame=$frame shot=$expectedShot phase=live score=0/2") {
            throw "Rim-rattle render failed at frame $frame.`n$text"
        }
        $records += [pscustomobject]@{
            frame = $frame
            png = $png
            sha256 = (Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
            state = (($text -split "`r?`n" | Where-Object { $_ -like 'gameplay-state *' } | Select-Object -First 1))
        }
    }
    return $records
}

function Assert-RimRattleDeterministic($Left, $Right) {
    if ($Left.Count -ne $Right.Count) { throw 'Rim-rattle repeat count changed.' }
    for ($i = 0; $i -lt $Left.Count; ++$i) {
        Assert-Equal $Right[$i].frame $Left[$i].frame 'Rim-rattle repeated frame'
        Assert-Equal $Right[$i].sha256 $Left[$i].sha256 "Nondeterministic rim-rattle frame $($Left[$i].frame)"
    }
}

function New-ContactSheet([string]$Directory, [string]$Scenario,
                          [string]$FileName, [string[]]$Labels) {
    Add-Type -AssemblyName System.Drawing
    $records = @()
    foreach ($label in $Labels) {
        $match = @(Get-ChildItem -LiteralPath $Directory -Filter "$Scenario-*-$label.png" |
            Sort-Object Name | Select-Object -First 1)
        if ($match.Count -eq 1) {
            $records += [pscustomobject]@{ label=$label; path=$match[0].FullName; name=$match[0].Name }
        }
    }
    if ($records.Count -eq 0) { throw "No contact-sheet PNGs found for $Scenario." }
    $width = 320; $height = 240; $labelHeight = 30; $columns = 4
    $rows = [Math]::Ceiling($records.Count / $columns)
    $sheet = New-Object System.Drawing.Bitmap ($width * $columns), (($height + $labelHeight) * $rows)
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    $font = New-Object System.Drawing.Font('Consolas', 9, [System.Drawing.FontStyle]::Bold)
    try {
        $graphics.Clear([System.Drawing.Color]::Black)
        for ($i = 0; $i -lt $records.Count; ++$i) {
            $record = $records[$i]
            $x = ($i % $columns) * $width
            $y = [Math]::Floor($i / $columns) * ($height + $labelHeight)
            $image = [System.Drawing.Image]::FromFile($record.path)
            try { $graphics.DrawImage($image, $x, $y, $width, $height) }
            finally { $image.Dispose() }
            $graphics.DrawString($record.label, $font, [System.Drawing.Brushes]::White,
                                 $x + 3, $y + $height + 5)
        }
        $path = Join-Path $OutputRoot $FileName
        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        return $path
    } finally {
        $font.Dispose(); $graphics.Dispose(); $sheet.Dispose()
    }
}

function New-RimRattleContactSheet($Records) {
    Add-Type -AssemblyName System.Drawing
    $width = 320; $height = 240; $labelHeight = 30; $columns = 3
    $rows = [Math]::Ceiling($Records.Count / $columns)
    $sheet = New-Object System.Drawing.Bitmap ($width * $columns), (($height + $labelHeight) * $rows)
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    $font = New-Object System.Drawing.Font('Consolas', 9, [System.Drawing.FontStyle]::Bold)
    try {
        $graphics.Clear([System.Drawing.Color]::Black)
        for ($i = 0; $i -lt $Records.Count; ++$i) {
            $x = ($i % $columns) * $width
            $y = [Math]::Floor($i / $columns) * ($height + $labelHeight)
            $image = [System.Drawing.Image]::FromFile($Records[$i].png)
            try { $graphics.DrawImage($image, $x, $y, $width, $height) }
            finally { $image.Dispose() }
            $graphics.DrawString(("rim-rattle frame {0}" -f $Records[$i].frame),
                                 $font, [System.Drawing.Brushes]::White,
                                 $x + 3, $y + $height + 5)
        }
        $path = Join-Path $OutputRoot 'rim-rattle-render-contact-sheet.png'
        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        return $path
    } finally {
        $font.Dispose(); $graphics.Dispose(); $sheet.Dispose()
    }
}

Invoke-Trace $RunA
Invoke-Trace $RunB
Assert-RunDeterministic $RunA $RunB
$RattleA = @(Invoke-RimRattle (Join-Path $RunA 'rim-rattle'))
$RattleB = @(Invoke-RimRattle (Join-Path $RunB 'rim-rattle'))
Assert-RimRattleDeterministic $RattleA $RattleB

$PrimaryName = 'bulls-pacers-away-pulse'
$SecondName = 'new-york-philadelphia-home-hold'
$CpuName = 'bulls-pacers-cpu-only'
$NoInputName = 'bulls-pacers-no-input'
$Primary = Get-ScenarioRows $RunA $PrimaryName
$Second = Get-ScenarioRows $RunA $SecondName
$Cpu = Get-ScenarioRows $RunA $CpuName
$NoInput = Get-ScenarioRows $RunA $NoInputName
$AllRows = @($Primary + $Second + $Cpu + $NoInput)

$PreTipSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\tecmo_gameplay_pretip.c') -Raw
Assert-True ($PreTipSource -notmatch 'CAPTURE_CLOCK_TERMINAL_ENTRY') 'A calibrated capture-clock terminal-entry seam was reintroduced.'
Assert-True ($PreTipSource -notmatch 'tip_capture_clock[^\r\n]*0xF4') 'A literal $F4 capture-clock seam was reintroduced.'

Assert-True (@($AllRows | Where-Object { $_.render_ok -ne '1' }).Count -eq 0) 'A continuous GUI-equivalent render returned false.'
foreach ($row in $AllRows) {
    if ($row.jump_active -eq '1') {
        Assert-Equal $row.away_x $row.away_anchor_x "Anchored away jumper at scene frame $($row.scene_frame)"
        Assert-Equal $row.away_y $row.away_anchor_y "Anchored away jumper depth at scene frame $($row.scene_frame)"
        Assert-Equal $row.home_x $row.home_anchor_x "Anchored home jumper at scene frame $($row.scene_frame)"
        Assert-Equal $row.home_y $row.home_anchor_y "Anchored home jumper depth at scene frame $($row.scene_frame)"
    }
}

$Capture = Get-One $Primary { $_.total_frame -eq '452' } 'primary Bank04 capture'
$DerivedSource = Get-Bank04SourceAfter ([int]$Capture.capture_source_initial) 0x5A ([int]$Capture.capture_source_mix_count)
$DerivedSeed = (($DerivedSource -band 0x3F) + 0x82) -band 0xFF
$DerivedClock = ($DerivedSeed + [int]$Capture.capture_ticks) -band 0xFF
$DerivedError = Get-Bank04Error ([int]$Capture.away_capture_clock)
Assert-Equal $Capture.capture_source_initial 0 'Bank04 initial $6A'
Assert-Equal $Capture.capture_source_current $DerivedSource 'Bank04 evolved $6A from fixed $CD96 mixer'
Assert-Equal $Capture.capture_source_6a $DerivedSource 'Bank04 sampled $6A'
Assert-Equal $DerivedSource 133 'Deterministic presentation source $6A ($85)'
Assert-Equal $DerivedSeed 135 'Bank04 initial $8A from ($6A & $3F)+$82 ($87)'
Assert-Equal $Capture.away_capture_clock 225 'Bank04 captured $8A ($E1)'
Assert-Equal $Capture.away_error $DerivedError 'Bank04 wrapped-distance error'
Assert-Equal $Capture.away_countdown $DerivedError 'Bank04 derived countdown'
Assert-Equal $Capture.capture_clock $DerivedClock 'Bank04 next scheduler clock ($E2)'
Assert-Equal $Capture.capture_ticks 91 'Bank04 elapsed source-loop ticks'
Assert-Equal $Capture.capture_scheduler_phase 3 'Bank04 scheduler phase after capture (poll-first)'
Assert-Equal $Capture.capture_scheduler_yields (7 + 20 + 2 * [int]$Capture.capture_ticks) 'Bank04 exact route/prepare/poll yield count'
Assert-Equal $Capture.capture_special_yields 0 'Bank04 pre-marker special-yield count'
Assert-True ($Capture.away_capture_clock -ne '244') 'The unproven $F4 terminal-entry calibration was accepted.'
Assert-True ($Capture.away_capture_clock -ne '249' -and $Capture.away_error -ne '0') 'Invented phase-zero=$F9 timing was accepted.'

$SchedulerComplete = Get-One $Primary { $_.total_frame -eq '514' } 'completed Bank04 scheduler'
$WrapTicks = 0x100 - $DerivedSeed
Assert-Equal $SchedulerComplete.capture_clock 0 'Bank04 scheduler wrapped $8A'
Assert-Equal $SchedulerComplete.capture_ticks $WrapTicks 'Bank04 exact seed-to-wrap tick count'
Assert-Equal $SchedulerComplete.capture_scheduler_phase 6 'Bank04 completed scheduler phase'
Assert-Equal $SchedulerComplete.capture_special_yields 2 'Bank04 $F6/$F9 special waits'
Assert-Equal $SchedulerComplete.capture_scheduler_yields (7 + 20 + 2 * $WrapTicks + 2) 'Bank04 exact total scheduler yields'

$LastCourt = Get-One $Primary { $_.total_frame -eq '515' } 'last pre-cinematic court'
$First = Get-One $Primary { $_.phase -eq 'toss-closeup' -and $_.phase_frame -eq '0' } 'first cinematic'
$Middle = Get-One $Primary { $_.phase -eq 'toss-closeup' -and $_.phase_frame -eq '30' } 'middle cinematic'
$Last = Get-One $Primary { $_.phase -eq 'toss-closeup' -and $_.phase_frame -eq '59' } 'last cinematic'
$Return = Get-One $Primary { $_.phase -eq 'jump-contest' -and $_.phase_frame -eq '0' } 'first returned court'
$Resumed = Get-One $Primary { $_.phase -eq 'jump-contest' -and $_.phase_frame -eq '1' } 'resumed court physics'
$Handoff = Get-One $Primary { $_.phase -eq 'live' -and $_.live_handoff -eq '1' -and $_.scene_frame -eq '606' } 'live handoff'

Assert-Equal $First.total_frame 516 'Source-scheduled primary first cinematic frame'
Assert-True ([int]$First.total_frame -ne 498 -and [int]$First.total_frame -ne 508) 'An injected fast/legacy cinematic frame was accepted.'
Assert-True ([int]$LastCourt.cinematic -eq 0 -and [int]$LastCourt.away_altitude_q8 -gt 0) 'Last court frame was not visibly airborne.'
Assert-True (@($Primary | Where-Object { $_.cinematic -eq '0' -and [int]$_.away_altitude_q8 -gt 0 -and [int]$_.total_frame -lt [int]$First.total_frame }).Count -gt 0) 'No visible human jump preceded the cutaway.'
$Frozen = Get-JumpTuple $First
foreach ($row in @($Middle,$Last,$Return)) { Assert-Equal (Get-JumpTuple $row) $Frozen "Frozen jumper tuple at $($row.phase)/$($row.phase_frame)" }
Assert-True ([int]$Return.away_altitude_q8 -gt 0) 'Court return forcibly grounded the jumper.'
Assert-True ((Get-JumpTuple $Resumed) -ne $Frozen) 'Court return did not resume normal airborne physics.'
Assert-Equal $Handoff.holder $Handoff.receiver 'Receiver-directed live handoff holder'
Assert-Equal $Handoff.possession 0 'Away winner live possession'
$Landing = Get-First $Primary { $_.away_state -eq '19' -and $_.away_altitude_q8 -eq '0' } 'primary natural landing'
Assert-True (@($Primary | Where-Object { $_.live_handoff -eq '1' -and [int]$_.scene_frame -ge ([int]$Landing.scene_frame + 8) }).Count -gt 0) 'No rendered live frames followed natural recovery.'

$SecondCapture = Get-One $Second { $_.total_frame -eq '455' } 'home hold capture'
$SecondFirst = Get-One $Second { $_.phase -eq 'toss-closeup' -and $_.phase_frame -eq '0' } 'home cinematic'
$SecondHandoff = Get-First $Second { $_.phase -eq 'live' -and $_.live_handoff -eq '1' -and $_.phase_frame -eq '0' } 'home live handoff'
Assert-Equal $SecondCapture.capture_source_current $DerivedSource 'Second matchup evolved the same deterministic source schedule'
Assert-Equal $SecondCapture.home_capture_clock 227 'Held/released home Bank04 capture ($E3)'
Assert-Equal $SecondCapture.home_error (Get-Bank04Error 227) 'Held/released home countdown'
Assert-Equal $SecondCapture.capture_scheduler_phase 4 'Held/released scheduler phase (poll-second)'
Assert-Equal $SecondCapture.capture_scheduler_yields 212 'Held/released exact scheduler yield count'
Assert-True ([int]$SecondFirst.total_frame -ne 498 -and [int]$SecondFirst.total_frame -ne 508) 'Late valid capture used an injected cinematic frame.'
Assert-Equal $SecondHandoff.holder $SecondHandoff.receiver 'Home receiver-directed handoff holder'
Assert-Equal $SecondHandoff.possession 1 'Home winner live possession'

$CpuFirst = Get-One $Cpu { $_.phase -eq 'toss-closeup' -and $_.phase_frame -eq '0' } 'CPU-only cinematic'
Assert-True ($CpuFirst.away_sampled -eq '1' -and $CpuFirst.home_sampled -eq '1' -and
             $CpuFirst.away_capture_clock -eq '0' -and $CpuFirst.home_capture_clock -eq '0') 'CPU-only route fabricated a Bank04 human sample.'
Assert-True ([int]$CpuFirst.away_committed -eq 1 -and [int]$CpuFirst.home_committed -eq 1) 'CPU-only tip did not exercise both automatic jumpers.'

Assert-True (@($NoInput | Where-Object { $_.cinematic -ne '0' -or $_.away_committed -ne '0' -or $_.home_committed -ne '0' }).Count -eq 0) 'No-input route unexpectedly resolved a tip.'
$NoInputFinal = $NoInput[-1]
Assert-Equal $NoInputFinal.phase 'ball-descent' 'No-input terminal phase'

$PrimaryContact = New-ContactSheet $RunA $PrimaryName 'tipoff-primary-continuous-contact-sheet.png' @(
    'capture','visible-airborne','last-court','first-cinematic','cinematic-middle',
    'cinematic-last','court-return','resumed-physics','natural-landing',
    'live-handoff','live-after-landing'
)
$SecondContact = New-ContactSheet $RunA $SecondName 'tipoff-new-york-philadelphia-contact-sheet.png' @(
    'capture','visible-airborne','last-court','first-cinematic','cinematic-middle',
    'cinematic-last','court-return','resumed-physics','natural-landing',
    'live-handoff','live-after-landing'
)
$RattleContact = New-RimRattleContactSheet $RattleA

$MetadataPath = Join-Path $OutputRoot 'tipoff-regression-final.json'
[pscustomobject]@{
    schema = 'tecmo.tipoff-regression-final/2'
    asset_pack = $AssetPackPath
    assertions_passed = $true
    deterministic_repeat = $true
    source_scheduler = [pscustomobject]@{
        scheduler_phase = 'poll-first'
        source_6a_initial = [int]$Capture.capture_source_initial
        source_6a_current = [int]$Capture.capture_source_current
        source_mix_count = [int]$Capture.capture_source_mix_count
        source_mixer = 'Bank04 fixed $CD96, mixer byte $53=$5A'
        initial_clock = ('0x{0:X2}' -f $DerivedSeed)
        captured_clock = ('0x{0:X2}' -f [int]$Capture.away_capture_clock)
        next_clock = ('0x{0:X2}' -f [int]$Capture.capture_clock)
        elapsed_ticks = [int]$Capture.capture_ticks
        exact_yields_at_capture = [int]$Capture.capture_scheduler_yields
        special_yields_at_capture = [int]$Capture.capture_special_yields
        completed_phase = 'complete'
        exact_yields_at_wrap = [int]$SchedulerComplete.capture_scheduler_yields
        special_yields_at_wrap = [int]$SchedulerComplete.capture_special_yields
        target_clock = '0xF9'
        error = [int]$Capture.away_error
        countdown = [int]$Capture.away_countdown
        lifecycle = [pscustomobject]@{
            last_court = [int]$LastCourt.total_frame
            first_cinematic = [int]$First.total_frame
            first_return = [int]$Return.total_frame
            live_handoff = [int]$Handoff.scene_frame
        }
        evidence_boundary = 'Exact Bank04 byte mixer, seed formula, poll cadence, marker waits, and countdown arithmetic; port visible-phase mapping remains a deterministic bridge tied to the presentation update schedule.'
    }
    matchups = @(
        [pscustomobject]@{ scenario=$PrimaryName; away_team=3; away='CHICAGO BULLS'; home_team=10; home='INDIANA PACERS' },
        [pscustomobject]@{ scenario=$SecondName; away_team=17; away='NEW YORK KNICKS'; home_team=19; home='PHILADELPHIA SEVENTY SIXERS' }
    )
    contact_sheets = @($PrimaryContact,$SecondContact,$RattleContact)
    rim_rattle = [pscustomobject]@{
        assertions_passed = $true
        deterministic_repeat = $true
        contact_sheet = $RattleContact
        frames = $RattleA
    }
    traces = @(
        [pscustomobject]@{ scenario=$PrimaryName; csv=(Join-Path $RunA "$PrimaryName-trace.csv"); rows=$Primary.Count },
        [pscustomobject]@{ scenario=$SecondName; csv=(Join-Path $RunA "$SecondName-trace.csv"); rows=$Second.Count },
        [pscustomobject]@{ scenario=$CpuName; csv=(Join-Path $RunA "$CpuName-trace.csv"); rows=$Cpu.Count },
        [pscustomobject]@{ scenario=$NoInputName; csv=(Join-Path $RunA "$NoInputName-trace.csv"); rows=$NoInput.Count }
    )
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $MetadataPath -Encoding UTF8

Write-Host 'TIP-OFF REGRESSION FINAL PROOF PASS'
Write-Host "Metadata: $MetadataPath"
Write-Host "Primary contact sheet: $PrimaryContact"
Write-Host "Second-matchup contact sheet: $SecondContact"
Write-Host "Rim-rattle contact sheet: $RattleContact"
