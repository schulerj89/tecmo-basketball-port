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
build\tecmo_port.exe
```

Each successful build also refreshes a local Desktop shortcut named:

```text
Tecmo Basketball Native Port.lnk
```

Set `TECMO_SKIP_SHORTCUT=1` before running `build.ps1` to skip shortcut generation.

## Run

Launch the current native port:

```powershell
.\build\tecmo_port.exe --play
```

Pass the private decomp path explicitly when needed:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --play
```

Or set an environment variable:

```powershell
$env:TECMO_DECOMP_ROOT='<LOCAL_DECOMP_ROOT>'
.\build\tecmo_port.exe --play
```

## Main Menu

The normal executable menu is intentionally simple:

```text
Play Game
Quit
```

Controls:

```text
Boot title:
Enter = main menu
Esc = quit

Main menu:
Up/Down = choose Play Game or Quit
Enter = confirm
Esc = quit
F3 = debug overlay

Play Game:
Runs the current native port sequence
Player 1: arrows = directions, Space = NES A, Esc = NES B,
Enter = START, Tab = SELECT
Player 2: numpad 8/2/4/6 = directions, numpad 1 = NES A,
numpad 3 = NES B, numpad 9 = START, numpad 7 = SELECT
```

The current original-game boundary includes the blue start-game menu and the
PRESEASON path through both team selectors. In MAN VS MAN, controller 2 owns
the second division and team selection. Confirming the second team is
intentionally terminal for now and does not launch a game.

Older diagnostic screens such as Title Screen, Intro Lab, CHR Playground, and Rosters are no longer exposed from the main executable menu. They remain available through explicit render-test/debug paths for development work.

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
```

Render the normal menu or a focused intro frame:

```powershell
.\build\tecmo_port.exe --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
```

Run every active screenshot test declared in `port_iteration.json`:

```powershell
.\tools\Run-ScreenshotTests.ps1 -Build
```

Run every active native flow test declared in `port_iteration.json`:

```powershell
.\tools\Run-NativeFlowTests.ps1 -Build
```

Pass `-DecompRoot <LOCAL_DECOMP_ROOT>` if a helper script cannot discover your private local decomp workspace.

Build a private local asset pack from a local iNES image:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

Generated `.assetpack` files are ignored local data. The default builder writes `system/manifest`, `system/source-map`, raw `prg/*` entries, raw `chr/*` entries, and reserves logical namespaces for later decomp-derived entries.

The runtime prefers `TECMO_ASSETPACK` or `build\tecmo.assetpack` for CHR data, then falls back to the older local `build\baseline\Tiles.asm` path.

## Current Scope

The project is actively porting the original game into native C modules. Current work includes:

- a native Win32 runtime and software framebuffer
- local private decomp/asset discovery tools
- roster parsing from private local labels
- Bank07 fixed-helper C counterparts
- the opening sequence path, including the TECMO/rabbit intro, NBA license screen, and arena transition
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
