# Tracked Gameplay Laboratory

This is a private developer diagnostic with two closed Tecmo NBA Basketball
Rev 1 MAN VS MAN jump-shot profiles. It power-on navigates to a two-player
game, watches the live world model, uses coordinate feedback to clear a front
defender and place the ball holder in the selected proven window, then performs
the captured eight-frame B hold and frame-nine release. It is not a general
gameplay AI.

## Separate CPU lifecycle proof surface

`tecmo_cpu_lifecycle.lua`, `tecmo_cpu_lifecycle_rev1_map.lua`, and
`Run-GameplayCpuLifecycleProof.ps1` are a separate, read-only CPU lifecycle
surface. They do not alter either closed shot profile or reuse its acceptance
criteria. The CPU map is locked to the Rev1 ROM SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` and the
FCEUX SHA-256
`F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`.

Its controller-only setup first proves the MAN VS MAN gameplay setup, then
executes the authentic tip schedule (P1 A at ages 30-34, A+B at 35-37, B at
38-55) and waits for the clock to demonstrably leave its stopped state before
starting the 120-frame CPU capture. The driver writes complete neutral P1/P2
pads every frame and never writes RAM, loads cheats/savestates, or patches the
ROM. FCEUX `gui.savescreenshotas` reference PNGs are contractually 256x224,
with 3x4 sheets at 768x896; the original AVI/video contract remains separately
declared as 256x240.

The source-pinned dispatch evidence uses canonical ROM addresses `$8B90`
(fetch), `$8B9F` (fixed reader call), `$8BA2` (copied opcode), and `$8BAE`
(indirect dispatch). `$8BB1/$8BC9` are static handler-table data anchors, not
runtime execution hooks; `$8BE1` is opcode-22's handler. The capture fails
closed unless the window contains fetch, copied-opcode, dispatch, handler, and
advance observations, aligned in-range stream offsets, and the exact fixed
link bytes. Actor slot 10 has no `$06CB` fixed-link entry and is emitted as
`NA` in the actor CSV.

The native half runs the warning-clean CPU focused test and deterministic
`gameplay-cpu-steering-frameN` renders. Those frames are production continuity
and regression evidence only: the current scene still uses the accepted native
harness/formation approximation and does not consume the isolated lifecycle
engine. Normal-flow integration remains `R1-LIVE`.

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

The default `three_point_baseline` profile retains the original window and
pass behavior. The only second profile is selected explicitly:

```powershell
.\tools\gameplay-lab\Run-GameplayLab.ps1 `
  -Profile ordinary_two_point_make `
  -RomPath <LOCAL_REV1_ROM.nes> `
  -FceuxPath <LOCAL_FCEUX_2.6.6.exe>
```

That profile is fixed to `x=$0108..$010F`, `y=$6C..$74` and passes only with
ordinary `$8C57` release, no `$8C7D` close launch, mapper-aware `$B995/$B9D7`
point evidence equal to 2, terminal MAKE, and score delta 2. Arbitrary profile
names, coordinates, delays, outcomes, or score deltas are not exposed.

`TECMO_GAMEPLAY_LAB_ROM_PATH` and `TECMO_GAMEPLAY_LAB_FCEUX_PATH` are supported
as clearly named alternatives. The runner revision-locks both binaries, rejects
concurrent FCEUX, runs hidden with redirected logs by default, and imposes
frame, wall-clock, row, screenshot, and tracked-text caps. While FCEUX runs, the
runner polls the entire session -- including screenshots, optional FM2, and
redirected logs -- and terminates it above 64 MiB. Outputs stay beneath the ignored
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
address/hook map. Its current schema is TGLM-4, and the matching output schema
is TGLAB-4. The native C runtime does not read any laboratory output.

Point telemetry snapshots RAM `$0398` in the bounded hook queue. Mapper-gated
Bank05 `$B995` and `$B9D7` events prove classifier entry and the orientation-0
two-point return; status records the selected closed profile, point evidence,
expected point value, and observed value.

TGLM-4 replaces blind defensive A retries with a confirmed transaction.
Bank06 `$91CB` is a pre-store hook: the queued candidate must equal `$0309` on
the next frame. The driver records the current cycle's unique selected
defenders, succeeds when the front threat is selected, and recognizes a closed
cycle only after seeing a different actor and returning to the origin. Six
confirmed stores across the entire pilot is the hard cap. When the threat is
absent from that closed cycle, only
`ordinary_two_point_make` may neutralize both pads, emit one offense-A pulse,
wait at most 90 frames for a different `$0308` holder, and rebuild the holder,
ball, side, live-route, and score proof for eight stable frames. Repeated
holders and more than four successful transfers abort.

Controlled movement holds one cardinal input instead of alternating direction
and neutral every frame. Right/Left/Down/Up admit only actor state
`1/2/4/8` respectively (or state 0), controller readback must be neutral before
a direction or controlled port changes, and the 90-frame progress deadline is
reset only when the relevant coordinate metric improves.

The timing event rows snapshot mapper select/register state, hook order/frame
and CPU registers, target `$0094-$0097`, object-slot-10 count, altitude and
velocities, state-08 count, selected actors/sides, and both scores inside the
callback. Mapper-aware hooks cover the `$8C57/$8C78` direction remap,
`$AD4E/$AD50`, generic target-motion helper `$B32C`, `$AD68`,
`$B100/$B139/$B13E`, `$AB73/$AB36`,
`$BA02/$BA19`, `$AC0A/$AC6A`, and the `$8FB9/$9042` actual possession
swap. TGLAB-4 status reports cycle/pass/holder counts, target/slot evidence,
`$B100` entry count, and score/handoff deltas. The two-point profile cannot
pass without a complete ordered timing record; the default three-point
baseline does not require this new evidence.

The exact pending two-point route orders `$B995->$B9D7` point value 2 and
`$91BC->$933B->$942D` MAKE before target flight. It requires raw shooter
direction `$05` at `$8C57`, direction `$00` at `$8C78` and again at
`$AD4E/$B32C`, phase low nibble `$05`, close mode `$00`, target
`$00A0/$008F`, 16-bit slot-10 count `$003C`, slot position shooter `+(2,-1)`,
altitude `$3900`, altitude velocity `$04EC`, raw H/V velocity bounds
`$FF88..$FF8F`/`$001D..$0026`, exactly 63 `$B100` entries, one score delta of
two, exactly 26 `$AC0A` state-08 updates, and SFX mailbox `$0B` throughout the
same-frame `$8FAD->$8FB9->$9042` swap. Native C enum direction 1 is not
treated as raw `$0463==1`.

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
shot only. The default window is `x=$0164..$0170`, `y=$6C..$74`; the closed
two-point profile uses `x=$0108..$010F`, `y=$6C..$74`. Mirrored movement,
general defensive cycle order, general shot selection, close routes, fouls,
violations, and arbitrary possession recovery are not inferred. The only
recovery is the bounded cycle-closure pass transaction described above. An
unsupported or unstable context aborts with neutral pads.
The TGLM-4 controller changes are shared by both profiles; the three-point
window and acceptance contract are unchanged, but its hardened controller has
not yet received a new smoke run.
The bot's front-threat policy uses a conservative `20x12` window, wider than
the original strict contact box (`abs(dx)<12`, `abs(dy)<7`). If an identified
threat cannot be selected through observed defensive A edges, the experiment
aborts instead of pretending the player can be controlled.

The first original-ROM two-point pilot stopped at exactly that guard because an
AI-controlled front defender could not be selected and cleared. TGLM-4 was
added to investigate that blocker. Its first launch then exposed and fixed a
Lua 5.1 60-upvalue compilation error in status generation; both tracked Lua
files now pass the bundled 32-bit Lua 5.1 parser. A final bounded launch still
failed before creating the startup sentinel, with empty FCEUX output logs, so
the controller and timing contract have not been runtime-validated. Diagnose
that FCEUX/Lua startup boundary before another pilot; do not compensate with
coordinate, delay, or controller sweeps. No live native two-point schedule is
claimed.
