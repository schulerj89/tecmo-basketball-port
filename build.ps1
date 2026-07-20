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

$ConsoleExePath = Join-Path $BuildDir "tecmo_port.exe"
$GameExePath = Join-Path $BuildDir "tecmo_port_game.exe"
$ObjPrefix = $ObjDir.TrimEnd("\") + "\"
$Sources = @(
    "src\main.c",
    "src\asm_inventory.c",
    "src\png_writer.c",
    "src\tecmo_asset_pack.c",
    "src\asset_pack\tecmo_asset_pack_arena.c",
    "src\asset_pack\tecmo_asset_pack_all_star.c",
    "src\asset_pack\tecmo_asset_pack_d9f6.c",
    "src\asset_pack\tecmo_asset_pack_finale.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_audio.c",
    "src\asset_pack\tecmo_asset_pack_gameplay.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_court.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_close_shots.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_dunk_cutaway.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_jump_shots.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_shot_resolution.c",
    "src\asset_pack\tecmo_asset_pack_gameplay_penalties.c",
    "src\asset_pack\tecmo_asset_pack_title.c",
    "src\asset_pack\tecmo_asset_pack_opening.c",
    "src\asset_pack\tecmo_asset_pack_music.c",
    "src\asset_pack\tecmo_asset_pack_post_arena.c",
    "src\asset_pack\tecmo_asset_pack_preseason.c",
    "src\asset_pack\tecmo_asset_pack_season.c",
    "src\asset_pack\tecmo_asset_pack_team_data.c",
    "src\asset_pack\tecmo_asset_pack_team_management.c",
    "src\asset_pack\tecmo_asset_pack_reader.c",
    "src\asset_pack\tecmo_asset_pack_source_map.c",
    "src\asset_pack\tecmo_asset_pack_start_menu.c",
    "src\asset_pack\tecmo_asset_pack_util.c",
    "src\asset_pack\tecmo_asset_pack_writer.c",
    "src\tecmo_bank07.c",
    "src\tecmo_controls.c",
    "src\tecmo_all_star_menu.c",
    "src\tecmo_music.c",
    "src\tecmo_gameplay_audio.c",
    "src\tecmo_audio_output.c",
    "src\tecmo_flow_test.c",
    "src\tecmo_gameplay_assets.c",
    "src\tecmo_gameplay_court.c",
    "src\tecmo_gameplay_close_shots.c",
    "src\tecmo_gameplay_dunk_cutaway.c",
    "src\tecmo_gameplay_jump_shots.c",
    "src\tecmo_gameplay_shot_resolution.c",
    "src\tecmo_gameplay_penalties.c",
    "src\tecmo_gameplay_scene.c",
    "src\tecmo_gameplay_state.c",
    "src\tecmo_intro_stage.c",
    "src\tecmo_nes_video.c",
    "src\tecmo_preseason_menu.c",
    "src\tecmo_season_menu.c",
    "src\tecmo_start_game_menu.c",
    "src\tecmo_team_data.c",
    "src\tecmo_team_management.c",
    "src\tecmo_title_screen.c",
    "src\tecmo_nametable_screen.c",
    "src\tecmo_intro_arena.c",
    "src\tecmo_intro_arena_scene.c",
    "src\tecmo_intro_layout.c",
    "src\tecmo_intro_finale.c",
    "src\tecmo_intro_post_arena.c",
    "src\tecmo_intro_screen.c",
    "src\tecmo_intro_trace.c",
    "src\tecmo_memory.c",
    "src\tecmo_game.c",
    "src\win32_platform.c"
)
$SourceArgs = $Sources -join " "
$ObjectArgs = ($Sources | ForEach-Object {
    $ObjectName = [System.IO.Path]::GetFileNameWithoutExtension($_) + ".obj"
    "`"$(Join-Path $ObjDir $ObjectName)`""
}) -join " "
$BuildSteps = @(
    "cl /nologo /std:c11 /W4 /I include /c /Fo:$ObjPrefix $SourceArgs",
    "link /nologo /out:`"$ConsoleExePath`" $ObjectArgs user32.lib gdi32.lib winmm.lib",
    "link /nologo /subsystem:windows /entry:mainCRTStartup /out:`"$GameExePath`" $ObjectArgs user32.lib gdi32.lib winmm.lib"
)
foreach ($BuildStep in $BuildSteps) {
    $Command = "call `"$VcVars`" >nul && cd /d `"$Root`" && $BuildStep"
    & cmd.exe /d /c $Command
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Built $ConsoleExePath"
Write-Host "Built $GameExePath"

$ShortcutScript = Join-Path $Root "tools\Update-DesktopShortcut.ps1"
if ((Test-Path $ShortcutScript) -and ($env:TECMO_SKIP_SHORTCUT -ne "1")) {
    & $ShortcutScript -ExePath $GameExePath -ProjectRoot $Root
}
