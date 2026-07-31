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
| Team Data | Supported for team profiles, rosters, player detail, STARTERS, and PLAYBOOK; accumulated player-stat fields remain `.000`/zero until per-player accumulators are ported |
| All Star | Partial: selectors work, but the route stops before game launch |
| League Leaders | Partial: category navigation works; ranked player results remain unavailable until per-player season statistics are ported |
| Gameplay | Playable full-game shell with a ROM-derived pre-tip presentation (exact cards and capture-bounded later staging), ROM-derived ordinary human movement, passing, defender switching, a ROM-derived full-court horizontal camera/world renderer and live two-row scoreboard, close shots, one bounded ordinary-jump miss/three-point-make context, clocks, periods, halftime, overtime/final, audio, and result handoff. The original CPU command transport, target handlers, and direction quantizer are now isolated in a strict test-only asset, but are not live-wired; pre-tip claim settlement, live foul/contact and free-throw outcomes, general shot selection, and live CPU AI remain approximate. Rim-rattle is diagnostic-only and is not selected by normal live misses |

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

Regenerate this ignored pack after pulling code that adds or changes a strict
asset entry. A stale pack is deliberately rejected instead of being interpreted
with the new schema.

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
Player 1: arrows = directions, Z = NES A, X = NES B,
Enter = START, Shift or Space = SELECT
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

- Every preseason and season launch now enters the native pre-tip sequence:
  the exact mode/matchup/`1ST PERIOD` card waits and 16-pixel ROM lettering,
  referee/player close-up,
  center-court setup, descending ball, page-1 toss cut-in, jump contest, and
  live handoff. PRESEASON ignores NES B on the three cards; the regular-season
  route may cancel there because it sets the ROM's `$69` bit-0 gate. During the
  close-up each controller's first held B samples its tip timing. The
  presentation freezes game/shot clocks, queues original track 8, and switches
  to gameplay track 5 only at the 691-frame live handoff when GAME MUSIC is
  enabled. Lower error advances that side's contest and selects initial
  possession; exact original claim/tie settlement remains approximate.
- Directions move the owned actor through strict TGMO-1 locomotion: the selected
  player's ROM movement rating, GAME SPEED adjustment, condition term, Q4
  subpixel accumulator, one-update direction-change latency, diagonal reduction,
  vertical gates, animation phase, and fixed-bank court clamp are reproduced.
  CPU locomotion and ordinary locomotion pose-half rendering are not part of
  that live exact boundary yet. The original CPU command/target/direction
  routines have a separate strict inspection boundary described below, but it
  does not drive actors during normal play.
- NES A passes on offense and switches defenders on defense.
- NES B starts an offensive shot or attempts the current defensive
  steal/contact action. START and SELECT are inert during live play.
- Dunks use the strict ROM-derived cutaway, including its black transitions,
  staged sprite groups, return to live play, and supported audio/settlement
  timing. Live trigger, profile/direction selection, and make/miss policy are
  still native approximations.
- Layups use bounded ROM-derived motion data, but their live trigger and outcome
  policy remain approximate and their separate action/audio caller path is
  incomplete.
- Ordinary-jump support is deliberately limited to one captured
  human-controlled, away-side, right-facing context with deterministic miss and
  three-point-make branches. Other profiles, directions, and ordinary
  two-point makes remain unsupported. TGJS-2 adds a strict, test-only
  explicit-input translation of the ROM distance-flight initializer/update;
  the live scene still lacks the exact `$AD6E` launch inputs. The supported
  three-point make derives its post-score handoff from the exact state-08
  4-then-11x2 timer (26 updates). An early B release
  is safely normalized to the known supported release transition.
- The strict TGSR-3 rim-rattle prefix is available only through a focused debug
  diagnostic. Normal live misses retain the captured direct settlement route
  and never select the rattle; no selector or RNG behavior has been invented.
- The scene advances the game and shot clocks, score, possession, shot-clock
  violations, current native foul/free-throw flow, period banners, halftime,
  overtime/final presentation, and preseason/season result handoff.
- Live possession now synchronizes a strict ROM-derived binary offensive
  direction state through TGOR-1. Fresh launch is direction 0/AWAY; a real
  possession change toggles direction exactly once, while same-possession
  period/foul restarts preserve it. TGOR now selects the live TGCP follow
  direction and the `$00A0/$0260` world-space shot target. Possession changes
  invalidate only camera thresholds/latching; they do not reset or teleport
  the camera.
- Human free throws launch from the scoring team's current NES B level and
  have no timeout. CPU free throws use the bounded observed 125-update
  schedule. The exact two-orientation raw lineup, shooter-dependent actor
  stream, pose indexes, and base actor-state seeds are available through the
  strict TGFL-1 data foundation. The live scene now loads TGFL-1, selects its
  orientation from possession-synchronized TGOR-1, copies all ten exact raw
  positions into canonical court coordinates, settles TGCP-2 at camera
  `$0066` or `$0198`, and renders the actors through the matching TGCT/TGCP
  court frame. A focused independent TGFL-1 -> TGCP-2 checkpoint still proves
  the bounded orientation-1 six-visible/four-offscreen projection. Live play
  decodes the complete 768-by-240 court, follows the ball once per live update,
  and draws exact coarse/fine-scroll 32/33-column viewports. Shooter/secondary
  slot selection, held-ball attachment, and the camera composition are native
  adapters rather than new ROM claims. TGFL pose/state overrides, aiming,
  outcome, rebound, and CPU positioning/scripts remain unsupported or
  approximate.
- Rebounds, blocks, and steals remain approximate or nonsemantic. The current
  scene can transfer possession and attempt defensive contact, but it does not
  claim the original game's selection or outcome logic for those events.

See [PORTING.md](PORTING.md) and the
[gameplay state foundation](docs/gameplay-state-foundation.md) for the internal
asset contracts, provenance, and exact supported state boundaries.

### ROM-derived versus approximate

Strict ROM-derived data currently covers the pre-tip source spans and
card/cut-in/close-up assets, including the exact 61/121/61 card waits, Bank06
character mapping and 16-pixel 2-by-2 glyphs, the `$69` bit-0 cancellation
gate, and Bank04/fixed-`$D861` close-up motion. The later phase durations and
the 33-frame close-up motion anchor form a deterministic, capture-bounded
native 691-frame schedule; those timings are not claimed as exact ROM timing.
It also covers the complete 768-by-240 court decode,
camera-positioned tile/palette viewport slicing, CHR and palette entries,
embedded FCEUX RGB profile, actor pose data, numeric close-shot step
tables, dunk cutaway, the bounded ordinary-jump miss/three-point-make context,
TGSR-3 shot resolution, its exact 1/2/3-point classifier and
diagnostic-only rim-rattle prefix, the TGFL-1 raw free-throw lineup, the
TGCP-2 horizontal camera/projector and production live prime/follow, TGMO-1
controlled-player movement and strict actor dispatcher/clamp,
the test-only TGAI-1 CPU command transport, target-handler graph, direction
quantizer, and transactional TGAI-to-TGMO movement adapter,
TGOR-1 live possession-synchronized
offensive direction and target selection, THUD-1 live team/score/clock/player
lettering, the Bank01/Bank02/fixed-bank live
player palette recipe and all 29 home-team uniform colors, rules timing, and native
music/SFX/DMC programs. Strict entries are loaded from the same
revision-fingerprinted asset pack with exact-size and malformed-data checks.

Live player colors now follow the original matchup path. Profile byte 2 bit 7
selects Bank01 palette group `$B138/$B148`; `$04B0 & 3` selects the away/home
OAM palette pair; fixed `$DEAB-$DEDF` supplies away `$30` (or Lakers `$38`)
and the selected home color from `$DC19-$DC35`; `$B0ED-$B133` injects that
uniform color and its light variant. The current five roster slots are still a
native lineup policy, but the profile and team colors applied to those slots
are ROM-exact. The same binding is used by live play and dunk cutaways.

The fixed live HUD is backed by strict `gameplay/hud` THUD-1 (864 bytes,
FNV1a32 `3D13AA89`). Bank01 supplies the exact 29 five-tile team marks and
their two PPU destinations; Bank02 supplies the exact score/name character map
and first-initial-plus-nine-tile-surname formatter. Every mapped character is
cross-checked against the same-pack TTDT-1 `$FA` CHR record before the scene is
available. The score, clock, duplicated shot clock, and selected-player labels
remain fixed while TGCP moves the court. Team-mark destinations and character
mappings are ROM-exact; the other tile columns, colon tile, black backing, and
live `$FA` top-row binding are reference-verified bounds. Capping the wider C
score at three visible digits and choosing a CPU-side label from the current
holder/shooter matchup are native adapter policies, not reconstructed ROM
selection logic.

The live actor starting layout and current fixed five-player roster-slot
binding, CPU command selection/movement/AI integration, pre-tip actor geometry,
the exact original tip-claim settlement, general shot selection and make/miss policy,
live close-shot profile/direction selection, left-facing mirroring, contact/foul
detection, free-throw simulation, rebounds, blocks, steals, per-player game
statistics, and the HUD's non-controller actor-selection/fixed-column adapter
remain native approximations or are unsupported. The HUD font, team marks, and
Bank02 name formatting are no longer approximation placeholders.
`gameplay/penalties` TPNL-1 contains strict ROM-backed rule data,
but the live scene's current contact/foul code does not consume it yet.
Likewise, `gameplay/free-throw-lineup` TGFL-1 preserves raw world coordinates
and now supplies the live free-throw positions for all ten actors in both
orientations. The TGFL coordinate derivation is exact; selecting the shooter
from the current scoring holder (with a first-team-slot fallback), selecting
the secondary actor from opposing controller ownership, attaching the ball,
and composing the one-time TGCP settle are native integration policies. The
scene deliberately preserves existing actor poses instead of applying TGFL
pose/state/script fields. The live scene also loads
`gameplay/camera-projection` TGCP-2 (1536 bytes, FNV1a32 `53247856`) and
`gameplay/court` TGCT-1, primes the
native cursor from `$20` to `$21`, seeds at camera `$0100`, decodes the
768-pixel court, follows ball world X exactly once per live update, and clips
the 32/33-column viewport within the 256-by-240 gameplay subview. Actors,
anchors, ball Q8 coordinates, shot endpoints, proximity, passing, switching,
and AI all share world X/Y; actor and ball rendering uses TGCP projection.
Offscreen projection uses the deterministic native sentinel
(`visible=false`, X/Y zero) because the ROM branches before writing projected
Y.

The shared `TecmoGameplayCourtCoordinate` contract fixes `(0,0)` at the
upper-left of the complete 768-by-240 court, with X increasing right and Y
increasing down. Player positions and anchors are integer pixels; ball and
shot positions use Q8 in the same plane. TGOR exposes the ROM-backed hoop
anchors as `(160,148)` and `(608,148)`. The ordinary flight endpoint
`(hoop.x,143)` is deliberately separate. The transactional
`tecmo_gameplay_scene_court_coordinates` snapshot returns all ten players,
the ball, and both hoops and rejects out-of-court state without changing its
caller-owned output. This makes the coordinate system canonical; it does not
make the native approximate starting lineup ROM-exact.

TGCP is connected through typed transactional adapters rather than
scene-local scalar conversion. Launch and pre-tip handoff settle on the Q8
ball, each live update follows it once, and
`tecmo_gameplay_scene_court_projection` projects all ten players plus the ball
at one camera X. Player anchors stay integer; the Q8 ball is validated and
floored exactly once at the TGCP boundary. Offscreen objects retain TGCP's
neutral `visible=false`, X/Y-zero sentinel, and jump altitude applies only to
the shooting player. These adapters connect existing exact TGCP behavior;
the type conversion itself is native plumbing, not new ROM behavior.

Live actor drawing now consumes one transactional
`tecmo_gameplay_scene_court_frame`. It combines the possession-aware
`tecmo_gameplay_scene_court_slice` with all ten TGCP player projections and
the ball projection at one camera X, plus the scene frame and camera-follow
serial. For a stationary actor, each visible screen X change is exactly the
inverse signed camera delta; screen Y is unchanged by horizontal camera
motion. Fine-scroll, coarse-tile, possession-reversal, endpoint, and neutral
offscreen transitions are covered. The native left, center, and right
possession checkpoints render
distinct frozen backgrounds at camera X `102`, `256`, and `408`
(`4F52BCC1`, `9CC9CD31`, and `033B45D5`). TGCT decoding and TGCP camera
movement remain ROM-derived; this scene-level binding and its checkpoint
placements are native integration, not a claim about complete ROM possession
choreography.

Ordinary human-controlled movement now loads `gameplay/movement` TGMO-1 (1664
bytes, FNV1a32 `6C82A137`) with seven exact Rev 1 spans: Bank02
`$A89E-$A90D`, Bank04 `$ACE4-$AD25`, Bank05 `$879B-$8866`,
`$88F9-$89BC`, `$8E58-$8F96`, `$BF6C-$BFA7`, and fixed
`$F106-$F1B0`. It requires exact same-pack TGPL-1, TGCP-2, and TTDT-1.
For ordinary noncontradictory input the live adapter uses TTDT profile byte 0,
the current condition, and GAME SPEED adjustments `+5/-1/-6`; movement amount
is `max(8, adjusted_rating + (condition >> 4) - 6)`. It preserves the ROM's
Q4 accumulator, `amount-floor(amount/4)` diagonal step, one-update action
latency, `$4A/$EC` compare-before-move Y gates, and animation phase.
The live scene now feeds that exact direction/base plus animation-low-nibble
state into TGMO-1's pose resolver for initial and controlled-player court
frames. The metasprite renderer binds each record tag to the MMC3 R2-R5
selector addressed by its `$0479`-style slot, including the ball's `$C1`/R5
path; this replaces the unrelated close-shot-family pose 509 as the default
court stance and removes the former hardcoded R3 binding. The scene currently
selects TGMO's primary pose-table half as an explicit native policy; the
opponent-relative `$8F02` half choice is still unported. The pre-tip court pass
no longer composites the full matchup-card
team logo over the actors; those logo tiles remain presentation-card assets,
not player metasprite pieces.

TGMO-1 also applies the original selected-actor dispatcher exclusions and
violation-latch conditions around the `$00DF-floor(Y/2)` / page-1 /
`$0220+floor(Y/2)` trapezoid. Ordinary live control currently supplies object
state 0 and movement flags 0. The latch is retained in actor state but is not
yet connected to original reset/violation settlement. Opposing directions on one live input
axis are normalized to neutral as a native integration policy. Fatigue
evolution, opponent-relative pose-half selection, CPU locomotion/AI, and the
approximate starting layout, direction, and fixed five-player roster-slot
binding remain outside the exact boundary.

`gameplay/cpu-steering` TGAI-1 is a separate, test-only 7616-byte asset
(FNV1a32 `D6C4DB35`). It preserves Bank06's ten-actor/state dispatcher,
actor-to-reference and target-delta direction selectors, 24-way state-4
command dispatch, target-application helpers, and formation-stream selector;
fixed `$C006->$CBE0` temporarily maps Bank04 and copies one five-byte command
from the exact 680-record `$9F2E-$AC75` stream into `$C7-$CB`. The pure API
decodes only aligned records and reproduces the original inclusive 2:1
dominant-axis octant choice over court-reachable deltas, including the 6502's
16-bit doubling wrap at synthetic extremes. Direction codes `0..7` = right,
left, down, down-right, down-left, up, up-right, up-left. It requires exact same-pack
TGMO-1 and rejects malformed headers, handlers, source records, padding,
commands, provenance, or dependencies.

The deterministic `--gameplay-cpu-steering-harness` accepts one selected actor,
all ten canonical X/Y coordinates, possession, TGOR orientation, a
possession-consistent ball holder, an explicit opposing linked/matchup actor,
and difficulty. It prints the complete court snapshot, a canonical FNV1a32
input fingerprint, the selected target/delta, and the exact TGAI direction.
Holder targeting reuses the native scene's `48/48/40`-pixel hoop approach;
other actors target the supplied linked actor. That target choice and link are
developer-harness policy, not reconstructed ROM behavior. Zero delta reports
`direction=keep` without fabricating a prior direction.

`--gameplay-cpu-steering-movement-harness` takes that same complete snapshot
plus explicit movement rating, condition, GAME SPEED, and frame count. For a
nonzero delta it converts the exact TGAI direction identity through TGMO's
validated NES direction table and advances the selected CPU actor with the
exact transactional TGMO kernel, including its one-update action latency, Q4
accumulator, diagonal reduction, animation, and secondary-actor court clamp.
After every step it writes the actor's resulting canonical coordinate back
into the next snapshot before steering is evaluated again. TGAI's zero-vector
no-write has no held-controller-bit equivalent, so the adapter supplies
neutral as an explicitly native harness policy; TGMO latency still applies.
The initial offensive-facing direction, zero-input choice, rating/condition
inputs, and harness target/link policy are not claimed as reconstructed ROM
CPU command behavior.

TGAI-1 does not yet choose a live actor command, bind the ROM's live
linked-actor state, or replace the scene's approximate CPU movement. It also
does not claim the nearby `$B081-$B32E` candidate scan as ordinary targeting,
nor does it infer shot/pass/steal policy. Use the console-only commands
`--gameplay-cpu-steering-test`, `--gameplay-cpu-steering-inspect`,
`--gameplay-cpu-steering-harness`, and
`--gameplay-cpu-steering-movement-harness`, or run
`tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.
The exact call graph and remaining integration boundary are documented in the
[CPU steering evidence note](docs/gameplay-cpu-steering.md).

`gameplay/court-orientation` TGOR-1 is loaded by the live scene and owns
the binary offensive direction, previous direction, tracked possession team,
transition serial, and full offensive-hoop coordinate. Its direction and
`($00A0,$94)/($0260,$94)` hoop anchors drive TGCP and shot launch; the
separately proven flight target Y is `$8F`. That same direction selects the
production TGFL-1 free-throw lineup and its `$0066/$0198` settled camera.

The first two intro screens, TECMO/rabbit and NBA license, are silent. Strict
opening music begins at the license-to-arena handoff. On the first START,
the title setup preserves opening audio through its five proven native yields,
then hard-stops it on imported title frame 5. A fresh
second START queues original SFX 10 once on confirmation frame 1; the title
remains visible through frame 126, and frame 127 queues presentation/menu
track 6 and enters the blue menu. Accepted Player 1 NES A releases on the blue
menu queue original SFX 8; START, directions, B, PERIOD A+B, rejected chords,
and held A do not. Gameplay music,
halftime/final presentation music, crowd responses, SFX, and DMC playback work
in the native runtime, but nonlinear cycle-exact NES APU fidelity is not
claimed. GAME MUSIC gates gameplay track 5 and its evidence-bounded restart
cue; GAME SPEED does not change menu or soundtrack tempo. The visible `SIC`
left beside the speed popup is an authentic overlap from the original menu.
The pre-tip cards always queue the original matchup stinger (track 8);
GAME MUSIC gates only the later track-5 live handoff.
Close-up B samples retain the ROM's bounded error semantics. Smaller error
launches the corresponding native contest interaction sooner and determines
initial possession; equal errors choose away. That lower-error policy is
ROM-supported, but the final claim/tie settlement remains an explicit native
approximation.

Older diagnostic screens and the modern Play Game/Quit menu remain available
through explicit render-test/debug paths for development work.

## Common Commands

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --summary
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --banks
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --chunks
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --assets
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --roster CHICAGO
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --gameplay-state-test
.\tools\Run-FrontendAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT>
.\tools\gameplay-lab\Test-GameplayLab.ps1
.\tools\Run-GameplayShotResolutionTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayPenaltyTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayCameraProjectionTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayMovementTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayPreTipTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayDunkCutawayTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

For a deterministic developer trace of one movement vector, build a private
pack and run, for example:

```powershell
.\build\tecmo_port.exe --gameplay-movement-harness build\tecmo.assetpack 0 0 384 148 1 0 0 right 8
```

This is a console-only test harness. It is not an in-game debug mode or a route
reachable from normal play.

The tracked gameplay-lab command above is a static safety/schema test. See its
[README](tools/gameplay-lab/README.md) for private pilot instructions and
required local inputs.

Render the normal menu or a focused intro frame:

```powershell
.\build\tecmo_port.exe --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-start build\gameplay_start_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-pretip-frame631 build\gameplay_pretip_toss_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-live-start build\gameplay_live_start_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-possession-left build\gameplay_possession_left_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-possession-center build\gameplay_possession_center_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-possession-right build\gameplay_possession_right_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame12 build\gameplay_jump_12_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-rattle-frame89 build\gameplay_jump_rattle_89_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame85 build\gameplay_jump_make_85_test.png
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

The current Rev 1 builder emits an 81-entry pack. In addition to the raw PRG and
CHR entries, it contains strict logical assets for the opening, arena, finale,
title, blue menu, frontend audio, preseason, Team Data, team management, season state, music,
gameplay audio, court, live court-orientation state, controlled movement, poses, close shots, dunk
presentation, the bounded jump route, shot-resolution rules, penalty rules,
the raw free-throw lineup, and the complete pre-tip presentation contract.
These entries are derived directly from the local ROM during pack construction;
decompilation files and captures are not pack inputs.

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
  close-shot, dunk, bounded ordinary-jump miss/three-point-make, TGSR-3 shot
  resolution/point classification, diagnostic-only rim-rattle, rules, state,
  and audio assets
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
