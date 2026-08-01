# Tecmo Basketball Native Port

This is a work-in-progress native C port of **Tecmo NBA Basketball** for
Windows. It recreates the game as native code rather than embedding a NES
emulator.

The port is playable from the opening sequence through a completed preseason
or season game. It is still under active development, so some on-court systems
use native approximations instead of the original game logic.

## What is currently in the game

### Menus and game modes

- TECMO/rabbit opening, NBA license screen, arena sequence, title screen, and
  blue start-game menu
- PRESEASON team selection, game launch, completed result, and return to the
  menu
- SEASON team control, schedule/playoffs, standings, programmed results, game
  launch, and persistent game results
- TEAM DATA profiles, rosters, player details, STARTERS, and PLAYBOOK
- ALL STAR team selectors; the All-Star game launch is not implemented yet
- LEAGUE LEADERS category navigation; ranked results await per-player season
  statistics

### On-court gameplay

- Original-style matchup, `1ST PERIOD`, referee/player close-up, ROM-positioned
  center-court tip, and live-game handoff
- Full 768-pixel court with horizontal camera movement
- Ten court players, ball, hoops, possession changes, and camera projection
- ROM-derived human locomotion plus TGAI-directed/TGMO-driven ordinary CPU
  movement, passing, and defender switching
- ROM-derived walking poses and held-ball bounce animation, fatigue
  decay/bench recovery, plus out-of-bounds and backcourt turnover settlement
  with the original referee-pointing sequences
- Dunks, layups, orientation-aware jump shots with entry, turn, release, and
  airborne poses toward either basket, plus free-throw sequences
- Game clock, shot clock, scoring, periods, halftime, overtime, final results,
  and return to the selected game mode
- Original screen, lettering, palette fade, and selector-specific referee
  animation for shot-clock, out-of-bounds, and backcourt presentations
- Original game font for team names, scores, clock, jersey numbers, and selected
  players in the live HUD
- ROM-derived matchup-specific court, player, ball, and uniform colors
- Original music, sound effects, crowd responses, and sampled audio through the
  native audio runtime

## Still in progress

The port is playable, but it is not yet a frame-identical recreation. The main
remaining gameplay work includes:

- Original CPU play-command selection, dynamic link assignments, and spacing
- Complete ROM shot selection and the remaining jump-shot families and outcomes
- Exact steals, blocks, rebounds, contact, fouls, and free-throw outcomes
- Exact post-tip live spacing, starting lineups, and some possession interactions
- Per-player game and season statistics
- All-Star game launch and populated League Leaders rankings

Human movement, the tip-off formation, and several presentation systems are
ROM-derived. Ordinary CPU actors now use the exact ROM octant quantizer and
movement kernel, but their hoop/matchup target policy and shot timing remain
native approximations until the original play-command lifecycle is ported.

## Controls

| Action | Player 1 | Player 2 |
| --- | --- | --- |
| Move | Arrow keys | Numpad 8/2/4/6 |
| NES A: pass / switch defender | Z | Numpad 1 |
| NES B: shoot / defensive action | X | Numpad 3 |
| START | Enter | Numpad 9 |
| SELECT | Shift or Space | Numpad 7 |

`F3` toggles the development overlay.

## Build

The build requires PowerShell and the Visual Studio C++ tools:

```powershell
.\build.ps1
```

This produces:

```text
build\tecmo_port_game.exe  Windowed game without a console
build\tecmo_port.exe       Console build and development commands
```

The source repository does not contain proprietary game data. On a fresh
checkout, generate the ignored local asset pack from a legally obtained
**Tecmo NBA Basketball (USA) (NES-BK) (Rev 1)** ROM:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_REV1_ROM.nes> .\build\tecmo.assetpack
```

Supported ROM SHA-256:

```text
076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4
```

Wrong revisions, malformed assets, and stale asset packs are rejected instead
of being loaded loosely.

## Run

```powershell
.\build\tecmo_port_game.exe --root . --play
```

Every successful build also refreshes the desktop shortcut named
`Tecmo Basketball Native Port.lnk`. Set `TECMO_SKIP_SHORTCUT=1` before running
`build.ps1` if shortcut generation is not wanted.

Normal play reads `build\tecmo.assetpack`; it does not load decompilation
files, screenshots, traces, save states, or emulator captures.

## Development and verification

The focused gameplay and native-flow checks are:

```powershell
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_REV1_ROM.nes>
.\tools\Run-GameplayBackcourtTests.ps1 -Build -RomPath <LOCAL_REV1_ROM.nes>
.\tools\Run-NativeFlowTests.ps1 -Build
.\tools\Run-Win32LaunchSmokeTest.ps1 -Build
```

Additional focused test scripts live under `tools\`.

For implementation details, exact ROM-backed boundaries, and the remaining
approximate systems, see:

- [PORTING.md](PORTING.md)
- [Gameplay state foundation](docs/gameplay-state-foundation.md)
- [CPU steering evidence](docs/gameplay-cpu-steering.md)
- [Agent and development notes](AGENTS.md)

## Repository boundaries

This public repository contains source, build scripts, tests, and documentation
only. Do not commit or distribute ROMs, PRG/CHR dumps, extracted graphics,
audio, rosters, save states, captures, generated asset packs, or other
copyrighted game data. Local generated files belong under ignored build or
evidence paths.
