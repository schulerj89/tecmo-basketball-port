param(
    [string]$RomPath = $env:TECMO_ROM_PATH
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RomPath) -or !(Test-Path -LiteralPath $RomPath)) {
    throw 'Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM.'
}

$ExpectedSha256 = '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'
$Spans = @(
    [pscustomobject]@{ Name='launch'; Start=0xB074; End=0xB0FD; Fnv='D444B867' },
    [pscustomobject]@{ Name='flight-catch'; Start=0xB1E7; End=0xB2F1; Fnv='5BBDEF81' },
    [pscustomobject]@{ Name='solver'; Start=0xB42F; End=0xB4B5; Fnv='FF547B48' },
    [pscustomobject]@{ Name='planar-step'; Start=0xB500; End=0xB521; Fnv='67318049' },
    [pscustomobject]@{ Name='gravity'; Start=0xB678; End=0xB6E4; Fnv='026830BD' },
    [pscustomobject]@{ Name='signed-half'; Start=0xAA84; End=0xAA9E; Fnv='26D17BB7' },
    [pscustomobject]@{ Name='table-and-kernels'; Start=0xBB9F; End=0xBDC6; Fnv='2E9E77D2' }
)

function Get-Bank05Offset([int]$Cpu) {
    return 16 + 5 * 0x4000 + ($Cpu - 0x8000)
}

function Get-Fnv1a32([byte[]]$Bytes, [int]$Offset, [int]$Count) {
    [uint64]$Hash = 2166136261
    for ($Index = 0; $Index -lt $Count; ++$Index) {
        $Hash = (($Hash -bxor $Bytes[$Offset + $Index]) * 16777619) % 4294967296
    }
    return ('{0:X8}' -f $Hash)
}

function Test-SourceContract([byte[]]$Bytes) {
    if ($Bytes.Length -lt (16 + 8 * 0x4000) -or
        $Bytes[0] -ne 0x4E -or $Bytes[1] -ne 0x45 -or
        $Bytes[2] -ne 0x53 -or $Bytes[3] -ne 0x1A -or
        $Bytes[4] -ne 8) {
        return $false
    }
    foreach ($Span in $Spans) {
        $Offset = Get-Bank05Offset $Span.Start
        $Count = $Span.End - $Span.Start + 1
        if ((Get-Fnv1a32 $Bytes $Offset $Count) -ne $Span.Fnv) {
            return $false
        }
    }
    $TableOffset = Get-Bank05Offset 0xBBA1
    for ($Index = 0; $Index -lt 256; ++$Index) {
        $Expected = [Math]::Max(1, [Math]::Floor($Index / 7))
        if ($Bytes[$TableOffset + $Index] -ne $Expected) {
            return $false
        }
    }
    return $true
}

$Rom = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $RomPath))
$Sha256 = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash
if ($Sha256 -ne $ExpectedSha256) {
    throw "Pass trajectory source ROM SHA-256 mismatch: $Sha256"
}
if (!(Test-SourceContract $Rom)) {
    throw 'Pass trajectory source spans/table rejected the exact Rev1 ROM.'
}

# Every one of the 256 duration bytes must be identity-bearing, not merely a
# few sampled boundaries. A one-bit mutation at every table index must fail.
$TableOffset = Get-Bank05Offset 0xBBA1
for ($Index = 0; $Index -lt 256; ++$Index) {
    $Mutated = [byte[]]$Rom.Clone()
    $Mutated[$TableOffset + $Index] = $Mutated[$TableOffset + $Index] -bxor 1
    if (Test-SourceContract $Mutated) {
        throw "Pass duration mutation at table index $Index was accepted."
    }
}

# Pin execution code as well as data: mutate the middle byte of each decoded
# source span and require the contract to reject it.
foreach ($Span in $Spans) {
    $Mutated = [byte[]]$Rom.Clone()
    $MiddleCpu = [int](($Span.Start + $Span.End) / 2)
    $Offset = Get-Bank05Offset $MiddleCpu
    $Mutated[$Offset] = $Mutated[$Offset] -bxor 1
    if (Test-SourceContract $Mutated) {
        throw "Pass trajectory mutation for '$($Span.Name)' was accepted."
    }
}

Write-Host ('Gameplay pass trajectory source tests passed: exact Rev1 SHA-256, ' +
    '7 Bank05 execution/data FNV spans, exhaustive 256-byte BBA1 formula, ' +
    '256 table mutations, and 7 execution-span mutations.')
