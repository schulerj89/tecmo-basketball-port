$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"
$ObjDir = Join-Path $BuildDir "obj"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $VsWhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with the Desktop development with C++ workload."
}

$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$VsPath) {
    throw "MSVC C++ tools were not found. Install the Desktop development with C++ workload."
}

$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $VcVars)) {
    throw "vcvars64.bat was not found under $VsPath."
}

$ExePath = Join-Path $BuildDir "tecmo_port.exe"
$ObjPrefix = $ObjDir.TrimEnd("\") + "\"
$Sources = @(
    "src\main.c",
    "src\asm_inventory.c",
    "src\png_writer.c",
    "src\tecmo_bank07.c",
    "src\tecmo_controls.c",
    "src\tecmo_flow_test.c",
    "src\tecmo_intro_stage.c",
    "src\tecmo_nes_video.c",
    "src\tecmo_nametable_screen.c",
    "src\tecmo_intro_arena.c",
    "src\tecmo_intro_license.c",
    "src\tecmo_intro_title.c",
    "src\tecmo_memory.c",
    "src\tecmo_game.c",
    "src\win32_platform.c"
)
$SourceArgs = $Sources -join " "
$Command = "call `"$VcVars`" >nul && cd /d `"$Root`" && cl /nologo /std:c11 /W4 /I include /Fo:$ObjPrefix /Fe:`"$ExePath`" $SourceArgs user32.lib gdi32.lib"

& cmd.exe /d /c $Command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built $ExePath"

$ShortcutScript = Join-Path $Root "tools\Update-DesktopShortcut.ps1"
if ((Test-Path $ShortcutScript) -and ($env:TECMO_SKIP_SHORTCUT -ne "1")) {
    & $ShortcutScript -ExePath $ExePath -ProjectRoot $Root
}
