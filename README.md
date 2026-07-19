# Tecmo Basketball Native Port

This repository is a native C port workspace for Tecmo NBA Basketball.

The port is built from verified behavior observed in a private local decompilation/reference environment. This public repo contains port source, build scripts, tests, and local tooling only. Original game data and generated outputs stay outside the repo or under ignored local build paths.

## Boundaries

This repository intentionally does not include:

- ROM files
- PRG/CHR dumps
- reverse-engineered ASM
- lifted decompilation chunks
- extracted graphics, tile sheets, audio, or roster dumps
- generated files derived from the original game

Do not use this project to distribute copyrighted game data, bypass ownership of the original game, sell or repackage proprietary content, or help others obtain assets they do not have the right to use. Use it only with local files you are legally allowed to study.

The tooling expects any private decompilation or asset workspace to live outside this repository.

## Build

PowerShell with Visual Studio C++ tools installed:

```powershell
.\build.ps1
```

The script locates Visual Studio via `vswhere` and builds:

```text
build\tecmo_port_game.exe  GUI game launch (no terminal window)
build\tecmo_port.exe       console CLI and development tools
```

Each successful build also refreshes a local Desktop shortcut named:

```text
Tecmo Basketball Native Port.lnk
```

Set `TECMO_SKIP_SHORTCUT=1` before running `build.ps1` to skip shortcut generation.

## Run

Launch the current native port without a terminal window:

```powershell
.\build\tecmo_port_game.exe --root . --play
```

The generated Desktop shortcut uses the absolute port project root in the same
command. Normal play loads the strict ROM-derived `build\tecmo.assetpack` and
does not require loose roster files from a decompilation checkout.

The console build exposes the same windowed play path plus CLI diagnostics:

```powershell
.\build\tecmo_port.exe --root . --play
```

Pass a private decomp path only for explicit console development commands that
still inspect loose reference data:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --roster CHICAGO
```

Or set an environment variable for those developer commands:

```powershell
$env:TECMO_DECOMP_ROOT='<LOCAL_DECOMP_ROOT>'
.\build\tecmo_port.exe --flow-test
```

## Native Play

The game executable and Desktop shortcut boot directly into the original
TECMO/rabbit opening. The temporary Play Game/Quit screen remains available
only to console flow tests and explicit debug/render paths.

Controls:

```text
Player 1: arrows = directions, Space = NES A, Esc = NES B,
Enter = START, Tab = SELECT
Player 2: numpad 8/2/4/6 = directions, numpad 1 = NES A,
numpad 3 = NES B, numpad 9 = START, numpad 7 = SELECT
F3 = debug overlay
```

The current original-game boundary includes the blue start-game menu,
PRESEASON through both team selectors, the ALL STAR selectors, TEAM DATA's
profile/roster/player-detail/STARTERS/PLAYBOOK flows, and native season
management through TEAM CONTROL, SCHEDULE/PLAYOFF, STANDINGS/PROGRAMMED,
LEADERS category navigation, and GAME START. Preseason final team confirmation
and a prepared season GAME START now launch the native gameplay scene. A final
preseason result returns to the blue-menu PRESEASON row; a season result is
validated and committed exactly once before returning to the season result
rows. ALL STAR still ends at its documented prelaunch boundary, and League
Leaders does not fabricate ranked player results until per-player season
accumulators are ported.

During gameplay, directions move the owned actor, NES A passes on offense or
switches defenders, and NES B starts a shot on offense or attempts a defensive
steal/contact action. START and SELECT are inert. The current scene includes
ordinary jump shots, the ROM's dunk (numeric variant 0) and layup (numeric
variant 2) close-shot families, score and
possession changes, shot-clock violations, fouls/free throws, period banners,
halftime, overtime/final handling, crowd/gameplay audio, and result handoff.
Static court/CHR/palette assets, the embedded FCEUX RGB profile, numeric
close-shot step tables, rules timing, and audio programs are ROM-derived. Actor
layout, movement/AI, ordinary-jump timing, shot physics/results, dynamic
team/court palette selection, live close-shot profile/direction choice and
left-facing mirroring, contact/foul detection, free-throw lineup, aim, results,
rebounds, CPU positioning/script behavior, and the temporary HUD typography
remain native approximations. Human free throws already use the scoring team's
current NES B level with no timeout; unassigned CPU sides use the bounded
observed 125-update launch schedule. The strict pose asset
stores 208 exact TGCS profile/direction resolutions into TGPL pose data; live
play currently selects only profile 0/direction 0.

Opening music plays from the strict ROM-derived semantic music asset. GAME
MUSIC gates gameplay track 5 and the evidence-bounded restart cue; halftime and
final presentation request track 6. Crowd, violation, clock/countdown, and
held-ball/dribble events use strict ROM-derived TSFX-1/TDMC-1 assets. GAME SPEED
remains a stored gameplay setting and does not change menu or soundtrack tempo.
The visible `SIC` left beside the speed popup is an authentic overlap from the
original menu.

Older diagnostic screens and the modern Play Game/Quit menu remain available
through explicit render-test/debug paths for development work.

## Common Commands

```powershell
.\build\tecmo_port.exe --summary
.\build\tecmo_port.exe --banks
.\build\tecmo_port.exe --chunks
.\build\tecmo_port.exe --assets
.\build\tecmo_port.exe --roster CHICAGO
.\build\tecmo_port.exe --flow-test
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --gameplay-state-test
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

Render the normal menu or a focused intro frame:

```powershell
.\build\tecmo_port.exe --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-start build\gameplay_start_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame12 build\gameplay_jump_12_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-dunk-frame16 build\gameplay_dunk_16_test.png
```

The former `gameplay-close-shot-frameN` spelling remains a compatibility alias
for the canonical dunk checkpoint.

Run every active screenshot test declared in `port_iteration.json`:

```powershell
.\tools\Run-ScreenshotTests.ps1 -Build
```

Run every active native flow test declared in `port_iteration.json`:

```powershell
.\tools\Run-NativeFlowTests.ps1 -Build
```

Pass `-DecompRoot <LOCAL_DECOMP_ROOT>` if a helper script cannot discover your private local decomp workspace.

Verify the GUI/console subsystem split and the complete generated-shortcut
contract without requiring a decompilation checkout:

```powershell
.\tools\Run-Win32LaunchSmokeTest.ps1 -Build
```

Add `-DecompRoot <LOCAL_DECOMP_ROOT>` to that command to also exercise the
explicit console developer flow; the GUI smoke launch still uses the port root.

Build a private local asset pack from a local iNES image:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

Generated `.assetpack` files are ignored local data. The default builder writes `system/manifest`, `system/source-map`, raw `prg/*` entries, raw `chr/*` entries, and reserves logical namespaces for later decomp-derived entries.

The normal Desktop launch resolves native assets from `TECMO_ASSETPACK` or the
port's `build\tecmo.assetpack`. Loose decomp fallbacks remain development-only
for explicit console commands.

## Current Scope

The project is actively porting the original game into native C modules. Current work includes:

- a native Win32 runtime and software framebuffer
- local private decomp/asset discovery tools
- roster parsing from private local labels
- Bank07 fixed-helper C counterparts
- the opening sequence path, including the TECMO/rabbit intro, NBA license screen, and arena transition
- native preseason/season gameplay with strict court, pose, state, and audio assets
- focused render-test modes for visual regression checks

The public repo remains source-only. Local CHR, OAM, palette, nametable, roster, trace, screenshot, and emulator-capture outputs are generated under ignored paths and should not be committed.

## Native Runtime Direction

This project is not embedding a NES CPU emulator. The intended path is a native port:

- translate verified routines into portable C modules
- keep proprietary data outside the public repo
- load private/local extracted data only at development time
- replace NES hardware dependencies with explicit platform layers

Lower-level runtime and memory notes are kept in [AGENTS.md](AGENTS.md) for development agents.

## Local Generated Outputs

These commands write ignored local outputs:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
.\build\tecmo_port.exe --generate-rosters generated
.\build\tecmo_port.exe --export-chr build\tecmo_tiles.chr
.\build\tecmo_port.exe --export-chr-png build\chr_png
```

Those files are for private inspection only and should stay local.

## Development Notes

Agent/debug workflow notes live in [AGENTS.md](AGENTS.md). That file covers hidden diagnostic render modes, Lua watcher captures, large-log handling, and local-only trace files.

Longer investigation notes live under `docs/`. Some documents describe historical diagnostic screens and probes that are no longer visible from the main executable menu.
