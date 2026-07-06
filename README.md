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
.\build\tecmo_port.exe --flow-test
.\build\tecmo_port.exe --render-test build\play_test.png
.\build\tecmo_port.exe --render-test-mode boot-title build\boot_title_test.png
.\build\tecmo_port.exe --render-test-mode menu docs\screenshots\menu_default.png
.\build\tecmo_port.exe --render-test-mode menu-overlay build\menu_overlay_test.png
.\build\tecmo_port.exe --render-test-mode title-screen build\title_screen_runtime_test.png
.\build\tecmo_port.exe --render-test-mode intro-presents build\intro_presents_test.png
.\build\tecmo_port.exe --render-test-mode intro-builder-sample build\intro_builder_sample_test.png
.\build\tecmo_port.exe --render-test-mode intro-rabbit-preset build\intro_rabbit_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-tecmo-preset build\intro_tecmo_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-c051-d861-model build\intro_c051_d861_model_test.png
.\build\tecmo_port.exe --render-test-mode intro-presents-table1 build\intro_presents_table1_test.png
.\build\tecmo_port.exe --render-test-mode chr-playground build\chr_playground_test.png
.\build\tecmo_port.exe --render-test-mode chr-playground-table1 build\chr_playground_table1_test.png
.\build\tecmo_port.exe --render-test-mode rosters build\rosters_test.png
.\build\tecmo_port.exe --render-test-mode play build\play_setup_test.png
.\build\tecmo_port.exe --render-test-mode original-title build\original_title_test.png
.\build\tecmo_port.exe --render-test-mode original-title-chr build\original_title_chr_test.png
```

Run every active screenshot test declared in `port_iteration.json`:

```powershell
.\tools\Run-ScreenshotTests.ps1 -Build
```

Pass `-DecompRoot <LOCAL_DECOMP_ROOT>` if the script cannot discover your private local decomp workspace.

Run every active native flow test declared in `port_iteration.json`:

```powershell
.\tools\Run-NativeFlowTests.ps1 -Build
```

The flow runner writes ignored `build\native_flow_test_report.json` with sanitized pass/fail metadata only.

Run the headless native flow test for title -> menu -> rosters -> play setup -> court -> quit:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
```

The `intro-c051-d861-model` render mode runs a synthetic C helper self-test before writing its PNG, then shows the staged byte rows without decoded payload data.

Detect a local rebuilt NES ROM and emulator candidate for original intro comparison:

```powershell
.\tools\Find-NesReferenceIntro.ps1
```

That writes ignored `build\nes_reference_intro_check.json` with sanitized availability facts only. Add `-Launch` when you want to manually open the local ROM in the discovered emulator.

Map the local-only source candidates for the first original title/menu screen:

```powershell
.\tools\Find-OriginalScreenSources.ps1
```

Resolve the local-only title text to glyph/tile mapping path and write an ignored JSON/probe:

```powershell
.\tools\Find-TitleChrMapping.ps1
```

Map the local-only title setup ranges, helper calls, fixed-helper aggregate counts, fixed-bank vector counts, palette/PPU probe counts, write targets, stream format/effect summary, and adjacent table references:

```powershell
.\tools\Find-TitleSetupMapping.ps1
```

Create an ignored local-only draft file for Intro Lab bank/table/tile/canvas picks. The running Intro Lab can also write this file with `S` after records are added:

```powershell
.\tools\New-IntroLayoutDraft.ps1 -Bank 31 -Table 1
```

Scan the private decomp for safe intro procedure counts and `$C051` helper leads:

```powershell
.\tools\Find-IntroProcedureMapping.ps1
```

Decode the local-only Bank 04 rabbit stream selector into the fixed `$C051/$D861` sprite-record model:

```powershell
.\tools\Find-IntroRabbitLookup.ps1
```

Decode the local-only Bank 04 TECMO logo selector into the same fixed helper model:

```powershell
.\tools\Find-IntroTecmoLogoLookup.ps1
```

Decode the local-only Bank 04 composite intro streams and render a private CHR preview of the rabbit/TECMO pointer leads:

```powershell
.\tools\Find-IntroCompositeTrace.ps1
.\tools\Find-IntroCompositeTrace.ps1 -ChrBank 31
```

That writes ignored `build\intro_composite_trace.json` and `build\intro_composite_trace_preview.png`; do not commit those generated files.

When that ignored trace JSON is present, the runtime Title Screen loads it locally and renders a first rabbit plus asset-backed `TECMO` splash. The visible `TECMO` word uses the Bank 31/table 1 `$180-$193` construction; the rabbit is drawn from the decoded `$A7DB` selector `$01` trace. Palette, live CHR bank, and exact scroll/base state are still under investigation.

Prototype controls:

```text
Boot title:
Enter = launcher menu
Esc = quit

Main menu:
Up/Down = choose Title Screen, Intro Lab, CHR Playground, Play Prototype, Rosters, or Quit
Enter = confirm
Esc = quit
F3 = debug overlay

Title Screen:
Enter = launcher menu
Esc = quit

Intro Lab:
Q/E = switch CHR bank
T = switch CHR pattern table half
Tab = switch focus between source sheet and canvas
Arrows = move focused source tile or canvas cell
Space = record selected tile at selected canvas cell
R = record the Bank 31 table 1 rabbit lookup head/upper candidate as 8x16 sprite pairs 124-12B
M = record the visual TECMO logo candidate tiles 180-193
C = record the current composite candidate with rabbit plus TECMO
Backspace/Delete = remove last placement record
S = save ignored local placement JSON
Enter/Esc = launcher menu

CHR Playground:
Left/Right = switch CHR bank
Up/Down = switch CHR pattern table half
Tab = next CHR bank
Enter/Esc = launcher menu

Play Prototype:
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

The shortcut points at the latest `build\tecmo_port.exe`, regenerates an original TNB basketball icon at `build\tecmo_port.ico`, and launches `--play`. It will include `--root` automatically when `TECMO_DECOMP_ROOT` is set or the local decomp folder is found next to this repo. Set `TECMO_SKIP_SHORTCUT=1` before running `build.ps1` if you want to skip shortcut generation.

## Current Scope

- Inventory banked ASM files from a private decomp tree
- Count lifted chunks and labels
- Parse local Bank 02 roster labels into C-friendly records
- Export local CHR bytes and grayscale tile sheet PNGs for private inspection
- Run a native Win32 playable prototype with explicit memory arenas, source-backed title/CHR diagnostics, bank/table-switchable intro/CHR labs, and roster-driven team/player selection

The current playable mode is a native prototype, not a full recreation of the original game. It establishes the frame loop, input path, memory model, and data-loading boundary that future translated gameplay systems can plug into. The `original-title-chr` render test also loads a native title setup summary from the private local Bank 04 and fixed-bank baselines so setup helper/write/table/stream/staging counts, fixed-helper aggregate categories, fixed-bank vector counts, and palette/PPU probe counts can be verified without committing setup streams, palette values, helper code, or graphics.

For original intro comparison, `tools\Find-NesReferenceIntro.ps1` can find the local rebuilt `.nes` and an installed emulator such as Mesen or FCEUX, then write an ignored sanitized report. It does not make the runtime depend on an emulator.

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

The original title/menu investigation is tracked in [docs/original_title_screen_plan.md](docs/original_title_screen_plan.md). Its local mappers write `build\original_screen_sources.json`, `build\title_chr_mapping.json`, `build\title_mapped_chr_probe.png`, and `build\title_setup_mapping.json`, which are ignored because they are generated from the private local decomp workspace.
