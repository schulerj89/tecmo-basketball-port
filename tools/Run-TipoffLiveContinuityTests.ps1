param(
    [string]$ProjectRoot,
    [string]$RomPath,
    [string]$AssetPackPath,
    [switch]$Build
)

$ErrorActionPreference='Stop'
if(!$ProjectRoot){$ProjectRoot=Split-Path -Parent $PSScriptRoot}
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
if(!$RomPath){$RomPath=$env:TECMO_ROM_PATH}
if(!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)){throw 'Pass -RomPath or set TECMO_ROM_PATH.'}
$RomPath=(Resolve-Path -LiteralPath $RomPath).Path
if(!$AssetPackPath){$AssetPackPath=Join-Path $ProjectRoot 'build\gameplay-pretip-tests\tecmo.assetpack'}
if($Build){& (Join-Path $ProjectRoot 'build.ps1');if($LASTEXITCODE -ne 0){throw 'Build failed.'}}

$Rom=[IO.File]::ReadAllBytes($RomPath)
if($Rom.Length -ne 393232 -or (Get-FileHash $RomPath -Algorithm SHA256).Hash -ne '076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4'){throw 'Canonical Rev1 ROM identity rejected.'}
function Assert-Span([int]$Bank,[int]$Cpu,[int]$Length,[bool]$Fixed,[string]$Expected){
    $Base=if($Fixed){0xC000}else{0x8000};$Offset=16+$Bank*0x4000+($Cpu-$Base)
    $Bytes=New-Object byte[] $Length;[Array]::Copy($Rom,$Offset,$Bytes,0,$Length)
    $Sha=[Security.Cryptography.SHA256]::Create();try{$Hash=([BitConverter]::ToString($Sha.ComputeHash($Bytes))).Replace('-','')}finally{$Sha.Dispose()}
    if($Hash -ne $Expected){throw ('Source span Bank{0} ${1:X4} rejected.' -f $Bank,$Cpu)}
}
Assert-Span 7 0xF024 53 $true '3C0FE0337190EF2A7A57082BDB3E054CCCB806248DBE2567DB5C307DF8A0AE42'
Assert-Span 5 0xA274 107 $false '71A6BCE1DD326193B354F7E4D721D6D3DBEC9854C362B2F9BD61C8EBCE910D4E'
Assert-Span 5 0x86BB 224 $false 'B36772055E1210601A38891441996A6F8D733DBE928B981DDFC543DB88E578FE'
Assert-Span 6 0x827E 53 $false 'B68FF871D994DB4E846DAD435FC1D79D2E4374458992C29D62AD8AC32BF2DCCD'

$Exe=Join-Path $ProjectRoot 'build\tecmo_port.exe'
& $Exe --gameplay-pretip-test $AssetPackPath
if($LASTEXITCODE -ne 0){throw 'Focused pre-tip continuity regression failed.'}
& (Join-Path $ProjectRoot 'tools\New-TipoffLiveContinuityProof.ps1') -ProjectRoot $ProjectRoot -AssetPackPath $AssetPackPath
Write-Host 'TIP-OFF LIVE CONTINUITY TEST PASS: fixed loop, claim, recovery, Bank06 dispatch, in-place scene handoff, deterministic native visual proof'
