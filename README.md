# Tecmo Basketball Native Port

A work-in-progress native C port of **Tecmo NBA Basketball** for Windows. It
recreates the game without embedding a NES emulator.

The port is playable from the opening sequence through completed preseason and
season games. Some on-court systems still use native approximations while the
original game logic is being reconstructed.

## Current status

- Original-style opening, title screen, menus, music, sound effects, and sampled
  audio
- Preseason and season flows, including team management, schedules, standings,
  persistent results, and completed-game return paths
- Full scrolling court with ten players, possession, passing, shooting, dunks,
  layups, free throws, fatigue, turnovers, clocks, scoring, periods, and overtime
- Interactive opening tip with timed player and CPU jumps, ROM-derived poses and
  trajectory timing, receiver-directed ball flight, and continuous movement into
  live play
- Possession-aware offensive control and defender handoff, with persistent CPU
  spacing commands that keep defenders matched more closely to their assignments
- Ordinary jump shots use the ROM's family-, player-, and direction-specific
  animation sequences through the gather, release, flight, and recovery phases
- ROM-derived presentation, movement, animation, court layouts, palettes, and
  uniforms
- Team Data, rosters, starters, playbooks, and player details

The largest remaining gaps are exact CPU offensive play selection and full-team
spacing; steals, blocks, rebounds, contact, and fouls; several shot outcomes;
per-player statistics; All-Star game launch; and populated League Leaders.

## Controls

| Action | Player 1 | Player 2 |
| --- | --- | --- |
| Move | Arrow keys | Numpad 8/2/4/6 |
| Pass / switch defender (NES A) | Z | Numpad 1 |
| Shoot / defensive action (NES B) | X | Numpad 3 |
| START | Enter | Numpad 9 |
| SELECT | Shift or Space | Numpad 7 |

Hold NES B during the visible opening jump contest to contest the tip. `F3`
toggles the development overlay. While a strict court scene is active, the
developer-only violation presentation lab is available only with the overlay
on: `F4` opens or closes it, `F5`/`F6` select the seven TGVR messages plus the
separate fixed foul script, `F7` switches the explicitly labelled source or
supported production-state preview, `F8` plays/pauses, `F9` restarts, and
`F10` advances one frame. The lab freezes the court and restores it when it
closes; its production-state preview is not a claim that a gameplay detector
fired. State preview is available only from an idle LIVE scene (not pre-tip,
an active shot/cutaway, or a free-throw lineup); source preview remains
available there. `F3` off makes all lab keys inert.

The F3 court overlay also exposes passive typed CPU diagnostics for idle
dribbling: holder/primary/defender, a retained source command offset/opcode,
target kind/validity, and the current shot-request supported/deferred flags.
It does not invoke the CPU steering console harness, invent raw RAM mirrors,
or report an unavailable first source-gate reason.

## Build and run

The build requires PowerShell and the Visual Studio C++ tools:

```powershell
.\build.ps1
```

This creates a windowed game at `build\tecmo_port_game.exe` and a console build
with development commands at `build\tecmo_port.exe`.

The repository does not contain proprietary game data. On a fresh checkout,
build the ignored local asset pack from a legally obtained **Tecmo NBA
Basketball (USA) (NES-BK) (Rev 1)** ROM:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_REV1_ROM.nes> .\build\tecmo.assetpack
.\build\tecmo_port_game.exe --root . --play
```

Supported ROM SHA-256:

```text
076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4
```

Each successful build also refreshes the desktop shortcut named
`Tecmo Basketball Native Port.lnk`. Set `TECMO_SKIP_SHORTCUT=1` before running
`build.ps1` to skip shortcut creation.

## Development

Focused verification scripts live under `tools\`. Useful starting points are:

```powershell
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_REV1_ROM.nes>
.\tools\Run-NativeFlowTests.ps1 -Build
.\tools\Run-Win32LaunchSmokeTest.ps1 -Build
```

See [PORTING.md](PORTING.md) for porting direction and [AGENTS.md](AGENTS.md)
for detailed development notes and verification commands.

## Repository boundaries

This public repository contains source, build scripts, tests, and documentation
only. Do not commit or distribute ROMs, PRG/CHR dumps, extracted graphics or
audio, rosters, save states, captures, generated asset packs, or other
copyrighted game data.
