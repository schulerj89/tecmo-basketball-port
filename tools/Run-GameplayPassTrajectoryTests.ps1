param(
    [string]$RomPath = $env:TECMO_ROM_PATH
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RomPath) -or !(Test-Path -LiteralPath $RomPath)) {
    throw 'Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM.'
}

$ExpectedSha256 = '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'
$Spans = @(
    [pscustomobject]@{ Name='launch'; Bank=5; Start=0xB074; End=0xB0FD; Fnv='D444B867' },
    [pscustomobject]@{ Name='interception'; Bank=5; Start=0xB13F; End=0xB1B8; Fnv='7563EC59' },
    [pscustomobject]@{ Name='interception-table'; Bank=5; Start=0xB1B9; End=0xB1D0; Fnv='D6E79B2D' },
    [pscustomobject]@{ Name='flight-catch'; Bank=5; Start=0xB1E7; End=0xB2F1; Fnv='5BBDEF81' },
    [pscustomobject]@{ Name='solver'; Bank=5; Start=0xB42F; End=0xB4B5; Fnv='FF547B48' },
    [pscustomobject]@{ Name='planar-step'; Bank=5; Start=0xB500; End=0xB521; Fnv='67318049' },
    [pscustomobject]@{ Name='gravity'; Bank=5; Start=0xB678; End=0xB6E4; Fnv='026830BD' },
    [pscustomobject]@{ Name='claimant-settlement'; Bank=5; Start=0xB87C; End=0xB98A; Fnv='A5AF16F0' },
    [pscustomobject]@{ Name='signed-half'; Bank=5; Start=0xAA84; End=0xAA9E; Fnv='26D17BB7' },
    [pscustomobject]@{ Name='table-and-kernels'; Bank=5; Start=0xBB9F; End=0xBDC6; Fnv='2E9E77D2' },
    [pscustomobject]@{ Name='profile4-to-0533'; Bank=2; Start=0xA8BC; End=0xA8D2; Fnv='16C69F5A' }
)

function Get-BankOffset([int]$Bank, [int]$Cpu) {
    return 16 + $Bank * 0x4000 + ($Cpu - 0x8000)
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
        $Offset = Get-BankOffset $Span.Bank $Span.Start
        $Count = $Span.End - $Span.Start + 1
        if ((Get-Fnv1a32 $Bytes $Offset $Count) -ne $Span.Fnv) {
            return $false
        }
    }
    $TableOffset = Get-BankOffset 5 0xBBA1
    for ($Index = 0; $Index -lt 256; ++$Index) {
        $Expected = [Math]::Max(1, [Math]::Floor($Index / 7))
        if ($Bytes[$TableOffset + $Index] -ne $Expected) {
            return $false
        }
    }
    $InterceptionExpected = [byte[]]@(
        0x28,0x20,0x10,0,0,0,0,0,
        0x38,0x24,0x18,0x04,0,0,0,0,
        0x40,0x38,0x30,0x28,0x20,0,0,0)
    $InterceptionOffset = Get-BankOffset 5 0xB1B9
    for ($Index = 0; $Index -lt $InterceptionExpected.Length; ++$Index) {
        if ($Bytes[$InterceptionOffset + $Index] -ne
            $InterceptionExpected[$Index]) {
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
$TableOffset = Get-BankOffset 5 0xBBA1
for ($Index = 0; $Index -lt 256; ++$Index) {
    $Mutated = [byte[]]$Rom.Clone()
    $Mutated[$TableOffset + $Index] = $Mutated[$TableOffset + $Index] -bxor 1
    if (Test-SourceContract $Mutated) {
        throw "Pass duration mutation at table index $Index was accepted."
    }
}

$InterceptionOffset = Get-BankOffset 5 0xB1B9
for ($Index = 0; $Index -lt 24; ++$Index) {
    $Mutated = [byte[]]$Rom.Clone()
    $Mutated[$InterceptionOffset + $Index] =
        $Mutated[$InterceptionOffset + $Index] -bxor 1
    if (Test-SourceContract $Mutated) {
        throw "Pass interception mutation at table index $Index was accepted."
    }
}

# Pin execution code as well as data: mutate the middle byte of each decoded
# source span and require the contract to reject it.
foreach ($Span in $Spans) {
    $Mutated = [byte[]]$Rom.Clone()
    $MiddleCpu = [int](($Span.Start + $Span.End) / 2)
    $Offset = Get-BankOffset $Span.Bank $MiddleCpu
    $Mutated[$Offset] = $Mutated[$Offset] -bxor 1
    if (Test-SourceContract $Mutated) {
        throw "Pass trajectory mutation for '$($Span.Name)' was accepted."
    }
}

Write-Host ('Gameplay pass trajectory/interception source tests passed: exact ' +
    'Rev1 SHA-256, 11 execution/data FNV spans, exhaustive 256-byte BBA1 ' +
    'formula and 24-byte B1B9 table, 280 table mutations, and 11 ' +
    'execution-span mutations.')
