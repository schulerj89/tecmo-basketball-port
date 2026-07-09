param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [switch]$Build,
    [string]$AssetPackPath,
    [string]$ReportPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path $ProjectRoot).Path

$BuildDir = Join-Path $ProjectRoot "build"
$OutputDir = Join-Path $BuildDir "intro_sequence"
if (!$AssetPackPath) {
    $AssetPackPath = Join-Path $OutputDir "tecmo_intro_sequence_test.assetpack"
}
if (![System.IO.Path]::IsPathRooted($AssetPackPath)) {
    $AssetPackPath = Join-Path $ProjectRoot $AssetPackPath
}
$AssetPackPath = [System.IO.Path]::GetFullPath($AssetPackPath)
if (!$ReportPath) {
    $ReportPath = Join-Path $BuildDir "intro_sequence_test_report.json"
}
if (![System.IO.Path]::IsPathRooted($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot $ReportPath
}
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

function Find-ReferenceRom {
    if ($RomPath) {
        if (!(Test-Path $RomPath)) {
            throw "RomPath does not exist."
        }
        return (Resolve-Path $RomPath).Path
    }
    if ($env:TECMO_ROM_PATH -and (Test-Path $env:TECMO_ROM_PATH)) {
        return (Resolve-Path $env:TECMO_ROM_PATH).Path
    }

    return $null
}

function Test-PathUnder {
    param(
        [string]$Path,
        [string]$Parent
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $FullPath.StartsWith($FullParent, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-RepoRelativePath {
    param(
        [string]$Path
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    $FullRoot = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
    $RootWithSlash = $FullRoot + [System.IO.Path]::DirectorySeparatorChar
    if ($FullPath.StartsWith($RootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $FullPath.Substring($RootWithSlash.Length).Replace('\', '/')
    }
    if ($FullPath.Equals($FullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return "."
    }
    return "<outside-project>"
}

function Get-SafeExceptionName {
    param($ErrorRecord)

    if ($ErrorRecord -and $ErrorRecord.Exception) {
        return $ErrorRecord.Exception.GetType().Name
    }
    return "Error"
}

function Get-AssetPackEntryPayloadOffset {
    param(
        [byte[]]$Bytes,
        [string]$EntryId
    )

    if ($Bytes.Length -lt 40 -or
        [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "TAP1") {
        throw "Malformed asset-pack header."
    }

    $EntryCount = [System.BitConverter]::ToUInt32($Bytes, 16)
    $DirectoryOffset = [System.BitConverter]::ToUInt64($Bytes, 20)
    for ($EntryIndex = 0; $EntryIndex -lt $EntryCount; ++$EntryIndex) {
        $EntryOffset64 = $DirectoryOffset + [uint64]($EntryIndex * 128)
        if ($EntryOffset64 + 128 -gt [uint64]$Bytes.Length -or
            $EntryOffset64 -gt [int]::MaxValue) {
            throw "Malformed asset-pack directory."
        }

        $EntryOffset = [int]$EntryOffset64
        $EntryNameLength = 0
        while ($EntryNameLength -lt 64 -and $Bytes[$EntryOffset + $EntryNameLength] -ne 0) {
            ++$EntryNameLength
        }
        $EntryName = [System.Text.Encoding]::ASCII.GetString(
            $Bytes,
            $EntryOffset,
            $EntryNameLength)
        if ($EntryName -eq $EntryId) {
            $PayloadOffset = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 84)
            $PayloadSize = [System.BitConverter]::ToUInt64($Bytes, $EntryOffset + 92)
            if ($PayloadOffset + $PayloadSize -gt [uint64]$Bytes.Length -or
                $PayloadOffset -gt [int]::MaxValue) {
                throw "Malformed asset-pack entry payload."
            }
            return [int]$PayloadOffset
        }
    }

    throw "Asset-pack entry was not found."
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"

$ReferenceRom = Find-ReferenceRom
if (!$ReferenceRom) {
    throw "Could not find a local iNES ROM. Pass -RomPath or set TECMO_ROM_PATH."
}

if (!(Test-PathUnder $AssetPackPath $BuildDir)) {
    throw "Intro sequence asset pack output must stay under build\."
}
if (!(Test-PathUnder $ReportPath $BuildDir)) {
    throw "Intro sequence report must stay under build\."
}
if (!$AssetPackPath.EndsWith(".assetpack", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Intro sequence asset pack output must use the .assetpack extension."
}

if ($Build) {
    $PreviousSkipShortcut = $env:TECMO_SKIP_SHORTCUT
    $env:TECMO_SKIP_SHORTCUT = "1"
    try {
        & (Join-Path $ProjectRoot "build.ps1")
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:TECMO_SKIP_SHORTCUT = $PreviousSkipShortcut
    }
}
if (!(Test-Path $ExePath)) {
    throw "Executable not found at $ExePath. Run .\build.ps1 or pass -Build."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AssetPackPath) | Out-Null
$RomOnlyArenaRenderModes = @(
    "intro-arena-frame0",
    "intro-arena-frame240",
    "intro-arena-frame320"
)

$Results = New-Object System.Collections.Generic.List[object]
$Failures = 0
$Skipped = 0
$AssetPackRelative = Get-RepoRelativePath $AssetPackPath

try {
    if (Test-Path -LiteralPath $AssetPackPath) {
        Remove-Item -LiteralPath $AssetPackPath -Force
    }

    Write-Host "Building intro sequence test asset pack -> $AssetPackRelative"
    $PackBuildOutput = & $ExePath --build-assetpack $ReferenceRom $AssetPackPath 2>&1
    $PackBuildExitCode = $LASTEXITCODE
    $PackCreated = Test-Path -LiteralPath $AssetPackPath
    $PackBuildOutputText = (@($PackBuildOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RomOnlyMessageSeen = $PackBuildOutputText -match "from iNES ROM"
    $PackBuildPassed = $PackBuildExitCode -eq 0 -and $PackCreated -and $RomOnlyMessageSeen
    if (!$PackBuildPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-build"
        command = ".\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> $($AssetPackRelative.Replace('/', '\'))"
        output = $AssetPackRelative
        passed = $PackBuildPassed
        skipped = $false
        exit_code = $PackBuildExitCode
        created = $PackCreated
        rom_only_message_seen = $RomOnlyMessageSeen
        raw_output_persisted = $false
        error = if ($PackBuildPassed) { $null } else { "fresh test asset pack was not built" }
    })

    $ListOutput = & $ExePath --assetpack-list $AssetPackPath 2>&1
    $ListExitCode = $LASTEXITCODE
    $ListText = (@($ListOutput) | ForEach-Object { [string]$_ }) -join "`n"
    $RequiredNativeEntries = @(
        "arena/intro/script",
        "arena/intro/background-layer",
        "arena/intro/palette-cycle",
        "arena/intro/sprite-groups"
    )
    $ForbiddenCaptureEntries = @("intro/arena/capture.ndjson", "intro/post-arena/capture.ndjson", "intro/captures/source-map")
    $MissingNativeEntries = @($RequiredNativeEntries | Where-Object { $ListText -notmatch [regex]::Escape($_) })
    $PresentForbiddenCaptureEntries = @($ForbiddenCaptureEntries | Where-Object { $ListText -match [regex]::Escape($_) })
    $ListPassed = $ListExitCode -eq 0 -and
        $MissingNativeEntries.Count -eq 0 -and
        $PresentForbiddenCaptureEntries.Count -eq 0
    if (!$ListPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-test-assetpack-rom-only"
        output = $AssetPackRelative
        passed = $ListPassed
        skipped = $false
        exit_code = $ListExitCode
        required_native_entries = $RequiredNativeEntries
        missing_native_entries = $MissingNativeEntries
        forbidden_capture_entries = $ForbiddenCaptureEntries
        present_forbidden_capture_entries = $PresentForbiddenCaptureEntries
        raw_output_persisted = $false
        error = if ($ListPassed) { $null } else { "ROM-only test asset pack is missing native arena entries or contains capture entries" }
    })

    $PreviousAssetPack = $env:TECMO_ASSETPACK
    $PreviousLooseArenaCapture = $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE
    $RenderOutputs = New-Object System.Collections.Generic.List[object]
    $RenderPassed = $ListPassed
    try {
        $env:TECMO_ASSETPACK = $AssetPackPath
        Remove-Item Env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE -ErrorAction SilentlyContinue

        foreach ($Mode in $RomOnlyArenaRenderModes) {
            $RenderPath = Join-Path $OutputDir "$Mode.png"
            if (Test-Path -LiteralPath $RenderPath) {
                Remove-Item -LiteralPath $RenderPath -Force
            }
            $RenderOutput = & $ExePath --root $ProjectRoot --render-test-mode $Mode $RenderPath 2>&1
            $RenderExitCode = $LASTEXITCODE
            $RenderText = (@($RenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
            $RenderCreated = Test-Path -LiteralPath $RenderPath
            $CaptureUnavailableSeen = $RenderText -match "intro-capture-status kind=arena available=0"
            $NoCaptureSourceSeen = $RenderText -match "intro-capture-source kind=arena assetpack=0 entry=none"
            $ExactLayerSeen = $RenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=1 cells=1632 palette=16"
            $TasgContractSeen = $RenderText -match "sprite_groups=2 jumbotron_pieces=55 goal_pieces=16"
            $VisibleJumbotronSeen = $RenderText -match "visible_jumbotron=[1-9][0-9]*"
            $VisibleGoalSeen = $RenderText -match "visible_goal=[1-9][0-9]*"
            $FrameVisibilitySeen = if ($Mode -eq "intro-arena-frame0") {
                $VisibleJumbotronSeen
            } elseif ($Mode -eq "intro-arena-frame320") {
                $VisibleGoalSeen
            } else {
                $true
            }
            $ModePassed = $RenderExitCode -eq 0 -and
                $RenderCreated -and
                $CaptureUnavailableSeen -and
                $NoCaptureSourceSeen -and
                $ExactLayerSeen -and
                $TasgContractSeen -and
                $FrameVisibilitySeen
            if (!$ModePassed) {
                $RenderPassed = $false
            }
            $RenderOutputs.Add([pscustomobject]@{
                mode = $Mode
                output = Get-RepoRelativePath $RenderPath
                passed = $ModePassed
                exit_code = $RenderExitCode
                capture_unavailable_seen = $CaptureUnavailableSeen
                no_capture_source_seen = $NoCaptureSourceSeen
                exact_layer_seen = $ExactLayerSeen
                tasg_contract_seen = $TasgContractSeen
                visible_jumbotron_seen = $VisibleJumbotronSeen
                visible_goal_seen = $VisibleGoalSeen
                frame_visibility_seen = $FrameVisibilitySeen
            })
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        $env:TECMO_ALLOW_LOOSE_INTRO_CAPTURE = $PreviousLooseArenaCapture
    }
    if (!$RenderPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-render-rom-only-arena"
        passed = $RenderPassed
        skipped = $false
        render_modes = $RomOnlyArenaRenderModes
        outputs = $RenderOutputs
        rom_only_asset_pack = $AssetPackRelative
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($RenderPassed) { $null } else { "ROM-only arena render did not complete without capture data" }
    })

    $MalformedTasgCases = @(
        [pscustomobject]@{
            id = "top-tile-before-page-8"
            mutation = "u32"
            payload_offset = 108
            value = 8176
        },
        [pscustomobject]@{
            id = "bottom-tile-after-page-9"
            mutation = "u32"
            payload_offset = 108
            value = 10224
        },
        [pscustomobject]@{
            id = "priority-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x04
        },
        [pscustomobject]@{
            id = "reserved-flag-unsupported"
            mutation = "or-byte"
            payload_offset = 113
            value = 0x08
        },
        [pscustomobject]@{
            id = "jumbotron-connector-overlay-unsupported"
            mutation = "i16"
            payload_offset = 114
            value = -1
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-required"
            mutation = "i16"
            payload_offset = 858
            value = 0
        },
        [pscustomobject]@{
            id = "goal-connector-overlay-value-strict"
            mutation = "i16"
            payload_offset = 858
            value = -2
        }
    )
    $MalformedPackPath = Join-Path $OutputDir "malformed-tasg.assetpack"
    $MalformedRenderPath = Join-Path $OutputDir "malformed-tasg.png"
    $MalformedCaseResults = New-Object System.Collections.Generic.List[object]
    $MalformedContractPassed = $PackBuildPassed
    $PreviousAssetPack = $env:TECMO_ASSETPACK
    try {
        if ($PackBuildPassed) {
            foreach ($Case in $MalformedTasgCases) {
                [byte[]]$MalformedBytes = [System.IO.File]::ReadAllBytes($AssetPackPath)
                $TasgPayloadOffset = Get-AssetPackEntryPayloadOffset `
                    -Bytes $MalformedBytes `
                    -EntryId "arena/intro/sprite-groups"
                $MutationOffset = $TasgPayloadOffset + $Case.payload_offset
                if ($Case.mutation -eq "u32") {
                    [System.BitConverter]::GetBytes([uint32]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } elseif ($Case.mutation -eq "i16") {
                    [System.BitConverter]::GetBytes([int16]$Case.value).CopyTo(
                        $MalformedBytes,
                        $MutationOffset)
                } else {
                    $MalformedBytes[$MutationOffset] = [byte](
                        $MalformedBytes[$MutationOffset] -bor [byte]$Case.value)
                }
                [System.IO.File]::WriteAllBytes($MalformedPackPath, $MalformedBytes)

                if (Test-Path -LiteralPath $MalformedRenderPath) {
                    Remove-Item -LiteralPath $MalformedRenderPath -Force
                }
                $env:TECMO_ASSETPACK = $MalformedPackPath
                $MalformedRenderOutput = & $ExePath `
                    --root $ProjectRoot `
                    --render-test-mode intro-arena-frame0 $MalformedRenderPath 2>&1
                $MalformedRenderExitCode = $LASTEXITCODE
                $MalformedRenderText = (@($MalformedRenderOutput) | ForEach-Object { [string]$_ }) -join "`n"
                $Rejected = $MalformedRenderExitCode -eq 1 -and
                    !(Test-Path -LiteralPath $MalformedRenderPath) -and
                    $MalformedRenderText -match "intro-arena-render-source kind=arena exact_layer=1 rendered=0 cells=1632 palette=16 sprite_groups=0 jumbotron_pieces=0 goal_pieces=0 visible_jumbotron=0 visible_goal=0"
                if (!$Rejected) {
                    $MalformedContractPassed = $false
                }
                $MalformedCaseResults.Add([pscustomobject]@{
                    id = $Case.id
                    passed = $Rejected
                    exit_code = $MalformedRenderExitCode
                    rejected_by_runtime = $Rejected
                })
            }
        }
    } finally {
        $env:TECMO_ASSETPACK = $PreviousAssetPack
        Remove-Item -LiteralPath $MalformedPackPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MalformedRenderPath -Force -ErrorAction SilentlyContinue
    }
    if (!$MalformedContractPassed) {
        ++$Failures
    }
    $Results.Add([pscustomobject]@{
        id = "intro-tasg-malformed-contract"
        passed = $MalformedContractPassed
        skipped = $false
        cases = $MalformedCaseResults
        raw_output_persisted = $false
        coverage_status = "covered"
        error = if ($MalformedContractPassed) { $null } else { "runtime accepted a malformed TASG sprite contract" }
    })
} catch {
    ++$Failures
    $Results.Add([pscustomobject]@{
        id = "intro-sequence-runner-error"
        passed = $false
        raw_output_persisted = $false
        private_paths_included = $false
        error = "intro sequence runner failed before completing all checks"
        error_type = Get-SafeExceptionName $_
    })
} finally {
    $Report = [pscustomobject]@{
        schema_version = 1
        generated_by = "tools/Run-IntroSequenceTests.ps1"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        data_policy = "Sanitized intro-sequence smoke report only; raw stdout/stderr, local paths, ROM bytes, ASM, CHR bytes, trace payloads, and screenshots are not embedded."
        passed = $Failures -eq 0
        reference_rom_used = "<local>"
        build_requested = [bool]$Build
        output_directory = (Get-RepoRelativePath $OutputDir)
        asset_pack = [pscustomobject]@{
            output = $AssetPackRelative
            rom_only_contract = $true
        }
        render_coverage = [pscustomobject]@{
            status = "arena-native-chr-smoke"
            render_modes = $RomOnlyArenaRenderModes
        }
        private_paths_included = $false
        raw_output_persisted = $false
        test_count = $Results.Count
        skipped_count = $Skipped
        failure_count = $Failures
        tests = $Results
    }

    $ReportDir = Split-Path -Parent $ReportPath
    if ($ReportDir) {
        New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -Path $ReportPath -Encoding ASCII
}

$Results | Format-Table id, passed, skipped, output -AutoSize
Write-Host "Wrote intro sequence test report: $(Get-RepoRelativePath $ReportPath)"

if ($Failures -ne 0) {
    throw "$Failures intro sequence smoke test(s) failed."
}
Write-Host "All intro sequence smoke tests passed."
