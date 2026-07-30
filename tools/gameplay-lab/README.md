# Tracked Gameplay Laboratory

This is a private developer diagnostic for one tightly bounded Tecmo NBA
Basketball Rev 1 MAN VS MAN jump-shot experiment. It power-on navigates to a
two-player game, watches the live world model, uses coordinate feedback to
clear a front defender and place the ball holder in the already proven shot
window, then performs the captured eight-frame B hold and frame-nine release.
It is not a general gameplay AI.

The driver supplies controller input only. It never changes game RAM, enables
cheats, or loads an emulator state. Both controller tables are complete on
every frame, all aborts neutralize both pads, and the runner closes its FCEUX
process. Do not run it while another FCEUX process is open.

Run the static safety/schema suite:

```powershell
.\tools\gameplay-lab\Test-GameplayLab.ps1
```

Run the private pilot by supplying both local files explicitly:

```powershell
.\tools\gameplay-lab\Run-GameplayLab.ps1 `
  -RomPath <LOCAL_REV1_ROM.nes> `
  -FceuxPath <LOCAL_FCEUX_2.6.6.exe> `
  -RequirePass
```

`TECMO_GAMEPLAY_LAB_ROM_PATH` and `TECMO_GAMEPLAY_LAB_FCEUX_PATH` are supported
as clearly named alternatives. The runner revision-locks both binaries, rejects
concurrent FCEUX, runs hidden with redirected logs by default, and imposes
frame, wall-clock, row, screenshot, and tracked-text caps. While FCEUX runs, the
runner polls the entire session—including screenshots, optional FM2, and
redirected logs—and terminates it above 64 MiB. Outputs stay beneath the ignored
`temp-videos/gameplay-lab/<timestamp>` directory. Add `-RecordMovie` only when
an ignored FM2 is useful. A metadata/status startup sentinel must appear within
five seconds, so a Lua load error cannot leave hidden FCEUX waiting for the
full wall-clock timeout.

`gameplay_lab.lua` protects each non-yielding frame step and uses an
idempotent emergency finalizer; FCEUX's yielding `frameadvance` remains outside
that protected call because Lua 5.1 cannot yield across `xpcall`.
The runner therefore also enforces a five-second progress watchdog. The core
loop stays top-level to avoid Lua 5.1's function-upvalue limit. The session contains
compact status and phase files, per-frame actor/ball
telemetry, Bank05 outcome hooks, focused shot detail, at most eight
screenshots, and optional FM2. `tecmo_rev1_map.lua` is the only canonical
address/hook map. Its current schema is TGLM-2, and the matching output schema
is TGLAB-2. The native C runtime does not read any laboratory output.

Per-actor telemetry and focused shot detail preserve three distinct raw
16-bit velocity words without signed interpretation: altitude velocity uses
the actor arrays at `$049A/$04A5`, horizontal velocity uses `$04E7/$04F2`,
and vertical velocity uses `$04FD/$0508`. These names describe how the
currently traced ROM routines use the arrays; the CSV values remain raw
four-digit hexadecimal evidence.

Missed-shot telemetry includes mapper-gated Bank05 hooks at `$A6EE`, `$A708`,
`$A7A9`, and `$A8E9`. Each queued hook event snapshots raw `$006A`, its low
two-bit selector, the selected target address (`0/3 -> $A708`, `1 -> $A7A9`,
`2 -> $A8E9`), object-slot-10 state `$0478`, direct object-slot-10 horizontal
and vertical velocity, and the saved object velocity scratch words
`$038D/$038E` and `$038F/$0390` at hook time. The callback queues those exact
raw values so later event flushing cannot replace them with newer RAM. The
saved fields are scratch storage: at the `$A7A9` entry hook they can predate
the routine's `JSR $A790`, so they are not assumed to have been saved by that
same route invocation. These fields identify numeric initial miss routes only;
they do not assign rebound, rim, bounce, or signed-direction meaning. The
existing event-row and tracked-text caps still apply.

Current limits are intentional: Rev 1 and FCEUX 2.6.6 only; period 1;
orientation 0; offense side 0; distinct MAN VS MAN teams; ordinary, non-close
shot only; and the proven coordinate window `x=$0164..$0170`,
`y=$6C..$74`. Mirrored movement, the defensive A-cycle order, general shot
selection, close routes, fouls, violations, and arbitrary possession recovery
are not inferred. An unsupported or unstable context aborts with neutral pads.
The bot's front-threat policy uses a conservative `20x12` window, wider than
the original strict contact box (`abs(dx)<12`, `abs(dy)<7`). If an identified
threat cannot be selected through observed defensive A edges, the experiment
aborts instead of pretending the player can be controlled.
