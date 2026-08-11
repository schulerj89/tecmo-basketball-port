param(
    [string]$ProjectRoot,
    [string]$RomPath
)

$ErrorActionPreference = "Stop"
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ExpectedHarnessLine =
    "TGAI-2 opcode workspace harness: opcode7=defer opcode10=exact-harness opcode16=exact-harness ba=external-lifecycle"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) { $RomPath = $env:TECMO_ROM_PATH }
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne
        $ExpectedRomSha256) {
    throw "CPU workspace proof requires the exact Tecmo NBA Basketball Rev1 ROM."
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-CanonicalRange {
    param(
        [byte[]]$Rom,
        [int]$Bank,
        [int]$Start,
        [int]$End
    )
    if ($Bank -lt 0 -or $Bank -gt 7 -or $Start -lt 0x8000 -or
        $End -lt $Start -or $End -gt 0xBFFF) {
        throw "Invalid canonical PRG range."
    }
    $Offset = 16 + $Bank * 0x4000 + ($Start - 0x8000)
    $Count = $End - $Start + 1
    if ($Offset -lt 16 -or $Count -lt 1 -or
        $Offset -gt $Rom.Length - $Count) {
        throw "Canonical PRG range escaped the ROM."
    }
    $Result = New-Object byte[] $Count
    [Array]::Copy($Rom, $Offset, $Result, 0, $Count)
    return $Result
}

function Assert-CanonicalRange {
    param(
        [byte[]]$Rom,
        [int]$Bank,
        [int]$Start,
        [int]$End,
        [string]$ExpectedFnv
    )
    $Actual = Get-Fnv1a32 (Get-CanonicalRange $Rom $Bank $Start $End)
    if ($Actual -ne $ExpectedFnv) {
        throw ('Canonical source fingerprint changed for bank {0:X2} ' +
               '${1:X4}-${2:X4}: expected {3}, got {4}' -f
               $Bank, $Start, $End, $ExpectedFnv, $Actual)
    }
}

$Rom = [IO.File]::ReadAllBytes($RomPath)
if ($Rom.Length -ne 393232 -or $Rom[0] -ne 0x4E -or
    $Rom[1] -ne 0x45 -or $Rom[2] -ne 0x53 -or $Rom[3] -ne 0x1A -or
    $Rom[4] -ne 8) {
    throw "Canonical ROM header/size contract rejected."
}

# These hashes are provenance checks only. The harness never embeds, loads,
# or emits ROM code/table bytes.
Assert-CanonicalRange $Rom 6 0x8F12 0x8F29 "495E0788"
Assert-CanonicalRange $Rom 6 0x8CD0 0x8ED3 "5661731D"
Assert-CanonicalRange $Rom 6 0x9C97 0x9C9A "A27B0F6F"
Assert-CanonicalRange $Rom 6 0x9085 0x90DF "EBDD5956"
Assert-CanonicalRange $Rom 6 0x92BA 0x9314 "087BF69F"
Assert-CanonicalRange $Rom 5 0x9054 0x90AF "FE092D62"
Assert-CanonicalRange $Rom 5 0x86BB 0x879A "15CFFC00"
Assert-CanonicalRange $Rom 5 0x8FAD 0x8FE7 "7C94E5EA"
Assert-CanonicalRange $Rom 6 0x943B 0x9465 "D9664D46"
Assert-CanonicalRange $Rom 6 0x9621 0x9764 "F2543C57"
Assert-CanonicalRange $Rom 4 0x9F2E 0xAC75 "71331A96"

$Commands = Get-CanonicalRange $Rom 4 0x9F2E 0xAC75
[int]$Opcode7 = 0
[int]$Opcode10 = 0
[int]$Opcode16 = 0
for ($Offset = 0; $Offset -lt $Commands.Length; $Offset += 5) {
    switch ($Commands[$Offset]) {
    7 {
        ++$Opcode7
        if ($Commands[$Offset + 1] -ne 0x0A) {
            throw "Canonical opcode-7 probe contract changed."
        }
    }
    10 { ++$Opcode10 }
    16 {
        ++$Opcode16
        if ($Commands[$Offset + 1] -ne 0x09 -or
            $Commands[$Offset + 2] -ne 0x03) {
            throw "Canonical opcode-16 pointer contract changed."
        }
    }
    }
}
if ($Opcode7 -ne 2 -or $Opcode10 -ne 1 -or $Opcode16 -ne 2) {
    throw "Canonical focused opcode counts changed."
}

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
    throw "vswhere.exe was not found."
}
$VsPath = & $VsWhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (!$VsPath) { throw "MSVC tools were not found." }
$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path -LiteralPath $VcVars -PathType Leaf)) {
    throw "vcvars64.bat was not found."
}

$BuildRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "build"))
$Scratch = [IO.Path]::GetFullPath(
    (Join-Path $BuildRoot "cpu_opcode_workspace_harness"))
$BuildPrefix = $BuildRoot.TrimEnd('\', '/') +
    [IO.Path]::DirectorySeparatorChar
if (!$Scratch.StartsWith($BuildPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Workspace harness scratch path escaped build\\."
}
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$Executable = Join-Path $Scratch "cpu_opcode_workspace_harness.exe"
$Compile = "call `"$VcVars`" >nul && cd /d `"$ProjectRoot`" && " +
    "cl /nologo /std:c11 /W4 /WX /Iinclude " +
    "/Fe:`"$Executable`" " +
    "src\\tecmo_gameplay_cpu_opcode_workspaces.c " +
    "src\\tecmo_gameplay_cpu_opcode_workspaces_test.c"
$BuildOutput = @(& cmd.exe /d /c $Compile 2>&1)
if ($LASTEXITCODE -ne 0 -or
    @($BuildOutput | Where-Object { $_ -match 'warning [A-Z]+[0-9]+:' }).Count -ne 0) {
    throw ("Warning-clean opcode workspace harness build failed.`n" +
           ($BuildOutput -join [Environment]::NewLine))
}
$Output = @(& $Executable 2>&1)
$Text = ($Output -join [Environment]::NewLine).Trim()
if ($LASTEXITCODE -ne 0 -or $Text -ne $ExpectedHarnessLine) {
    throw ("Opcode workspace harness failed.`n" +
           ($Output -join [Environment]::NewLine))
}

Write-Host "CPU opcode workspace provenance: canonical=Rev1 command_table=71331A96 opcode7=2 opcode10=1 opcode16=2"
Write-Host $Text
Write-Host "CPU opcode workspace runner: PASS (standalone harness; LIVE remains deferred)"
