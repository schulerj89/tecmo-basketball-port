# Tecmo Basketball Port Tooling

This is a local hobby/research scaffold for exploring a native C port workflow for Tecmo NBA Basketball. It contains tooling only.

## Important Boundaries

This repository intentionally does not include:

- ROM files
- PRG/CHR dumps
- reverse-engineered ASM
- lifted decompilation chunks
- extracted graphics, tile sheets, audio, or roster dumps
- generated files derived from the original game

Do not use this project to distribute copyrighted game data, bypass ownership of the original game, sell or repackage proprietary content, or help others obtain assets they do not have the right to use. Use it only with local files you are legally allowed to study.

The tooling expects your private decompilation/asset workspace to live outside this repository.

## Build

PowerShell with Visual Studio C++ tools installed:

```powershell
.\build.ps1
```

The script locates Visual Studio via `vswhere` and builds:

```text
build\tecmo_port.exe
```

## Point It At Your Local Decomp

Pass the private decomp path explicitly:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --summary
```

Or set an environment variable:

```powershell
$env:TECMO_DECOMP_ROOT='<LOCAL_DECOMP_ROOT>'
.\build\tecmo_port.exe --summary
```

## Commands

```powershell
.\build\tecmo_port.exe --summary
.\build\tecmo_port.exe --banks
.\build\tecmo_port.exe --chunks
.\build\tecmo_port.exe --roster CHICAGO
.\build\tecmo_port.exe --assets
.\build\tecmo_port.exe --play
.\build\tecmo_port.exe --render-test build\play_test.png
.\build\tecmo_port.exe --render-test-mode menu docs\screenshots\menu_default.png
.\build\tecmo_port.exe --render-test-mode menu-overlay build\menu_overlay_test.png
.\build\tecmo_port.exe --render-test-mode rosters build\rosters_test.png
.\build\tecmo_port.exe --render-test-mode play build\play_setup_test.png
.\build\tecmo_port.exe --render-test-mode original-title build\original_title_test.png
```

Run every active screenshot test declared in `port_iteration.json`:

```powershell
.\tools\Run-ScreenshotTests.ps1 -Build
```

Pass `-DecompRoot <LOCAL_DECOMP_ROOT>` if the script cannot discover your private local decomp workspace.

Map the local-only source candidates for the first original title/menu screen:

```powershell
.\tools\Find-OriginalScreenSources.ps1
```

Prototype controls:

```text
Main menu:
Up/Down = choose Play Game, Rosters, or Quit
Enter = confirm
Esc = quit
F3 = debug overlay

Play Game:
Left/Right = team
Up/Down = player
Enter = start court prototype
Esc = main menu

Rosters:
Left/Right = team
Up/Down = player
Esc = main menu

Court prototype:
Arrows = move
Space = shoot
Esc = play setup
```

Local-only generated outputs:

```powershell
.\build\tecmo_port.exe --generate-rosters generated
.\build\tecmo_port.exe --export-chr build\tecmo_tiles.chr
.\build\tecmo_port.exe --export-chr-png build\chr_png
```

Those generated outputs are ignored by Git and should stay local.

## Desktop Shortcut

Each successful build refreshes a local Desktop shortcut named:

```text
Tecmo Basketball Native Port.lnk
```

The shortcut points at the latest `build\tecmo_port.exe`, uses a generated original basketball icon from `build\tecmo_port.ico`, and launches `--play`. It will include `--root` automatically when `TECMO_DECOMP_ROOT` is set or the local decomp folder is found next to this repo. Set `TECMO_SKIP_SHORTCUT=1` before running `build.ps1` if you want to skip shortcut generation.

## Current Scope

- Inventory banked ASM files from a private decomp tree
- Count lifted chunks and labels
- Parse local Bank 02 roster labels into C-friendly records
- Export local CHR bytes and grayscale tile sheet PNGs for private inspection
- Run a native Win32 playable prototype with explicit memory arenas and roster-driven team/player selection

The current playable mode is a native prototype, not a full recreation of the original game. It establishes the frame loop, input path, memory model, and data-loading boundary that future translated gameplay systems can plug into.

## Native Runtime Direction

This project is not embedding a NES CPU emulator. The intended path is a native port:

- translate verified routines into portable C modules
- keep proprietary data outside the public repo
- load private/local extracted data only at development time
- replace NES hardware dependencies with explicit platform layers

The current runtime separates:

- `TecmoGameMemory`: permanent/transient arenas plus NES-shaped RAM buffers for ported systems that still need mirrored RAM semantics
- `TecmoRuntime`: game state, roster selection, court prototype state, and deterministic frame update
- `TecmoFramebuffer`: platform-neutral software framebuffer consumed by the Win32 backend
- `win32_platform.c`: temporary Windows window/input/presentation layer

That gives us a place to port mechanics without letting platform code leak into gameplay code.

## Iteration Manifest

Small native-port steps are tracked in:

```text
port_iteration.json
```

That file lists quick launch points, screenshot tests, planned debug overlay fields, memory budgets, and near-term milestones. It is intentionally safe to commit: it must not contain ROM paths, ASM, extracted bytes, generated roster data, or private local paths.

Screenshot tests are driven from this manifest through `tools\Run-ScreenshotTests.ps1`. The runner only accepts the known render-test command shape and only writes PNGs under ignored `build\` output or explicitly safe `docs\screenshots\` files.

The original title/menu investigation is tracked in [docs/original_title_screen_plan.md](docs/original_title_screen_plan.md). Its local mapper writes `build\original_screen_sources.json`, which is ignored because it is generated from the private local decomp workspace.
