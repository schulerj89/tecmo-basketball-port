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

## Current Status

The port currently supports an end-to-end native path from the opening sequence
through a completed preseason or season game. It is not yet a complete or
frame-identical recreation of on-court gameplay.

| Area | Current boundary |
| --- | --- |
| Opening and title | Supported: TECMO/rabbit, NBA license, arena, post-PASS finale, attract continuation, title, and title confirmation |
| Blue start-game menu | Supported: root navigation, settings popups, season-page slide, input repeat/release behavior, fades, and return state |
| Preseason | Supported through team selection, native game launch, completed result, and return to PRESEASON |
| Season | Supported for TEAM CONTROL, schedule/playoffs, standings/programmed results, GAME START, persistent records, and one-time result commit |
| Team Data | Supported for team profiles, rosters, player detail, STARTERS, and PLAYBOOK |
| All Star | Partial: selectors work, but the route stops before game launch |
| League Leaders | Partial: category navigation works; ranked player results remain unavailable until per-player season statistics are ported |
| Gameplay | Playable full-game shell with movement, passing, defender switching, close shots, one verified ordinary-jump miss, fouls/free throws, clocks, periods, halftime, overtime/final, audio, and result handoff; much of the basketball simulation remains approximate |

Normal play is asset-pack-only. It does not load decompilation files, Lua
traces, screenshots, save states, dumps, or emulator captures at runtime.

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

The build creates the executables, but it cannot include proprietary game
data. Before normal play on a fresh checkout, create the ignored local asset
pack from a legally obtained Rev 1 ROM:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_REV1_ROM.nes> .\build\tecmo.assetpack
```

The currently supported ROM is **Tecmo NBA Basketball (USA) (NES-BK)
(Rev 1)** with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
Wrong revisions and malformed inputs fail closed.

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

### Gameplay that works today

- Directions move the owned actor.
- NES A passes on offense and switches defenders on defense.
- NES B starts an offensive shot or attempts the current defensive
  steal/contact action. START and SELECT are inert during live play.
- The close-shot system exposes the ROM's numeric variant 0 dunk family and
  numeric variant 2 layup family. Their live distance trigger and make/miss
  policy are still native approximations.
- The dunk uses the strict ROM-derived TGDK-1 cutaway, including its black
  transitions, screen `$0B`, seven staged sprite groups, return to live play,
  address-bound A9C5 audio checkpoint, and frame-132 settlement.
- Ordinary-jump support is deliberately narrow: only the captured human
  away-side, right-facing family-0/profile-0/direction-1 terminal miss is
  supported. It awards no points and changes possession at frame 87. Generic
  jump makes and other profiles, directions, and outcomes are not implemented.
- The scene advances the game and shot clocks, score, possession, shot-clock
  violations, current native foul/free-throw flow, period banners, halftime,
  overtime/final presentation, and preseason/season result handoff.
- Human free throws launch from the scoring team's current NES B level and
  have no timeout. CPU free throws use the bounded observed 125-update
  schedule; lineup, aim, outcome, rebound, and CPU positioning remain
  approximations.

### ROM-derived versus approximate

Strict ROM-derived data currently covers the static court, CHR and palette
entries, embedded FCEUX RGB profile, actor pose data, numeric close-shot step
tables, dunk cutaway, the one bounded jump-miss route and settlement decision,
rules timing, and native music/SFX/DMC programs. Every required gameplay entry
is loaded from the same revision-fingerprinted asset pack with exact-size and
malformed-data checks.

The actor and camera layout, movement and AI, jump-ball geometry, general shot
selection and make/miss policy, dynamic matchup palettes and uniforms, live
close-shot profile/direction selection, left-facing mirroring, contact/foul
detection, free-throw simulation, rebounds, blocks, steals, per-player game
statistics, and temporary HUD typography remain native approximations or are
unsupported. `gameplay/penalties` TPNL-1 contains strict ROM-backed rule data,
but the live scene's current contact/foul code does not consume it yet.

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
.\tools\Run-GameplayShotResolutionTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayPenaltyTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayDunkCutawayTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

Render the normal menu or a focused intro frame:

```powershell
.\build\tecmo_port.exe --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-start build\gameplay_start_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame12 build\gameplay_jump_12_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-dunk-frame16 build\gameplay_dunk_16_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-dunk-frame64 build\gameplay_dunk_64_test.png
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

Generated `.assetpack` files are ignored local data. Every pack includes the
manifest, sanitized source map, and raw PRG/CHR entries used by the strict
logical assets.

The current Rev 1 builder emits a 74-entry pack. In addition to the raw PRG and
CHR entries, it contains strict logical assets for the opening, arena, finale,
title, blue menu, preseason, Team Data, team management, season state, music,
gameplay audio, court, poses, close shots, dunk presentation, the bounded jump
route, shot-resolution rules, and penalty rules. These entries are derived
directly from the local ROM during pack construction; decompilation files and
captures are not pack inputs.

The normal Desktop launch resolves native assets from `TECMO_ASSETPACK` or the
port's `build\tecmo.assetpack`. Loose decomp fallbacks remain development-only
for explicit console commands.

## Current Scope

The project is actively porting the original game into native C modules. Current work includes:

- a native Win32 runtime and software framebuffer
- a strict Rev 1 ROM-to-asset-pack pipeline for normal play
- legacy private decomp/asset inspection tools for explicit developer commands
- Bank07 fixed-helper C counterparts
- the native opening, title, blue menu, preseason, Team Data, and season paths
- a playable but incomplete native gameplay scene with strict court, pose,
  close-shot, dunk, jump-miss, rules, state, and audio assets
- focused render-test modes for visual regression checks

The public repo remains source-only. Local CHR, OAM, palette, nametable, roster, trace, screenshot, and emulator-capture outputs are generated under ignored paths and should not be committed.

## Native Runtime Direction

This project is not embedding a NES CPU emulator. The intended path is a native port:

- translate verified routines into portable C modules
- keep proprietary data outside the public repo
- build private ROM-derived data into an ignored local asset pack
- keep decompilation files and captures limited to explicit development work
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
