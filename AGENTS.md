# Agent Notes

These notes are for Codex/AI-agent development work in this repo. Keep the README user-facing; put debug-only capture details here.

For porting direction, follow [PORTING.md](PORTING.md). In short: this is a
native C port, not an emulator wrapper. Final runtime paths should consume
ROM-derived asset packs and native C scene/game concepts, not decompilation
files, Lua captures, or emulator-shaped replay data.

## Current Product Surface

The Desktop shortcut uses the Windows GUI `build\tecmo_port_game.exe` with
`--root <PORT_PROJECT_ROOT> --play` and boots directly into the native
TECMO/rabbit opening. Normal Win32 initialization permits an empty legacy
roster, so this path depends on the strict ROM-derived asset pack under the port
root instead of loose decomp roster files. The same sources also build
`build\tecmo_port.exe`, which retains the console subsystem and explicit
`--root <LOCAL_DECOMP_ROOT>` workflows for CLI tools and tests. The modern Play
Game/Quit menu remains a debug/test surface only; normal Win32 play must not
route through it.

Do not re-add Title Screen, Intro Lab, CHR Playground, Rosters, or the modern
menu to normal play unless the user explicitly asks. Those paths can stay
available for agents through direct mode setup, render-test modes, or temporary
debug work.

## Data Boundaries

Do not commit or paste original game data. Keep these local or ignored:

- ROMs and rebuilt NES images
- PRG/CHR bytes
- decompiled or lifted ASM chunks
- generated rosters
- generated CHR PNGs
- emulator capture logs
- trace JSON files
- screenshots derived from private local data unless they are intentionally safe docs screenshots

Use `--root <LOCAL_DECOMP_ROOT>` or `TECMO_DECOMP_ROOT` only for explicit
private developer workflows. The generated normal-play shortcut must pass the
port project root explicitly so an ambient decomp environment variable cannot
become a runtime dependency. Do not hard-code private paths into committed
files.

## Large Log Handling

Lua emulator logs can be large. Avoid loading full `.ndjson` logs into the conversation. Prefer filtered commands such as:

```powershell
Select-String -Path build\emu_intro_memory_watch.ndjson -Pattern '"kind":"frame_state"|oam_frame_diff'
rg -n '"frame":(8[0-9]{2}|7[0-9]{2})|oam_frame_diff|scroll' build
```

When checking timing, extract only the frame range and fields needed for the current question.

## Sub-Agent Workflow

For non-trivial porting work, prefer a deliberate sub-agent cycle:

1. Use read-only explorer agents to inspect ASM, docs, and current C flow. Ask
   for concrete file/line mappings and behavior summaries, not broad opinions.
2. Put code-writing worker agents in temporary git worktrees with narrow file
   ownership. Tell them other agents may be working nearby and not to revert
   unrelated changes.
3. Have worker agents commit their work on the temporary branch after building
   and running focused tests.
4. Use a separate reviewer agent to inspect the worker commit before merging.
   If review finds issues, send the worker a targeted follow-up in the same
   worktree and repeat the review cycle.
5. The main agent owns integration: cherry-pick or merge only reviewed commits,
   run the full relevant verification set, inspect key screenshots when visuals
   matter, then push.
6. Close sub-agents and remove temporary worktrees/branches when the task is
   done.

Do not leave temporary worktrees or agent branches around after reviewed commits
have landed on `main`. Keep untracked handoff notes such as `NEXT_SESSION.md`
out of unrelated commits unless the user explicitly asks to commit them.

## Useful Verification Commands

```powershell
.\build.ps1
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --music-test
.\build\tecmo_port.exe --frontend-audio-test
.\build\tecmo_port.exe --gameplay-audio-test
.\build\tecmo_port.exe --gameplay-state-test
.\build\tecmo_port.exe --team-management-test
.\build\tecmo_port.exe --season-test
.\tools\Run-MusicTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayCourtOrientationTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-TeamDataTests.ps1 -RomPath <LOCAL_ROM.nes>
.\tools\Run-TeamManagementTests.ps1 -RomPath <LOCAL_ROM.nes>
.\tools\Run-SeasonTests.ps1 -RomPath <LOCAL_ROM.nes>
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode menu build\main_menu_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-clean-frame539 build\intro_arena_clean_frame539_test.png
```

If the build fails with `LNK1104` for either executable under `build\`, check
whether the local game window or console process is still running before
rebuilding.

## Debug Render Modes

Hidden/debug screens are still useful through render-test modes. Common examples:

```powershell
.\build\tecmo_port.exe --render-test-mode title-screen build\title_screen_runtime_test.png
.\build\tecmo_port.exe --render-test-mode first-sprite build\first_sprite_test.png
.\build\tecmo_port.exe --render-test-mode first-sprite-debug build\first_sprite_debug_test.png
.\build\tecmo_port.exe --render-test-mode intro-license build\intro_license_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-transition build\intro_arena_transition_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-clean-frame539 build\intro_arena_clean_frame539_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-opening-clean-frame0 build\finale_opening_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-reverse-frame27 build\finale_reverse_debug_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-staged-clean-frame1 build\finale_staged_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-title-clean-frame473 build\finale_title_test.png
.\build\tecmo_port.exe --render-test-mode intro-finale-hold-frame0 build\finale_hold_debug_test.png
.\build\tecmo_port.exe --render-test-mode chr-playground build\chr_playground_test.png
.\build\tecmo_port.exe --render-test-mode rosters build\rosters_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-start build\gameplay_start_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame75 build\gameplay_jump_75_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame87 build\gameplay_jump_87_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-rattle-frame89 build\gameplay_jump_rattle_89_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame85 build\gameplay_jump_make_85_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame111 build\gameplay_jump_make_111_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-dunk-frame16 build\gameplay_dunk_16_test.png
```

These are development tools, not main-menu entries.
`gameplay-close-shot-frameN` remains a compatibility alias for the canonical
`gameplay-dunk-frameN` numeric-variant-0 checkpoint.

The native desktop developer overlay owns a shared F4 lab chooser while F3 is
enabled. F5/F6 select either the violation-presentation lab or the shooting
pose-table lab, and F4 opens the selection or returns from a lab to the
chooser. The shooting lab reads only strict same-pack TGJS-2/TGPL-1 data: it
walks all 32 numeric family/profile/direction bases and phase-nibble poses 0..7,
uses a clearly labeled `$30` preview uniform, and offers a separately labeled
F7 mirror inspection. Its ten-frame slow playback cadence is a viewing aid,
not ROM gameplay timing or live shot-policy evidence. F3 off closes the tool
and resumes the untouched normal runtime.

## Lua Watchers

For original intro comparison in FCEUX, use the Lua scripts only against a private local rebuilt NES image:

```text
tools\emu_intro_first_sprite_watch.lua
tools\emu_intro_memory_watch.lua
tools\emu_intro_arena_irq_watch.lua
```

They write ignored local outputs under `build\`, including:

```text
build\emu_intro_first_sprite_watch.ndjson
build\emu_intro_memory_watch.ndjson
build\emu_intro_arena_irq_watch.ndjson
build\emu_intro_arena_irq_watch.log
```

The broad memory watcher logs OAM diffs, compact PPU write batches, MMC3 bank writes, and scroll evidence. Use narrow filters when reading it.

To avoid repeatedly parsing large raw watcher logs, distill the local-only arena data into a compact ignored capture:

```powershell
.\tools\Import-IntroArenaCapture.ps1
```

This writes `build\intro_arena_capture.ndjson`, which the arena renderer loads before the raw watcher logs. The compact file is generated data from the private reference environment and must stay uncommitted.

## Tracked Gameplay Laboratory

`tools\gameplay-lab` is a developer-only, read-only FCEUX control and telemetry
surface for two closed Rev 1 MAN VS MAN ordinary-jump profiles. The default
three-point baseline retains its exact prior window and acceptance contract;
TGLM-4's shared hardened controller has not been smoke-tested against it. The
`ordinary_two_point_make` profile exposes only `x=$0108..$010F`,
`y=$6C..$74` and requires point value 2, terminal MAKE, and score delta 2.
Its canonical TGLM-4 `tecmo_rev1_map.lua` owns every profile, address,
mapper-aware hook, timing bound, and revision fingerprint; recorder/status
output uses TGLAB-4. Per-actor
altitude, horizontal, and vertical velocities are separate raw 16-bit fields.
Accepted hooks snapshot direct object-slot-10 H/V and saved scratch H/V
`$038D-$0390` in the callback before queued emission. Those saved fields are
scratch and, at `$A7A9`, can predate its `JSR $A790`; do not infer signed
direction or same-invocation ownership. The PowerShell runner requires explicit local ROM/FCEUX
paths (or its specifically named environment variables), rejects other FCEUX
processes, validates the exact ROM and FCEUX 2.6.6 SHA256 fingerprints, runs
hidden with redirected output, a hard timeout, an in-Lua tracked-text cap, and
a polled 64 MiB whole-session limit covering logs, screenshots, and optional
FM2. It writes only below ignored `temp-videos/gameplay-lab/<timestamp>`.

After the proven power-on navigation and tip input, gameplay control is
state/coordinate driven. The script proves live mode, two-player control,
distinct teams, orientation/side, and a stable selected holder and ball.
Defensive A selection is accepted only after Bank06 `$91CB` pre-store evidence
matches `$0309` on the next frame; a cycle closes only after a different actor
and a return to its origin, with at most six confirmed stores in the entire
pilot. Only the
two-point profile may then pulse offense A once, observe a different holder,
and rebuild the eight-frame holder proof, with at most four transfers. Movement
holds one cardinal direction, admits only state 0 or its matching movement
state, and uses a neutral readback gate before changing direction or controller.
It holds 12 neutral frames in the proven window, then applies the captured B
timing. It supplies complete neutral-or-active joypad tables for both ports
every frame. It never changes RAM, uses cheats, or touches emulator state
slots; every failure and normal exit neutralizes both pads and closes the
runner-owned emulator.

The first original-ROM run of the two-point profile stopped safely because the
AI-controlled front defender could not be selected and cleared. TGLM-4 adds
bounded cycle/pass/reacquisition control and hook-time timing snapshots for
`$8C57/$8C78`, `$AD4E/$AD50/$B32C/$AD68`, `$B100` and its branch
boundaries, result/score helpers, and `$8FB9/$9042` actual possession swap.
The strict pending contract orders `$B995->$B9D7` point value 2 and
`$91BC->$933B->$942D` MAKE before flight. It requires raw shooter direction
`$05` before the ordinary-shot remap, raw direction `$00` afterward and at
target setup/solver, phase low nibble `$05`, close mode `$00`, target
`$00A0/$008F`, 16-bit slot count `$003C`, slot position shooter `+(2,-1)`,
altitude `$3900`, altitude velocity `$04EC`, horizontal raw velocity
`$FF88..$FF8F`, vertical `$001D..$0026`, 63 `$B100` entries, 26 state-08
updates, one +2 score commit, and SFX mailbox `$0B` throughout the same-frame
actual swap. Its first launch exposed a Lua 5.1 60-upvalue compilation error in
status generation. The status writer was split without changing its acceptance
predicate or 63 emitted keys, and both tracked Lua files pass the bundled
32-bit Lua 5.1 parser. A later bounded launch reached live setup but aborted at
frame 4214's defensive-A store confirmation deadline before any shot; it
supplied no `$B100`, state-08, or score evidence and closed FCEUX. Do not widen
this failure into coordinate/timing/controller retries. This control upgrade
has not produced a passed pilot and is not evidence that an ordinary two-point
make works.

Telemetry, hook events, screenshots, logs, movies, and status files are local
research evidence only. Native C and asset-pack import/runtime paths must never
read them. Run `tools\gameplay-lab\Test-GameplayLab.ps1` for the committed
static safety/schema gate; private smoke runs must pass the ROM and FCEUX paths
as invocation arguments and inspect only compact status or short log tails.

## Asset Pack Direction

Prefer moving local ROM-backed assets into an ignored `.assetpack` instead of making runtime code parse raw decomp files or emulator logs directly. Build the initial pack with:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

The initial pack contains `system/manifest`, `system/source-map`, `prg/bankNN`, `prg/fixed`, `chr/all`, and `chr/bankNN` entries. Generated `.assetpack` files are ignored local outputs. Runtime CHR loading already prefers `TECMO_ASSETPACK` or `build\tecmo.assetpack`; keep extending that pattern with the asset-pack builder API by adding named memory or local-file entries to the pack/import step, then pointing C render/game code at those entries.

## Opening Sequence Notes

The current opening path includes:

- ROM-only TECMO/PRESENTS background plus the native 20-piece rabbit OAM group
- ROM-only NBA license screen
- arena/jumbotron/crowd transition from ROM CHR through native arena bands
- native TASG-2 jumbotron and anchored goal sprite groups for the arena pan
- the ROM-only post-PASS finale, command-$14 NBA emblem continuation, and start screen

The first two screens use strict TISC-1 entries: `intro/tecmo-presents-screen`
and `intro/nba-license-screen`. The first contains the decoded screen `$00`
nametable, attribute palettes, resolved background CHR, the 20-piece `$BD9E`
rabbit group with resolved sprite CHR, and both palette halves. The second is
the decoded background-only screen `$02`. Native timing includes the title
fade and rabbit clear through the license handoff at frame 133, then the NBA
delay/fade and arena handoff at frame 277. Normal startup does not read
`intro_composite_trace.json`; loose trace parsing is diagnostic-only and must
be explicitly enabled with `TECMO_ALLOW_LOOSE_INTRO_TRACE=1`.

The normal arena render must not replay captured screen `$18` nametable or OAM data. The ROM-only importer decodes the fixed-bank screen descriptor and compressed Bank00 stream into `arena/intro/background-layer`, a versioned native `32x51` tile layer whose cells contain exact attribute-derived palette indexes and resolved `chr/all` offsets. It also emits `arena/intro/sprite-groups` as TASG-2 with the exact NES sprite palette, jumbotron pieces, and goal pieces. TASG-2 reuses piece bytes 10..11 as signed `connector_overlay_y_adjust`; the imported center `dy=32` goal connector record is the sole `-1` overlay adjustment and all other pieces use zero. Runtime first draws its canonical ROM-derived second tile at `y+8`, then draws a shifted copy of that tile with palette indexes 0 and 1 transparent while preserving exact ROM palette colors for indexes 2 and 3. Canonical goal position and extent remain unchanged. Runtime rendering requires both native entries, scrolls TATL as the background, and projects TASG groups from their stored anchors using the transition state. Capture-shaped arena loaders remain only as migration/debug scaffolding; palette-cycle migration can continue without replacing the exact background or sprite-group paths.

The post-PASS continuation is stored in `intro/finale-sequence` as TFIN-1.
It contains five native two-page screens, resolved palettes and CHR offsets, a
shared ten-piece sprite geometry with two scene palettes, imported scene
anchors, reverse-transition metadata, three title bands, and 44 resolved 2x2
title slots. Slots contain native page positions and tile cells, not imported
text. Runtime uses only TFIN-1 and `chr/all`; missing or malformed finale data
fails cleanly with no decompilation or capture fallback. Native play advances
through the named finale phases, then continues into the title attract route.

The command-$14 continuation and start screen are native ROM-only assets.
`title/attract-continuation` uses TATR-2 for the decoded screen `$01`, both
sprite-palette phases, the 49-piece NBA emblem, attribute states, resolved CHR
offsets, and the bounded 621/642-frame completion/reset points.
`title/start-screen` uses TTLE-1 for decoded screen `$03`, its palette and
resolved CHR cells, and the two exact ten-cell prompt rows. Runtime requires
these entries plus `chr/all`; it does not read Lua captures or trace data.
The first START enters the title after a ten-frame load window. It must be
released before a second START is accepted. Confirmation alternates the blank
and visible prompt rows every seven frames for 126 frames, then hands off at
frame 127 to the original blue start-game menu.

The blue menu is a strict ROM-only native scene. The importer emits
`menu/start-game` as TSGM-1: two precomposed 32x30 pages, nine exact title-out /
black / menu-in palette stages, the 49-piece NBA emblem, the root cursor,
settings overlays, digit cells, and native timing/input/route metadata. The
screen is composed during import from screen `$04` plus Bank03's bounded text
records and character map; runtime does not parse those records or use an
emulator dump. Frames 0-7 retain TTLE-1's `title/start-screen`, so the runtime
dependency set is TSGM-1, TTLE-1, and the same pack's exact 262144-byte
`chr/all`. Bank01's root-cursor selector `$30` and tile `$24` resolve the
exact 8x16 pair at `chr/all` offset `$C240`; both the source record and resolved
CHR pair are revision-fingerprinted. Native timing preserves palette
checkpoints at local frames 0,
2, 4, 6, 8, 20, 24, 28, and 32. Root Up/Down wraps across seven items, repeats
every eight held frames, and only NES A dispatches; B, START, SELECT, Left, and
Right are ignored. SEASON GAME slides to the six-item second page
over exactly 32 frames at eight background pixels and five emblem pixels per
frame; B reverses the same transition. That second-page boundary maps GAME
CONTROL, SCHEDULE, GAME START, STANDINGS, and LEADERS to native
`TECMO_MODE_SEASON_MENU`; TEAM DATA maps to `TECMO_MODE_TEAM_DATA`. GAME START
prepares the exact pending schedule ordinal and teams, then launches the native
gameplay scene. It must not fall through to `PLAY_SETUP`, advance TSAV before a
validated result, or synthesize a score.

Popup construction follows Bank03 `$AB77`: row zero transfers before its first
yield, then one row transfers per frame. MUSIC starts with one of six rows at
setup frame 0 and enters its helper on frame 6; SPEED does the same for eight
rows and enters on frame 8. PERIOD fills six rows by frame 5, holds one extra
full cursorless setup frame at frame 6, and enters its helper on frame 7.
Teardown begins full at frame 0 and removes one bottom row per frame (six frames
for MUSIC/PERIOD, eight for SPEED). Setup-to-popup, teardown-to-root, and both
32-frame season-slide destinations execute their helper on the final update,
but the cursor reaches displayed OAM one frame later. TSGM byte 148 binds that
commit delay to one frame; it is not a hardcoded renderer exception.

The post-return `$E481` fade is used for root TEAM DATA and all six season-page
departures. It holds palette stage 8 on exit frames 0-1, then stages 7/6/5 on
2-3/4-5/6-7, black stage 4 on 8-10, and emits the one-shot handoff on frame 11.
PRESEASON's `$9966` route and ALL STAR's `$8221` route enter their native
submenu construction directly and must not run `$E481` first.

PRESEASON's B/X return uses its own direct path: it rebuilds the stable
root on the PRESEASON row and resets menu settings to their initialized values.
ALL STAR, TEAM DATA, and season-management destinations use the recorded return
path, preserve committed MUSIC/SPEED/PERIOD values, and restore the exact root
or fully slid-in season row. Their return controls remain submenu-specific;
notably PROGRAMMED uses START/SELECT because B edits the selected record. The
recorded-return neutral gate consumes the held return input, its release edge,
and the first fully neutral frame before the menu can process input again.
PRESEASON and SEASON now use explicit native gameplay launch/result handoffs.
ALL STAR remains at its documented prelaunch boundary; no route may fall
through to the modern diagnostic court.

PRESEASON is a strict ROM-only native scene backed by `menu/preseason` TPRE-1.
It composes the 14-row CONTROL and DIVISION overlays and the eight-row
DIFFICULTY overlay from Bank03's character map and menu records over TSGM-1's
screen `$04`, then imports the four team nametables, palettes, CHR mappings,
team order/coordinates, and Bank01 player markers. Both `$8036` marker records
provide the same CHR selector `$30`; the importer resolves the seven referenced
8x16 pairs into a 224-byte CHR contract with FNV1a32 `1E505537`. CONTROL row zero opens
EASY/MEDIUM/EXPERT; accepting commits the difficulty and selects MAN VS COM,
while B restores the previously committed value. CONTROL rows 1-6 all proceed
to team setup. MAN VS MAN gives the second division/team selector to pad 2;
the other control modes keep both selectors on pad 1. Division Up/Down and
team Left/Right wrap and repeat every eight held frames. A/B actions remain
release-triggered, opposite directions consume the repeat gate without moving,
and a second player in the same division skips the first player's occupied
team. Division B returns to CONTROL, and team B rebuilds the same player's
division menu.

TEAM DATA is a strict ROM-only native scene backed by `menu/team-data` TTDT-1.
The 96372-byte payload (FNV1a32 `812628F0`) carries three decoded screens, 29
team selectors, 29 teams with 12 players each, four dynamic profile palette
groups, 27 expanded team logos, resolved player portraits, and timing/input
metadata. Runtime also requires the same pack's exact 262144-byte `chr/all`;
there is no decompilation, trace, screenshot, video, Lua, PPU/OAM dump, or save
state fallback. Missing, malformed, oversized, wrong-revision, or cross-pack
assets fail closed.

The root and season TEAM DATA routes retain the TSGM-1 fade through its frame-11
handoff, then TTDT-1 keeps rendering off for four local frames, turns rendering
on while black, and shows capped palette stages at local frames 7/11/15/19;
the selector is stable with its cursor on local frame 20. A/B actions are
release-triggered. Selector-to-profile is black at local frame 8, render-off at
10, render-on black at 16, capped/full at 19/23/27/31, and stable at 32.
Profile-to-roster changes only cursor/OAM state and is stable on the following
frame. STARTERS and PLAYBOOK also enter natively on the following frame. Roster
pages slide over 32 frames at eight pixels per frame.
Roster-to-player-detail is black at 8, render-off at 10, render-on black at 15,
capped/full at 18/22/26/30, and stable at 31; B uses the 32-frame reverse timing.
All three profile A routes are native. `PLAYERS DATA` opens the roster;
`STARTERS` edits five unique starters from the seven-player bench with player
detail and reset flows; `PLAYBOOK` edits four unique slots from eight plays with
the original replacement carousel and reset flow. Their mutable session state
remains native and never routes to gameplay.

STARTERS and PLAYBOOK require the same pack's strict 21061-byte
`menu/team-management` TTMG-1 payload (FNV1a32 `D192EAC6`) plus TTDT-1 and
`chr/all`. Missing, malformed, oversized, wrong-revision, or cross-pack
dependencies fail closed. Run `tools\Run-TeamManagementTests.ps1 -RomPath
<LOCAL_ROM.nes>` for its state, flow, malformed-data, and nine-pixel-checkpoint
coverage.

Profile colors come from Bank06 `$AC0B-$AC4A`, selected by `$A3AD`. Logos use
the Bank06 `$A2E4` layout tables and Bank03 `$8017` origin table; ATL resolves
to the exact E4-backed 10x6 matrix at `(16,48)`. Player names/attributes and
direct All-Star pointers come from Bank02, portraits use Bank03 `$8D5C/$B432`
plus Bank00 metatiles, and ability bars follow Bank02 `$AD5B`. The supported
TEAM DATA boundary includes player detail, STARTERS, PLAYBOOK, and their return
paths to profile, selector, and blue menu; none launches gameplay. Run the
focused suite with `tools\Run-TeamDataTests.ps1 -RomPath <LOCAL_ROM.nes>`.

The CONTROL/DIVISION stack fades at team-entry frames 3/5/7 and is black from
frame 9 through the frame-16 input handoff. The team screen remains black on
palette frames 16-17, displays capped stages on 18-21, 22-25, and 26-29, and
is fully bright from 30. Team exit is full at frame 0, capped on 1-2, 3-4, and
5-6, black from 7, and rebuilds the menus after 32 frames. The rebuilt division
menu is constructed while black; its displayed return fade is black at counter
0, capped on 1-4, 5-8, and 9-12, and full at 13. Input is active during that
return fade, matching the fixed helper.
Selector entry seeds `$E1=5`; setup, teardown, team-entry, and team-exit phases
freeze that value, and only interactive selector frames decrement it.

The final second-team A action seeds `$E1=5` and now emits the explicit native
preseason gameplay handoff. The port does not replay Bank03 `$B277-$B282` or
fixed `$E481`; runtime transfers the selected teams, difficulty, ownership, and
menu settings into `TecmoGameplaySceneLaunch`. TPRE-1 is 26736 bytes with
FNV1a32 `D9EE49F4` and requires the same pack's exact 14112-byte
TSGM-1 (`DF89006B`) and 262144-byte `chr/all` (`F6F6E854` /
`96A64F53B240ABB4`). Import fingerprints cover `$9966`, ownership/difficulty
flow, popup/input tables, `$B1CC`, focused `$B283/$B287` division/team maps,
the `$B277` launch boundary, `$8031` cursor and `$8036` player records,
four descriptors/streams/palettes, fixed input/loader/fades, and the full CHR.
Missing, malformed, oversized, cross-pack, or revision-mismatched data is a
hard native load/render failure. Emulator frames, Lua logs, OAM/PPU dumps,
states, and `temp-videos` remain local verification evidence only.

The settings popups are native: MUSIC wraps OFF/ON, SPEED wraps
FAST/NORMAL/SLOW, and PERIOD clamps across 2/3/4/8/12 minutes. A accepts the
highlighted setting and B cancels it, but `$07F6=0` makes every menu A/B action
release-triggered. Root, season, MUSIC, and SPEED reject the prior action while
the current NES controller byte is nonzero; held A/B never activates. When the
current byte reaches zero, the previous byte gets one final masked check: root's
`$9F87[0]=$80` admits released A only, while generic `$C0` rows admit released
A/B and raw A+B accepts with A priority. Current Up/Down still takes the generic
direction path, so A+Down first moves and releasing both activates the newly
selected row. Native byte order is A `$80`, B `$40`, SELECT `$20`, START `$10`,
Up `$08`, Down `$04`, Left `$02`, and Right `$01`.

Accepted release actions return through `$D788` and seed directional `$E1=5`;
generic direction `$D79D` writes eight before the same-loop tail decrement, so
held direction repeats on the eighth following frame. Generic release actions
reach `$D788` before that directional gate. PERIOD is the exception: it first
consumes `(current|previous)&$0C`, including direction release and zero-delta
Up+Down, so that preliminary path can suppress and lose released A/B. PERIOD A
alone accepts and B alone cancels on release; raw A+B is consumed with `$E1=5`
but does neither. Season slides do not tick `$E1` for their first 31 steps; the
32nd step enters the destination helper in the same update and ticks 5 to 4.

TSGM-1 import is revision-locked with fingerprints for its descriptor,
compressed and decoded screen, composed two-page result, palette sources,
sprite selectors/palette, emblem, cursor, character map, menu/settings text
records, fixed loader/fades, input tables, and season transition. Its exact
payload fingerprint is `DF89006B`; metadata includes the one-frame cursor
commit plus ROM-derived MUSIC `(47,200)`, SPEED `(47,167)`, and PERIOD
`(71,200)` cursor anchors. The anchors come from the popup flow selector indexes,
Bank03 coordinate parameter tables, and Bank01 cursor `dy=-4`; the full CHR
contract is 262144 bytes / FNV1a32 `F6F6E854` /
FNV1a64 `96A64F53B240ABB4`. Exact-size directory preflight rejects forged sizes
before allocation. The sanitized `system/source-map` records every ROM source
range plus TTLE-1 and `chr/all` runtime dependencies.
The MUSIC overlay is seven tiles wide and authentically leaves the final
`SIC` cells from the underlying `GAME MUSIC` row visible; do not erase that
overlap as a native cleanup.
Missing, malformed, cross-pack, or out-of-range menu data must remain a native
render failure; captures under `temp-videos` and FCEUX/Lua screenshots, logs,
states, PPU/OAM dumps, and traces are verification material only.

Native NES colors use the exact embedded 192-byte RGB profile distributed as
`palettes/FCEUX.pal` with FCEUX 2.6.6 (FNV1a32 `9F872B25`). Runtime does not
load that external file. This replaces the former generic lookup whose bright
blue did not match the known FCEUX reference: NES color `$01` is now
`#24188C`. Run `build\tecmo_port.exe --video-test` to verify the complete
profile fingerprint, representative mappings, and six-bit index masking.

A bounded local FCEUX pass across frames 1350-2550 confirmed that the
post-arena CLIPPERS, BUCKS, PASS, and finale assets are present. The finale
marquee intentionally scrolls its text independently of the magenta underline;
the late underline-only frames are not missing glyph assets. Keep mid-write
checkpoints 192, 288, 384, and 448 in the intro suite so future regressions
cannot hide behind the existing blank/tail endpoints. Captured PNGs and state
CSVs remain ignored verification material only.

Opening music is native and ROM-only. The importer emits `audio/music` as the
strict 36784-byte TMUS-1 payload (FNV1a32 `05C00ECB`) for requested music IDs
5, 6, 7, and 8: gameplay, presentation, opening, and pregame matchup stinger. It
compiles Bank04's bounded music graph into 2251 native semantic instructions
(`note`, voice/envelope selection, legato, pitch delta, rest, bounded loop,
resolved call/return, and end), deduplicates 37 voices, and imports 75 fixed-bank
periods. Each channel retains the engine's single live `$C0` loop counter;
separate commands do not receive artificial persistent counters. Runtime does
not retain or interpret 6502 addresses, phrase pointers,
or raw music opcodes. Import fingerprints cover Bank04 `$8AA4-$9F05`, its
18-byte directory at `$8CD0`, fixed `$F2F2-$F9D0`, the period table at
`$F93B-$F9D0`, and each requested track range. The sanitized source map records
those ranges; no ASM, decompilation file, trace, capture, video, log, screenshot,
state, or dump is an input.

Match fixed `$F7D5-$F7DB` when decoding a voice timing byte: attack is bit 7,
decay is bits 5-6, and release is bits 2-4. Music command `$91` with operand
zero resets both pitch-delta bytes; only nonzero operands add a signed delta.
The focused regression anchors real raw `$08`/`$07` voices, track-6 pulse-1
semantic resets 492/716, 100000-tick looping runs for IDs 5/6, and ID 8's clean
396-inclusive-tick termination.

The native sequencer advances at exact NTSC cadence `39375000/655171`
(approximately 60.0988 ticks per second) from the audio sample clock, not the
render loop or the GAME SPEED menu value. The TECMO/rabbit and NBA-license
scenes are silent. Opening ID 7 is queued once at the native license-to-arena
frame-277 handoff, matching Bank04 `$826A` immediately before the first arena
route pointer at `$82CF` resolves to `$88E8`. Its imported program lasts exactly
2614 native ticks (43.4950 seconds), inclusive from fixed `$F7EE` consuming the
queued ID through the first NMI with active mask `$063E=0`. Title setup clears
`$034E`; fixed `$D92E` then contributes three calls to the `$E3FA` frame-yield
helper and `$DB25` takes its zero-state path with two more. Strict TFSX timing
therefore hard-stops any remaining opening program on native title frame 5.
Presentation ID 6 then queues on confirmed title frame 127, matching
fixed `$E477` after the title loop and before blue-menu root setup. Generic returns to
the menu do not restart ID 6. GAME MUSIC only gates future ID-5 queues;
accepting OFF does not preview, stop the current song, reject ID 6, or act as a
global mute. The current synth implements the requested pulse 1,
pulse 2, triangle, and noise channels with native pitch, duty, and envelope
state; DMC and cycle-level nonlinear NES APU mixing are outside this boundary.
Win32 uses a 44.1 kHz mono 16-bit `waveOut` ring of eight 1024-sample buffers.
Runtime does not flush that ring at either scene handoff, so a queued ID 7 or ID
6 can have up to 8192 already-submitted samples (185.8 ms) ahead of it. Device
and asset failures produce an explicit silent fallback.
The device-failure fallback deliberately freezes sequencer state; focused tests
use the same renderer as a deterministic advancing null sink. Missing, oversized,
malformed, or wrong-revision TMUS-1 data must never crash startup or fall back
to loose/private sources.
Track 8's pregame-matchup label is anchored independently by Bank06
`$A145-$A149` (`A9 08 20 0C C0`, FNV1a32 `1E564AC0`); importer and source-map
validation must retain that queue-site fingerprint in addition to the Bank04
track bytes.

Frontend cues are a separate strict same-pack boundary. The builder emits
`audio/frontend-sfx` as TFSX-1: 1792 bytes / FNV1a32 `985DC7ED`, containing
only Bank04 effects 8 and 10, three deduplicated voices, 75 periods, and 87
semantic instructions. The entry also carries the proven timing contract:
title setup hard-stops opening audio on frame 5 after the exact five-yield
path; fresh title confirmation frame 1 queues SFX 10; frames 1-126 retain the
confirmation animation; frame 127 queues track 6 and hands off; an accepted
Player 1 A-release in the blue menu queues SFX 8. START, directions, B/cancel,
PERIOD A+B, rejected chords, held A, and repeat frames must not queue it.
Revision spans include Bank03 `$8056-$8090`, Bank04's 32-byte SFX directory
and exact `$8B97-$8C29` records, fixed `$C003`, complete `$D92E-$D9A4` and
`$DB25-$DB87` routines, frame-yield helper `$E3FA-$E419`, `$C024`, `$CBAF`,
`$D768-$D792`, `$E477-$E4A0`, `$EC06-$EC25`, and `$F2F2-$F2F9`. Runtime
canonicalizes the pack identity and accepts exact size, header, metadata,
padding, payload fingerprint, and TMUS-1 from that same container only; there
is no loose or capture fallback. Run
`tools\Run-FrontendAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
-DecompRoot <LOCAL_DECOMP_ROOT>` for stable PCM, flow timing/input, malformed,
missing, oversized, cross-pack dependency, exact source-map, and isolated
frontend-importer ROM-mutation coverage.

Gameplay audio is connected to the native live scene. `audio/gameplay-sfx` is
TSFX-1: 2824 bytes / FNV1a32
`968A5DE6`, with seven Bank04 effects (IDs 3, 5, 6, 11, 12, 13, and 14), 14
deduplicated voices, 75 fixed periods, and 131 native semantic instructions.
The proven names are clock buzzer 3 (shot or period expiry), referee violation
cue 6 (bounded dynamic cutaway correlation), crowd response 11, side results
12/13, and countdown 14 (each game-second boundary below 0:12). ID 5 remains the neutral
`BANK05_9FEC_CUE`; do not rename it whistle, foul, collision, shot, rim, or
dunk without bounded runtime correlation. Its `$9FEC` caller restarts it after
violation/foul/period-reset flow only when GAME MUSIC is enabled; keep that
call-site condition separate from the neutral effect name. TSFX requests are last-write-wins
until the next audio tick. An active SFX channel overrides only the matching
music output channel; the music sequencer and its oscillator state continue
advancing underneath it, following fixed `$F3F2-$F436`.
Advertised TSFX request provenance is revision-locked to bounded spans at fixed
`$E7DB-$E7DF`, `$E863-$E867`, `$E86D-$E871` and Bank05 `$9FEC-$9FF0`,
`$AD01-$AD0E` (FNV1a32 `B7141C72`), and `$B1D1-$B1E6` (FNV1a32
`CFCD9759`); every source-map role must carry the corresponding FNV1a32 rather
than a one-byte or inferred offset. `$AD01` requests crowd response 11. `$B1D1`
then requests away-side 12 or home-side 13 only above 0:01, using the scoring
side captured before possession changes. Because the request mailbox is
last-write-wins, a qualifying result leaves 12/13 for the next audio tick; at
0:00 or 0:01 it leaves 11.

`audio/gameplay-dmc` is TDMC-1: 2515 bytes / FNV1a32 `AD70E6E8`. It deduplicates
the exact fixed-bank `$C080-$C280`, `$C440-$C710`, and `$C740-$CAF0` inclusive
sample pools and exposes five bounded, non-looping, non-IRQ clips at rates 14
or 15. Bank05 `$B5AB` is held-ball/dribble. `$A9C5` and both `$A8D6` clips
remain address-bound and unresolved. Bounded local slot-2 evidence observes
numeric variant 0, its cutaway, and a later `$A9C5` trigger at action frame 87,
but does not prove the clip's meaning or exclusivity. Slot 1 correlates numeric
variant 2 with the `$ABF5` action sequence at frame 34 without proving an
impact/rim meaning. The live variant-0 presentation currently requests the
address-bound A9C5 clip at frame 87; ABF5 is not yet queued. DMC
advances independently of music and tonal SFX; no trigger writes `$4011`.
GAME MUSIC gates future track 5 only,
and GAME SPEED has no path into audio cadence. `tecmo_gameplay_audio_stop_all`
models fixed `$EC06-$EC25` (32 bytes, FNV1a32 `F1BCC8E2`), called from
`$E58D`, `$E9A0`, `$E9DE`, and `$ECAF`, as the clear-all path for music, SFX,
and DMC. The scene invokes that reset once on entry to foul/violation
presentation and once when completed period settlement enters its banner or
score presentation, before any replacement request. A return to live play
requeues the gated Bank05 `$9FEC` cue and gameplay track 5. Because none of these
triggers or the clear path writes `$4011`, the DMC delta counter/DAC level must
survive retriggers, clip completion, and clear-all while only the sample reader
stops. Missing, oversized,
malformed, wrong-revision, or cross-pack TSFX/TDMC dependencies fail closed.
Run `tools\Run-GameplayAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
parser, source-map, PCM/state hash, override, cadence, gating, mailbox, DMC
independence and DAC continuity, corruption, missing/oversized/cross-pack, and
ROM-mutation checks.

The scene queues track 5 at launch and qualifying restarts only when GAME MUSIC
is enabled, and queues track 6 for halftime/final presentation. It maps clock
expiry to SFX 3, the late-clock countdown to 14, violations to 6, and moving
possession to the proven `$B5AB` held-ball/dribble DMC clip. A made dunk and
every resolved free throw, including a miss, follow `$AD01` crowd response 11
and then the qualifying `$B1D1` side result 12/13; 0:00 and 0:01 retain 11.
Enabled GAME MUSIC requeues track 5 when foul presentation enters the
free-throw sequence, not when the final attempt returns live. The ignored
bounded slot-3 observation begins setup at frame 10, requests gameplay track 5
at 26 and consumes it at 27, then changes the terminal result mailbox from
`$0B` to `$0D` at 280 and consumes it at 281. It is live by 300 with no new
music-track request or SFX ID 5 through 360. The final free-throw result
therefore remains in the SFX mailbox across the live return, which queues
neither track 5 nor `BANK05_9FEC_CUE`.
The supported slot-0 jump miss follows the clock-gated 11-then-12/13 mailbox
ordering without awarding points; zero-clock settlement retains 11. Layups
alone remain bounded to crowd response 11 pending separate caller
integration. `BANK05_9FEC_CUE` remains neutral and is gated at the
bounded violation, direct-foul, and period restart boundaries. Dunk action
frame 87 requests address-bound A9C5. ABF5 and address-named A8D6 clips stay
imported without invented live use. The deterministic state-`$15` diagnostic
requests A8D6-short only at its proven nonterminal pass repeats.

Finale provenance is the raw Bank04 chain `$851C` wait 50 -> `$83EA` wait 30
-> `$852E` wait 0 -> `$83AE` wait 75 -> `$8310` wait 1 -> `$FFFF`, loading
screens `$1C`, `$20`, `$1F`, `$22`, and `$2D`. The selector-2 transition uses
first seed `$78`, second seed `$D8`, and delta `-8`: the swap holds the last
emitted `$E8`, and the outward pass begins at `$D0`. The native state model
starts from 742 imported route-core frames plus 156 dispatch-wait frames, or
898. Five explicit one-frame asynchronous load gates reach 903, and six
selector black/fade normalization frames preserve the exact `$E8` hold, `$D0`
outward start, and `$10` endpoint before the persistent hold begins at native
frame 909. The ROM's `$8A48` and `$850C` state gates are conditional; these 11
native scheduling frames are not claimed as ROM-exact scheduler wait durations.

Goal Y reproduces Bank07 `$D861` bytewise using Bank04's first `$8988` emit
pass (`$07EC/$21` stream1). For raw negative relative Y bytes (`dy - $40` in
`$C0-$FF`), D861 converts the byte to a magnitude and subtracts it from the
stream low byte. On borrow it decrements the page, then falls through with the
subtraction result still in A, adds the stream low byte a second time, and
repairs the page on carry. Non-negative bytes use the normal low-byte add and
carry. Page `$00` is admitted; page `$FF` is admitted only for low bytes
`$F0-$FF`; every other page is rejected before OAM Y is narrowed. ROM-exact
visible goal records are frame 240=0, 260=5, 276=10, 280=10, 292=15, 300=15,
and 308=16; the final remains 16. Jumbotron projection and the TASG-2 masked
connector overlay are unchanged.

The 51-row TATL source mapping is correct, but its runtime composition is not
a uniform scrolled stack. Rows `0..37` use the global `$0301` scroll and are
clipped below the arena IRQ restart. Rows `38..50` are the independent lower
large-crowd band: its logical screen origin is `motion_counter_88 + $7B`, its
first complete scanline/clip is `motion_counter_88 + $7C`, and the viewport is
cleared/restarted from that clip before the lower rows draw. Relative to the
old uniform renderer, the correction evolves through `+5`, `-3`, `-11`, and
`-15` native pixels at the `$50/$6A`, `$58/$5A`, `$60/$4A`, and `$64/$42`
scroll/motion checkpoints. The final `-15` is therefore an IRQ-composition
result, not a constant offset. This registers the centered gray post with the
black opening and cream/red pedestal carried by the lower band.

For screen `$18` research, use the verified ROM route rather than capture bytes:

- Bank04 arena entry starts at `$88E8`; `$88E7` is the preceding `RTS`.
- Fixed screen descriptor `$DD2D-$DD33` selects the Bank00 compressed stream and background palette.
- The fixed `$D9F6` decoder emits exactly two complete 1 KiB nametable pages.
- Backreferences subtract their distance from the source cursor before advancing past the distance word.
- Lower arena CHR selectors come from the fixed IRQ tables at `$FD7C/$FD80`; similarly valued Bank01 bytes are not the runtime source.

### Native gameplay state and scene boundary

`include/tecmo_gameplay_state.h` and `src/tecmo_gameplay_state.c` provide a
deterministic pure-state rules boundary. `src/tecmo_gameplay_scene.c` now drives
it for preseason and season gameplay, renders strict court/pose assets, dispatches
audio events, and returns final results. Proven timing/state anchors are fixed
`$E59B->$E823` for unconditional regulation-clock preparation,
`$E601-$E60F` for the tied-OT-only duration overwrite, `$E80F-$E81E` for the exact
31-update expiry wait, `$E7D0-$E822` for zero-clock live-action settlement,
`$E6ED/$E6FF` plus `$E765-$E76F` for post-banner resets, and
`$EA14-$EA2F`/`$D2B9-$D2CE` for the two-controller NES A-release gate.
Halftime/final score dismissal comes from Bank06 `$BC3C-$BCF9`. State timing,
event ordering, foul limits, explicit free-throw result accounting, and numeric
close-shot step tables are evidence-derived. Free-throw launch ownership now
follows Bank05's human state-20 gate: only the controller assigned to the
scoring team can launch, and only while that pad's current NES B level is held.
The other pad, input edges/releases, A, directions, START, and SELECT are inert,
and a human side has no timer fallback. An unassigned scoring side uses a
deterministic 125-update schedule from the bounded slot-3 trace's inclusive CPU
state-18-to-launch span (frames 22 through 146). Bank05 `$96B6-$9708` selects
command offsets `$007D/$00D7`, and Bank06 `$8B8E-$8B9D` maps those from base
`$9F2E` to stream/dispatch pointers; those values are not frame timers. The
native scene does not yet implement that positioning/script system. Its
per-attempt observed-schedule counter resets at launch, after each attempt, and
when the scene ends. The exact ordinary-jump evidence boundary now includes the
human away/right family-0/profile-0/direction-1 miss slot: current-level NES B
release, actor states `$0C/$0D/$0E/0`, Bank05 Q8.8
height/velocity seed `$02E8`, gravity, frame-40 clamp, recovery through frame
46, independent ball routes through frame
87, the conditional frame-75 `$B5AB` DMC, and post-shot settlement mailbox
ordering. The same controlled family/profile/direction context also admits one
captured deterministic three-point make. Current NES B remains held through
frames 1-8 and releases at 9; the already-selected entry pose 325 remains for
frames 1-4, followed by 1060 for 5-8, 1061 at 9, 213 through flight, and 469
after recovery. TGSR-4 classifies
the terminal clear-bit result as MAKE at frame 19. Uninterrupted Q8.8 motion
starts at frame 20 with velocity `$0308` and gravity `$0028`, lands at native
frame 57, recovers through 62, and becomes neutral at 63. The emulator displayed
landing/recovery at frames 59-65 because unrelated main-loop overruns held
frames 38 and 53; native code does not reproduce those renderer stalls. Score
and shot-clock reset remain the separately observed frame-85 checkpoint, while
frame 111 hands possession over and queues crowd 11 only. The make ball arc
remains a native approximation while its world endpoint and TGCP camera
projection are now production-wired. Live scene policy reuses and horizontally
mirrors this one numeric route for either manually controlled team. Ordinary
live jumps therefore select the shooting lab default's
family-0/profile-0/direction-1 TGJS base; physical hoop resolution still owns
actor facing and the ball target.
At launch it transactionally verifies actor/possession/TGOR coherence, resolves
the tracked offensive hoop, faces the actor toward it, and freezes that endpoint;
this two-basket facing adapter is native approximation, not evidence for another
TGJS direction. An earlier B release is normalized to the
captured frame-9 transition so normal input cannot strand the scene; no
earlier-release ROM timing is claimed. If the period expires before frame 111,
the frame-85 score is applied exactly once without an invalid shot-clock reset,
then the settled action retains the shooting side/holder for the normal period
banner transition. TGSR-4 classifies TGJS's bit-7-set terminal flag as MISS and proves
the non-current, other-team claimant handler/possession decision. Native play
applies that one decision at frame 87, awards zero points, uses an explicitly
approximate opposing actor, and queues crowd 11 followed by clock-gated side
result 12/13. At period expiry it retains the current side and crowd 11.

For ordinary-shot pose work, preserve the distinction proved by Bank05
`$83E9-$842B` and `$8469-$847A`: state `$1E` retains the actor's existing pose
through its first `30/20/10/00` cycle, then walks the facing-indexed
`$0846..$0854` table before `$842C` selects `$01AA`. The bounded make begins at
`$028A`; the bounded miss begins at `$030A`; neither value is a universal
gather-pose selector. The current live miss path layers a pose-only counter over
the shorter exact `$0C/$0D/$0E` route: actual entry pose for held ticks 1-4,
1060 for 5-8, 1061 on release, and 213 on the following update. Early release
is intentionally compact, and horizontal mirroring remains native policy. Do
not describe that composition as a complete ROM-exact state-`$1E/$0B` miss
timeline, and do not add ASM/capture files as runtime inputs.
For live ordinary shots, Bank05 `$8B12` proves family 0 as the reset state;
family 1 remains unavailable until `$8B83-$8BC8`'s full hoop, defender,
defender-side, and raw `$006A<$9C` gate has a retained native owner. Never
substitute a stable/frame hash for that raw gate. A terminal miss with no actor
inside the strict claimant envelope may use the explicitly native generic
opposing-team handoff so play does not freeze, but it must not emit a B87C
claimant trace or be labeled rebound/steal parity. A TGJS-owned shot pose clears
the retained pre-tip orientation-encoded flag before rendering its launch-facing
mirror.

Post-handoff live actor layout, CPU locomotion/AI, pre-tip jump/ball
interpolation and original tip-claim/tie settlement, unsupported jump
directions/profiles and outcomes, ordinary two-point makes, the longer +157-update claimant route,
semantic rebounds/blocks/steals, general make/contact rules, the distance policy
selecting dunk/variant 0 versus layup/variant 2, live close-shot
profile/direction selection and left-facing render
mirroring, state-dependent palette transitions outside the exact live-court
and cutaway contexts, foul detection, live
free-throw camera/full-court projection and lineup
integration/aim/outcome/rebound and CPU
positioning/script behavior, plus the HUD's fixed-column and unassigned-CPU
actor-selection adapters are explicit native approximations. HUD typography is
not: THUD-1 owns the exact Bank01 team marks, Bank02 character map, and Bank02
initial/surname formatter.

Live player palette selection is no longer part of that approximation list.
Bank02 `$A8AE-$A8C9` rotates selected roster profile byte 2 bit 7 into
`$04B0` bit 0, the second side adds bit 1, and fixed `$F1F2-$F24C` passes
`$04B0 & 3` into the `$D413` OAM compositor. TGCT-1 retains fixed
`$F2E2-$F2F1`'s four live-court profile/side palettes. Fixed
`$DEAB-$DEDF` chooses first-side `$30`, Lakers first-side `$38`, or the
29-entry second-side table `$DC19-$DC35`; native rendering substitutes those
colors at live-palette entries 3/7/11/15 for the court, ball, and actors.
Bank01 `$B0ED-$B133` and `$B138/$B148` remain the separate exact pose/cutaway
palette recipe, including offsets 6/7/9 and the light variant at 13. TGDK uses
that bounded recipe. The scene's fixed slots 0..4 lineup selection remains
native policy; that does not make starter selection exact.

Every gameplay launch now enters strict `gameplay/pre-tip` TPTI-1 before live
updates. TPTI-1 is 5888 bytes / FNV1a32 `99ADFE3D`, has 20 exact Rev1 source
spans, and requires same-pack TGPL-1, TTDT-1, TMUS-1, TWAR-1, and `chr/all`.
The first `61/121/61` card waits, Bank06 character mapping, `$AF05` 2-by-2
metatile glyphs, `$C6/$FA` text CHR selectors, and the 16-pixel cell positions
are ASM-exact. The later phase durations are capture-bounded. The asset-backed
cards and close-up reach center setup after `61/121/61/208 = 451` updates. A
successful Bank04-clocked tip claims state `$17` during court/ball descent,
immediately enters the 60-update toss cut-in, returns to the 30-update court
contest, and then hands off live; the deterministic primary human capture
reaches those boundaries at frames `516/576/606`. The independent CPU-only
threshold route still reaches `508/568/598`; that is not a human capture-clock
calibration. The phases are mode card, matchup,
first-period card, screen-`$1A` close-up, center black, court/ball descent,
toss transition/cut-in, contest, then live. Matchup logos are anchored
at `(16,32)` and `(16,128)`. The court overlay draws only one away-team ROM
logo and right/bottom-aligns it from validated dimensions; do not add separate
city/nickname text because that lettering is already in the logo cells.
Ball descent interpolates Y 71..145 for the first 60 updates, then holds.
The toss cut-in uses TGPL screen `$1B` nametable page 1; page 0 is the opposite
phase, while page 1 matches the bounded ball X 176..239 and hand X 67..159
geometry.

Bank06 `$A10A-$A124` permits current-level NES B cancellation only when `$69`
bit 0 is set. Native PRESEASON clears that gate and ignores B on all three
cards; the regular-season route sets it and either pad may cancel. Exact
Bank05 `$985E-$986A` proves a current-level NES B mask/read/latch in the tip
machinery, but not controller/team ownership or winner settlement. Bank04
`$86E1-$8817` owns the countdown clock: it samples `$6A`, seeds
`$8A = ($6A & $3F) + $82`, polls both B levels in the `$871D` loop, increments
the byte at `$8788`, and at wrap derives the captured error with the wrapped
`abs($F9-captured)` / `$0B` cap at `$8795-$87D0`. The native bridge retains the
sampled `$6A`, evolving `$8A`, source-loop ticks, and each captured byte; it
does not reinterpret presentation frame zero as `$F9` and no longer injects a
terminal `$F4` entry. Fixed `$CD96-$CDAB` supplies the exact 8-bit `$6A` mixer.
The deterministic presentation bridge evolves it once per pre-seed update from
the asset-owned `$6A=$00/$53=$5A` state. After 243 card updates plus Bank04's
seven route-setup yields it samples `$6A=$85` and therefore seeds `$8A=$87`.
The traced scheduler then consumes 20 prepare yields, two yields per ordinary
poll byte, and one extra yield at each `$F6/$F9` marker. The primary away pulse
captures `$E1`, derives capped error/countdown 11, and reaches the state-$17
cutaway at total frame 516 through the ordinary ball-height/countdown gate.
Byte wrap ends B capture rather than manufacturing post-wrap input samples.
Fixed vector `$C000` jumps to `$E3FA` and is the task/visible-frame yield used
by the two `$871D` polls and `$877D` marker wait. `$8818` only stages the next
object tuple, while `$C054` jumps to `$D2D2` for OAM cleanup/finalization and
does not add a yield. Screen `$1A` is pointer-table route 2 at `$86D0`, after
routes 0/1 at `$88E8/$8483`; this is why neither `$8818` nor `$C054` may be
counted as an extra scheduler tick.
The ignored narrow Rev1 trace establishes the scheduler cadence and observed
one launch history sampling `$6A=$A1` / seeding `$8A=$A3`; it does not prove
that every menu and controller history enters screen `$1A` with the same
source byte. The port's `$85/$87` visible mapping is therefore an explicit,
deterministic bridge from its asset-owned initial state and presentation
update schedule, while the byte mixer, seed arithmetic, yields, marker waits,
wrap, and error derivation are exact source behavior.
Lower error wins; equal captured errors
defer the native claim because the original single-winner tie settlement remains
unproven. B cannot sample a tip in the close-up or toss phases and cannot cancel
those phases. Winner queries fail closed before `JUMP_CONTEST` without changing
caller-owned output. Other inputs are inert throughout the presentation. Rules,
clock, shot clock, camera, and live updates stay frozen until the eventual
handoff. Track 8 queues at entry and enabled GAME MUSIC queues track 5 only at
handoff. Bank04 `$88` plus fixed `$D861` moves the OAM player left while the
nametable figures scroll right; the 33-frame phase anchor is capture-bounded,
but the per-step projection and OAM-Y semantics are ROM-exact. Bank04
`$AC8C-$ACD9` initializes object slots 0..10 from state `$AD82`, sprite-slot
base `$AD8D`, facing `$AD98`, X-low `$ADA3`, X-high `$ADAE`, Y `$ADB9`, and
facing-indexed pose tables `$ADC4/$ADCD`. TPTI-1 retains those bytes across
its close-up-control/timing spans, and
`tecmo_gameplay_pretip_tip_lineup` transactionally exposes the complete setup
for ten players plus the ball in canonical TGCT space. The scene consumes the
exact coordinate, sprite-slot base, and facing-selected pose without a second
horizontal mirror. Raw object-state behavior, jump interpolation, the
capture-bounded ball descent, and exact original claim/tie settlement are not
implied. Equal captured errors defer the native claim because the original
single-winner tie settlement remains unproven. State updates validate phase bounds, total-frame
coherence, sample/error/sample-frame coherence, terminal flags, and overflow
before committing a candidate state; rejection must leave the caller state
unchanged. Run
`tools\Run-GameplayPreTipTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for the
strict parser, dependency/provenance/mutation coverage, state/input/audio
schedule, and deterministic render checkpoints. When the local capture exists,
the script also creates an ignored five-row reference/native comparison sheet,
including the Bulls/Pacers tip palette checkpoint, and requires the visible
`1ST PERIOD` mask to match exactly. Ignored PNGs and
reference frames remain local evidence only.

TGCS stores 208 exact profile/direction resolutions into TGPL pose data, but the
live scene currently selects only profile 0/direction 0 and mirrors
actor-facing-left; the asset breadth must not be read as proof of that narrower
live policy.
`gameplay/dunk-cutaway` is the strict 20272-byte TGDK-1 payload (FNV1a32
`E02F2D21`). It resolves screen `$0B`'s two D9F6 nametable pages, exact palette
recipe, both side-specific sprite streams, seven stage anchors, CHR selectors,
and OAM-priority order from revision-fingerprinted PRG spans plus same-pack
`chr/all`. The visible ROM schedule is live 1-22, dispatch 23, black 24-27,
cutaway 28-62, black/rebuild 63-70, live return 71, A9C5 at 87, and action
settlement at 132. Stage 0 is assigned at 27 and first visible at 28; later
assignments at 32/37/42/47/52/57 are visible on those captured frames. Frame 63
is black even though the last staged OAM remains, and frame 64 clears it.
Profile 1 with uniform `$30` remains the standalone exact bounded checkpoint.
Production selection is now exact for the scene's currently bound roster slot:
profile byte 2 bit 7 selects the group and fixed `$DEAB-$DEDF` plus
`$DC19-$DC35` select the first/second-side uniform color.
The imported TGCT live palette, its matchup substitutions, and embedded FCEUX
RGB profile are exact; that does not imply frame-identical palette transitions
outside the covered live-court and cutaway contexts. The high-level
mapping is proven as variant 0 = dunk and variant 2 = layup; low-level TGCS
APIs and fields retain those numeric ROM identities. The local save states,
FCEUX traces, and screenshots used for correlation remain ignored verification
material, not committed provenance or runtime input. See
`docs/gameplay-state-foundation.md`; verify state with
`tecmo_port.exe --gameplay-state-test` and the compound scene with
`tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

The scene must obtain TGPL-1 `gameplay/core`, TGCT-1 `gameplay/court`, TGCP-2
`gameplay/camera-projection`, TGMO-1 `gameplay/movement`, TGAI-2
`gameplay/cpu-steering`, TGOR-1
`gameplay/court-orientation`, THUD-1 `gameplay/hud`, TGCS-1
`gameplay/close-shots`, TGDK-1 `gameplay/dunk-cutaway`,
TGJS-2 `gameplay/jump-shots` (2776 bytes,
`A66EE873`), TGSR-4 `gameplay/shot-resolution` (608 bytes, `5376E82B`),
TMUS-1 `audio/music`, TSFX-1
`audio/gameplay-sfx`, TDMC-1 `audio/gameplay-dmc`, and `chr/all` from the same
explicit pack. Exact-size reads, canonical fingerprints, deep bounds/reserved
checks, CHR revision fingerprints, the music asset's selected pack path, and
source-map provenance fail closed before the scene is marked available. Drawing
preflights every court cell and actor/ball pose so a rejected frame leaves the
destination untouched.

THUD-1 is 864 bytes / FNV1a32 `3D13AA89` and requires exact same-pack TGPL-1,
TTDT-1, and `chr/all`. Its three exact Rev1 spans are Bank01 `$BDF0-$BE1E`,
Bank01 `$BE1F-$BECC`, and Bank02 `$AF64-$B07B`. Preserve the exact 29x5 team
marks, `$2041/$2057` destinations, `$20-$5A` character map, and
initial-dot-nine-surname formatter. Every character tile must match TTDT-1's
strict `$FA` CHR record. The scene owns the fixed two-row overlay and must
preflight it before modifying the framebuffer; the score, clock, jersey numbers,
and selected-player labels must not move with TGCP. The two complete HUD rows,
non-team blank columns, colon `$16`, black backing, and live `$FA` top-row binding are reference-bounded
presentation facts, not decoded placement routines. Three-digit score capping
and the holder/shooter matchup fallback for an unassigned CPU side are native
adapter policies. Do not call those policies ROM-exact.

TGAI-2 is a production scene dependency for bounded ordinary CPU movement.
The scene owns a fixed opposing roster-slot link, explicit target result,
snapshot fingerprint, and decision serial. The link remains matchup/pose and
defender-reference metadata; non-holder movement consumes scene-owned explicit
formation/marking coordinates. A defender goal-side target outside the shaped
court uses the equal 32-pixel court-side offset before final validation. An
explicit no-command sentinel keeps the still-unported ROM command/advance
lifecycle out of the exactness claim.

TGSR-4 is 608 bytes (FNV1a32 `5376E82B`, FNV1a64
`FACCE42B52382D6B`) and revision-locks Bank05 `$91BC-$943A`, `$A6EE-$A9D9`,
`$B73E-$B87B`, `$B87C-$B8F5`, `$BA56-$BA9C` (`B779AC48`,
`367ED7AC43F1ACA8`), `$9042-$9053` (`CE6C9466`,
`EC5906B34DC6D566`), and `$B98B-$B994` (`404311FE`,
`7CCF6AAD4241C4FE`) as explicit source records. The full `$BA56-$BA9C`
caller span intentionally includes its incoming predicate; the older
`35FB80C4` value described only the narrower `$BA65-$BA9C` subrange and is
not a source fingerprint. TGSR-4 requires exact same-pack TGPL-1;
missing, malformed, wrong-sized, wrong-revision, and cross-pack dependencies
reject the scene before availability. Its safe semantics are terminal outcome
polarity, numeric rim routes, claimant thresholds, and handler/possession
decisions; it does not label rebounds, blocks, steals, or generic makes.

TGSR-4 additionally carries the exact 124-byte Bank05 `$BEEF-$BF6A`
three-point arc boundary table (FNV1a32 `9EF1061B`, FNV1a64
`E8A0728513DD8BDB`). Its pure C API reproduces `$B995`'s free-throw/field-goal/
three-point classification for raw world X/Y, orientation 0/1, and shot-flag
low bits with the original low-byte subtraction and high-byte borrow. The
classifier routine itself remains in same-pack TGPL-1's existing
`$B995-$BA3F` source span; TGSR does not duplicate those bytes. This is a
rules foundation only. TGJS-2 strictly translates
`$AD4E->$B32C->$B100` when raw launch position, target, altitude, and context
are supplied explicitly, including 16-bit wrap and the zero-count fail-closed
case. Live ordinary two-point makes remain rejected because exact `$AD6E`
launch inputs are not owned. The supported three-point route derives its
frame-111 handoff from `$AC0A-$AC6E`'s 26-update state-08 timer after frame 85.
The exact `$91BC` evaluator remains documentation-only because the live scene
lacks its TTDT condition, matchup, motion, and RNG inputs.

TGSR-4 uses metadata bytes 29..63 for the strict state-`$15` rim-rattle
contract. It imports orientation starts `$009D/$0263`, Y `$93`, velocity
magnitude `$0040`, altitude `$38`, timer 4, pass derivation
`(($53 & 3) + 1) << 4` with the low nibble preserved, repeat DMC length `$0A`,
render-script selection addresses
`$BAB9,$BAB9,$BABF,$BAC5,$BACB,$BACB,$BAD1,$BAD7`, and exits
`$BADD/$BB01`. These addresses are not literal sprite or CHR IDs. The state
API saves the incoming velocity, moves one coordinate per update for four
updates per pass, reverses on each nonterminal pass, requests the existing
address-bound A8D6-short DMC clip, and restores velocity on completion.
Focused provenance adds `$A2DF-$A2F7` (`9D918043`), `$AD4E-$AD64`
(`AF1D6B17`), `$BDF3-$BDF6` (`79F66DB3`), and `$BEEF-$BF6A`
(`9EF1061B`), for seven primary plus four focused source spans. The debug mapper
reads the X launch target from the
required same-pack TGCS-1 `$BDEF-$BDF6` source and cross-checks its snap bytes
against TGSR-4; `$AD4E-$AD64` proves the BDEF/BDF1 loads and Y target `$8F`.
`$A2DF` does not universally enter state `$10`: nonzero `$036F`
or raw `$6A >= $18` selects that path; otherwise the original relaunches state
`$05`. Only the deterministic `gameplay-jump-rattle-frameN` debug route uses
the observed `$6A=$71` and a sign-only negative diagnostic sentinel to produce
the visible positive-first four passes before the state-`$10` handoff at frame
89 and settlement at frame 103. The incoming horizontal sign is proven;
its exact magnitude is not. The generic state API preserves and restores
whatever horizontal and vertical values its caller supplies. The raw
orientation-0 snap `(157,147)` is rendered relative to the ROM launch target
`(160,143)` and the native shot endpoint, so the ball remains beside the native
hoop. Normal live misses retain their existing 87-frame path and do not use
invented selection or RNG.

TPNL-1 `gameplay/penalties` is a separate strict 768-byte rules foundation
(FNV1a32 `980DDC76`) with same-pack TGPL-1 and TSFX-1 dependencies. Its pure
classification/presentation APIs do not infer collision or route state. The
live scene consumes TPNL only through the bounded human defensive-B bridge in
`scene_try_defense_action`: it requires the selected primary/defender pair,
uses the B05 `$9968` raw coordinate envelope, and applies the native
ordinary-fallthrough adapter profile `$07E3=0`, `$0478=$19`, `$05A8=0`.
The scene does not retain those raw bytes, so this is not a general original
collision/route reconstruction. Special routes and spontaneous CPU fouls fail
closed; see `docs/live-foul-asm-parity.json` before describing live fouls as
ROM-derived. For that one accepted ordinary defensive-pushing result, the
scene transactionally retains only typed actor/team/class/counter/attempt
identity after the state request succeeds. It maps Bank02 `$B0F8-$B398`'s
source-backed class/type/name/number/counter/fouled-out cells through existing
THUD/TTDT font and CHR data; `$B373-$B398` bonus remains a side-mask fact with
no invented visible BONUS string.

TGVR-1 `gameplay/violation-referee` is the strict 4752-byte visual companion
(FNV1a32 `2EB08CF0`) and requires exact same-pack `chr/all` plus TPNL-1. It
retains screen `$05`, all seven mapped violation messages, palettes, Bank04
metasprites, and selector-specific sequences. Each `$B33F` piece is one 8x8
CHR cell; preserve tile bit zero and do not synthesize an 8x16 partner tile.
Shot clock is groups
`9,10,10,10`; out of bounds is `3,4,5,5,5`. The 44-frame controller and
four-frame group cadence are exact. The nine-frame blackout/fade alignment
remains capture-bounded. The scene consumes strict TPNL-1 presentation metadata
to request shared SFX 6 at presentation frame 16 exactly once. This bounded cue
seam is native-faithful. The fixed ordinary defensive-pushing cutaway also
retains `$E95E`'s proven `$2C` then `$22` order, the TGVR fade/selector-0
groups `1,2,2,2`, and court actor/ball suppression; Bank02 dynamic PPU
completion timing remains unestablished. Other full presentation and original
caller-order parity remain incomplete. Missing, malformed, wrong-sized, stale, or
cross-pack data must fail closed.

`gameplay-out-of-bounds-frameN` is the visible integration checkpoint. It must
drive the live holder through TGMO's page-0 boundary, consume TPNL selector 1,
and then draw TGVR's `OUT OF BOUNDS` screen; do not replace that production
path with direct phase injection. Frames 23, 27, and 31 are distinct referee
groups 3, 4, and 5, while frames 31, 39, and 80 retain group 5.

TGBC-1 `gameplay/backcourt` is the independent strict 512-byte live detector
(FNV1a32 `810886EF`). It retains Bank05 `$970B-$9786` (`C137674F`) behind the
exact Rev1 fingerprint and requires same-pack TGOR-1 and TPNL-1. The ported
span is `$971F-$9786`: `$0478` must be zero, `$0588` bit 4 is the frontcourt
latch, and selector `$0742=2` is BACKCOURT. Orientation 0 establishes at ball
X `<=375` and calls the return at X `>=386`; orientation 1 establishes at X
`>=392` and calls it at X `<=383`. Preserve the original 16-bit subtract,
high-byte sign test, and low-byte compare rather than replacing them with an
unbounded distance heuristic. The preceding selector-4 ten-second test at
`$970B-$971E` is evidence only and remains unported.

The scene samples the attached held ball once after ordinary human and CPU
movement, resets its latch on each scene possession handoff, resolves selector
2 through TPNL, and renders TGVR sequence `3,4,5,5,5` with the ROM `BACKCOURT`
message. That scene scheduling/reset adapter is the best bounded integration;
exact 6502 intra-frame caller ordering is not claimed. TGVR's group controller
is exact, while the existing nine-frame blackout/fade alignment remains
capture-bounded. `gameplay-backcourt-frameN` must reach the production path,
not inject a violation; frames 23, 27, and 31 visibly prove groups 3, 4, and 5.

TGFL-1 `gameplay/free-throw-lineup` is a strict 1216-byte pure lineup
foundation (FNV1a32 `B17B9A3F`) with an exact same-pack TGPL-1 dependency.
It stores the complete Rev1 Bank06 spans `$88B0-$88D9` (`AD834719`),
`$9621-$976E` (`998D84B8`), `$976F-$985C` (`FB7680EF`), and
`$985D-$9918` (`AFB31306`) behind exact source records, zero-reserved fields,
the full-ROM revision fingerprint, and sanitized source-map provenance. Its
pure API accepts orientation 0/1 plus explicit, distinct shooter and secondary
slots and returns unclamped raw world coordinates, directions, state seeds,
raw even pose offsets, and TGPL pointer indexes `offset/2` (`517..520`) for
the nine non-shooters. Both descending streams skip the shooter without
consuming an item. The shooter's raw position/direction/state seed is exact,
but its pose is preserved/undefined because `$976F-$985C` does not call
`$88B0` for that slot. The base API accepts no side-control flags and therefore
does not invent the conditional shooter script override or secondary raw phase
`$15`.

`TecmoGameplayScene` now loads TGFL-1 as a strict same-pack dependency. On
free-throw entry it uses the scoring-team possession synchronized through
TGOR-1 to select orientation 0/1, copies all ten exact raw X/Y values into the
canonical court positions and anchors, and performs one typed TGCP-2 settle on
the shooter coordinate (`camera_x` `$0066/$0198`). The camera then remains
frozen during the sequence and rendering consumes one coherent TGCT/TGCP
frame. Shooter selection (current scoring holder, otherwise first scoring
actor), secondary selection (assigned opposing actor, otherwise first opposing
actor), held-ball attachment, and camera composition are native adapter policy,
not additional ROM claims. Existing actor poses are preserved: do not claim
that the live scene applies TGFL nonshooter pose/state fields or the omitted
side-control script overrides. Aim, attempt decrement/outcome policy, rebound,
and CPU positioning/scripts remain approximate or unsupported. Missing,
malformed, undersized/oversized, wrong-revision, and cross-pack data fails
closed. Verify it with
`tools\Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

TGOR-1 `gameplay/court-orientation` is the strict 640-byte live ownership
foundation (FNV1a32 `44B0C44E`) and requires exact same-pack TGPL-1
(`2047CCE0`) and TGSR-4 (`5376E82B`). It preserves Bank05
`$8FAD-$8FE7` (`7C94E5EA`) as the possession transition gate-and-swap,
`$9042-$9053` (`CE6C9466`) as the exact slots-0..9 `$04B0` bit-`$10`
toggle plus queue-`$17` operation, `$9054-$90AF` (`FE092D62`) as the
absolute target-delta routine, and `$BDEF-$BDF2` (`A27B0F6F`) as X targets
`$00A0/$0260`. Do not relabel `$9042` as a general team switch.

The live state owns current and previous binary offensive direction, tracked
possession team, transition serial, and target X. A fresh native launch uses
direction 0 with the existing initial AWAY possession. This is a
cold-start-aligned policy; repeat-game initialization in the original remains
unproven. A same-possession handoff or period/foul restart is a successful
no-op. A real possession change atomically saves the current direction, XORs
it, updates the tracked team/target, and increments the serial; invalid input
leaves output unchanged. Scene handoff always synchronizes TGOR even when the
rules state changed possession first. TGCT-1 remains the unchanged canonical
left-to-right court.

TGPL-1 fixed `$E537-$E548` derives presentation selector `$0758` from
`$04FC` bit 7 (slot-10 horizontal-velocity high byte/sign), with screen IDs
`$1B/$2E` at `$E699`; it is cross-check evidence, not orientation ownership.
TGSR-4 `$B87C-$B8F5` is a conditional alternate claimant-settlement path, not
a universal post-shot path. `$035B` is only observed as save-before-toggle and
has no direct reads; the only direct `$035A` stores are `$8FC4` and `$B8E0`.
The broad `STA $0300,X` initializer appears only at fixed-bank cold boot
`$CC68`. TGOR now supplies production TGCP follow direction and
`$00A0/$0260` world-space shot targets; launch Y is the separately proven
`$8F`. A live shot accepts only the active possession holder whose team matches
both rules possession and TGOR's tracked team, then derives facing from that
validated hoop instead of the actor's previous movement direction. TGOR also
selects the production TGFL-1 lineup orientation. Verify the
strict parser, source mutations, transitions, scene handoff/restart
integration, and provenance with
`tools\Run-GameplayCourtOrientationTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

TGCP-2 `gameplay/camera-projection` is the strict 1536-byte gameplay camera
foundation and live dependency (FNV1a32 `53247856`) and requires exact
same-pack TGPL-1
(`2047CCE0`) and TGCT-1 (`ECAB7A93`). It preserves fixed-bank Rev1 spans
`$DE13-$DE2C` (`A5CF7665`), `$DF05-$DFFF` (`7BC5351D`),
`$E0E7-$E13B` (`7FE800D4`), `$E168-$E2E6` (`19038AEA`),
`$EB4F-$EB8C` (`AF5725C0`), `$F1CB-$F1F1` (`CB8BD081`), and the actor
movement clamp `$F106-$F1B0` (`CB1D4EAF`, SHA-256
`0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6`)
behind strict source records, zero padding/reserved bytes, full-ROM
fingerprints, and sanitized source-map provenance.

The pure API initializes camera X `$0100`, scroll/page zero, direction zero,
and layout cursor `$20`; reproduces threshold selection, bounded horizontal
following, page toggles, and direction-aware coarse-column cursor updates;
offers a transactional bounded forced settle; and projects actors with
`screen_x = world_x - camera_x` only when the subtraction high byte is zero
and `screen_y = max(0, world_y - altitude)`. Invalid input does not mutate
state or output. A valid offscreen projection returns the deterministic API
sentinel `visible=false, screen_x=0, screen_y=0`; the ROM branches before its Y
calculation, so zero is a native safety value rather than a claimed ROM write.

Production launch preserves the pure `$DE13` cursor `$20`, performs the
bounded `$DDFB->$DF05` first-column prime to cursor `$21`, seeds every
actor/anchor and ball coordinate in world space at camera `$0100`, then settles
once after seeding. Each subsequent live scene update performs exactly one
route-0 follow after all actor and ball mutations, using ball world X and TGOR
direction. Free-throw entry performs the documented TGFL-driven typed settle;
subsequent free-throw updates and TGDK black/cutaway frames freeze camera state.
The first live-return update resumes it. A possession transition
clears only threshold validity and the endpoint latch, never camera position.
Production state validation additionally requires scroll/page consistency and
the reachable direction/cursor relation: right cursor is
`min((camera_x >> 3) + 1, $34)` and left cursor is
`max((camera_x >> 3) - 1, $0B)`. A valid threshold latch must carry one of the
three exact ROM-derived threshold pairs. The weaker pure-state validator remains
available only so focused synthetic carry/borrow and direction-reversal tests
can construct intermediate states; the live scene never accepts those states.

`src/tecmo_gameplay_free_throw_projection_test.c` remains an independent
test-only composition rather than the production scene adapter. It loads
TGFL-1 and TGCP-2 from the same pack, derives orientation
1/shooter 6/secondary 1, and proves the bounded
slot-3 checkpoint: capture-derived cursor `$21`, 76 moving camera updates,
an unchanged 77th update, transactional settle at camera `$0198`, and six
visible/four neutral-offscreen actors with TGFL-derived X/Y. Secondary slot 1
and cursor `$21` are bounded frame evidence; the remaining lineup coordinates
come from TGFL-1. Pure TGCP tests separately cover exact seven-pixel left/right
steps, disabled and routes `$01/$12/$13` no-ops, page carry/borrow, continuing
coarse-column updates, and three-column direction reversals.

TGCT-1 now exposes that strict pure court boundary without changing its
6559-byte payload or canonical `ECAB7A93` fingerprint. Its raw 15-by-48
little-endian macro layout expands to caller-owned 96-by-30 tile and palette
planes (768-by-240 pixels), with all 720 macro references checked. The exact
world fingerprints are `6458B5E5` for tiles and `7F650645` for per-tile
palette indexes; the full layout uses indexes 0..360 with 346 unique values.
Decode also proves that camera X `$0100` reproduces TGCT-1's existing 32-by-30
center tiles and its expanded attribute palettes row by row.

The pure slicer accepts camera X 0..`$0200`, derives coarse tile and fine
scroll, and returns a fixed 33-by-30 tile/palette buffer. Aligned views expose
32 columns with a zero unused tail cell per row; fine-scrolled views expose
the required 33rd fetch column. Contract tags, immutable metadata, and both
world-plane fingerprints are revalidated on every slice. Invalid, unavailable,
tampered, or out-of-range input leaves caller output untouched.

`TecmoGameplayScene` loads TGCP-2 and TGCT-1 from its canonical pack, decodes
the 768-by-240 world, slices 32 columns when aligned or 33 when fine-scrolled,
and draws through a framebuffer subview so partial first/last columns cannot
bleed into surrounding margins. Actors, anchors, ball Q8 coordinates, shot
start/end, movement, proximity, passing, switching, and AI use coherent world
coordinates. The TGCP projector applies jump altitude exactly once to the
actor, not the ball; offscreen objects are skipped. Draw preflight revalidates
camera/world state, every tile/CHR reference, and all poses before writing.

Object state must use the shared full-court coordinate contract:
`TecmoGameplayCourtCoordinate` is integer X `0..767`, Y `0..239` from the
upper-left TGCT world origin; `TecmoGameplayCourtCoordinateQ8` is the same
plane with eight fractional bits. Player positions/anchors, ball/shot
positions, and TGOR hoop landmarks must not introduce local screen-space
origins. TGOR's exact hoop anchors are `($00A0,$94)` and `($0260,$94)`;
the separately proven ordinary flight endpoint Y is `$8F`. Keep the
transactional `tecmo_gameplay_scene_court_coordinates` snapshot fail-closed
and leave output unchanged on malformed state. The static Bank04 tip-off
player/ball anchors are ROM-exact; the post-handoff live layout, tip animation,
and related scene policies remain native or capture-bounded and must not
inherit that claim.

Production scene code must connect those types through the transactional TGCP
adapters, not rebuild `focus_world_x` or cast raw X/Y locally. Launch and
pre-tip handoff use `tecmo_gameplay_camera_settle_court`; the live update uses
`tecmo_gameplay_camera_follow_court` exactly once after object mutation; draw
uses one `tecmo_gameplay_scene_court_projection` snapshot for ten players and
the ball. Keep the raw TGCP APIs intact for exact pure-vector tests. Q8 is
validated and floored once at the adapter boundary, jump altitude is applied
only to the shooter, and offscreen output remains the neutral TGCP sentinel.
Do not describe the adapters themselves as ROM-derived behavior.

Production live actor drawing must consume one
`tecmo_gameplay_scene_court_frame`, not independently fetch a TGCT slice and
TGCP projections. The combined transactional frame owns the TGOR-tagged
viewport, all ten player projections, the ball projection, the scene frame,
and camera-follow serial. Keep the viewport/projection camera X identical.
For stationary visible actors, screen X must change by the inverse signed
camera delta and screen Y must remain stable; visibility loss must produce
the neutral zero-X/Y sentinel. Keep the native left/center/right background
hashes
`4F52BCC1`, `9CC9CD31`, and `033B45D5` at camera X `102`, `256`, and `408`.
These are native integration checkpoints, not emulator-frame or complete
possession-choreography evidence.

TGMO-1 `gameplay/movement` is the strict 1664-byte ordinary actor-movement
movement boundary (FNV1a32 `6C82A137`). It requires exact same-pack TGPL-1
`2047CCE0`, TGCP-2 `53247856`, and TTDT-1 `812628F0`, and cross-checks its copy
of the fixed clamp byte-for-byte against TGCP-2. Its seven revision-fingerprinted
sources are Bank02 `$A89E-$A90D` (`0BD2CB61`), Bank04 `$ACE4-$AD25`
(`36A1B92C`), Bank05 `$879B-$8866` (`E05FE645`), `$88F9-$89BC`
(`613D0B4C`), `$8E58-$8F96` (`A32D3C92`), `$BF6C-$BFA7`
(`71812CB0`), and fixed `$F106-$F1B0` (`CB1D4EAF`). Keep exact sizes,
descriptors, source records, padding/reserved bytes, full-ROM SHA/FNV identity,
canonical payload hash, provenance, and same-pack dependencies fail-closed.

The pure transactional kernel uses direction bits right/left/down/up
`1/2/4/8`, TTDT profile byte 0, and GAME SPEED adjustments `+5/-1/-6`.
Movement amount is `max(8, adjusted_rating + (condition >> 4) - 6)`; the shared
fractional accumulator is Q4. Diagonals use `amount-floor(amount/4)`, direction
changes have one update of action-state latency, and vertical handlers compare
against `$4A/$EC` before moving. Animation phase uses period 8, delay high
nibble 3, and direction-transition high nibble 5. State validation must reject
bad tags/coordinates/actions/directions/fractions/animation phases, out-of-range
condition/speed, unreachable rating arithmetic, and overflow without mutating
the caller. The exact pose-base-plus-animation-low-nibble resolver is available,
and the live scene uses it for initial, controlled-player, and TGAI-driven CPU
court frames.
Pose-record tags are rebound to the MMC3 R2-R5 selector addressed by the
ROM-style `$01/$41/$81/$C1` slot; keep the ball's `$C1` path on R5. Use the
exact `$8F02` signed linked-minus-selected comparison for the pose-table half,
while labeling the scene's fixed opposing roster-slot link as native policy.
Keep matchup-card team logos
out of the on-court actor pass; they are not player metasprite components.

TGMO-1 applies fixed `$F106-$F1B0` as the full selected-actor dispatcher, not an
unconditional scene clamp: it honors object state 4, action `$0F/$10`, the
state-7/8 versus flags-bit-3 exemption, and the direction/state/flags conditions
that set the boundary-violation latch around
`$00DF-floor(Y/2)` / page-1 / `$0220+floor(Y/2)`. Ordinary live control supplies
object state 0 and flags 0. Only the offensive primary/ball holder may latch;
selector 1 must resolve through strict TPNL-1 to OUT OF BOUNDS, clear the latch,
and enter the existing violation/restart rules flow. Other violation detectors
remain unported. Contradictory axes are normalized to neutral on that axis as
native integration policy. Actor start placement/direction, the fixed
five-player roster-slot/matchup-link binding, and CPU target/shot policy remain
approximate or unsupported. Ordinary non-controlled
actor locomotion is TGAI-directed and TGMO-derived only within the exact/native
boundary below.

TGBD-1 `gameplay/ball-dribble` is the strict 608-byte ordinary held-ball
animation boundary (FNV1a32 `E2CE6BFF`). It requires exact same-pack TGPL-1
`2047CCE0` and TGMO-1 `6C82A137`, and imports Bank05 `$B52E-$B5BF`
(`DB540670`) plus `$B5C0-$B677` (`E9784D28`). Preserve the exact eight
directions, eight animation phases, two `$8F02`-selected table halves, signed
attachment offsets, `$B5C8[half*64+direction*8+phase]` height lookup, and DMC
condition where the animation low nibble is 3 and high nibble is 0. Parser,
dependency, source-map, full-revision, reserved-byte, table, and transactional
state validation must fail closed.

The live scene uses TGBD to keep an ordinary held ball attached for both human
and CPU holders. Human/legacy movement and the supported automatic selected-
primary flow can advance TGMO/TGBD cadence. Selected primary runs once through
Bank06 `$8374->$83F3->$8491` before `$8284-$82A5` skips it in the ordinary
loop. Keep free-throw and active-shot ball paths separately owned.
The fixed opposing roster-slot link used for `$8F02` remains native scene
policy, and the scene flattens the exact height into canonical visible Y before
TGCP projection. Do not describe that projection adapter, the link assignment,
or complete 6502 caller scheduling as ROM-exact. Verify the strict asset and
phase/sound vectors with
`tools\Run-GameplayBallDribbleTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

Ordinary passes use one actor-neutral scene transport. Human NES-A passes
carry their controller. For typed automatic ownership in the supported
ordinary `$05A1=0` context, selected-primary state 4 runs once through
`$8374->$83F3->$8491->$8B90`. Exact opcode 9 at `$8FC5-$8FE7` copies `C9=$21`
to `$046E[X]` at `$8FCA/$8FCC`, advances the five-byte cursor, and Bank05's
selected-primary pointer index `$21` enters `$89D7` in the same native update.
`$8284-$82A5` then skips `$0308/$0309` in the ordinary loop, preventing a
duplicate command step. Human selected primary remains excluded; never use raw
`$030C/$030D` as a controller mirror. `$89D7` writes state
`$0F`/packed `$32`; state `$0F` dispatches through `$8695`,
and `$8999/$9C29` yields the captured `$32->$22->$12->$02->$03->$04` cadence
before `$86A8-$86B7` jumps directly to shared `$B074`. Do not require
slot-10 state `$03`; the direct route is observed entering with `$0478=$13`.
`$B074-$B0FD` locks the typed candidate and swaps the `$000E/$037F`-shaped
roles at launch while the `$0308`-shaped holder stays the passer until genuine
Bank05 `$B24F` (`AC 0A 03`). The captured actor-2 route locks receiver actor 4,
and `$B24F` stores 4 to `$0308`; `$B2FA-$B300` only clears raw `$BA` bit 2.
CPU catch must not mutate human control. Current
duration/interpolation remains native-approximate until `$B42F/$BB9F/$BBA0`
and `$B1E7/$B500` are strict assets. The isolated exact `$BD6E-$BDC6` uint16
kernel does not imply trajectory ownership. Broader pass-selection policy,
unsupported selected-primary gates/states, and `$B13F` interception/contact
remain fail-closed.
Raw `$030C/$030D` is not a
valid zero-human/nonzero-automatic classifier; use typed controller ownership.

TGFT-1 `gameplay/fatigue` is a strict 512-byte ROM-only boundary (FNV1a32
`F80F170D`) with Bank02 `$B4E6-$B5C7` (`F61DFFF7`), fixed `$ED2F-$ED3E`
(`09342B88`), and exact same-pack TTDT-1. Preserve cadence reloads `6/4/1`,
active countdown/capacity/condition decay, bench +4 recovery/caps, and Rev1's
second-team countdown-store asymmetry. TTDT profile byte 3 is maximum capacity.
The live adapter ticks once per live-action scene update and publishes condition
to TGMO for the next update. Do not claim exact intra-frame 6502 ordering or
original active-lineup selection; scene roster slots `0..4` are still policy.

TGAI-2 `gameplay/cpu-steering` is the strict CPU target/direction evidence and
bounded live-movement boundary. It is 7632 bytes / FNV1a32 `C8CFFDC0`, requires
exact same-pack TGMO-1 (`6C82A137`), and retains Bank06 `$81F7-$82D3`
(`23BB7271`), `$87AE-$88AF` (`F866B06C`), `$88DA-$8A95` (`9616E586`),
`$8B90-$8BE0` (`9AD2BA91`), `$8BE1-$9237` (`344298FE`), `$9280-$9329`
(`C82E6853`), `$938B-$9620` (`47818A62`), fixed `$C006-$C008`
(`14B2472E`) and `$CBE0-$CBF6` (`41C5B5C8`), and Bank04 `$9F2E-$AC75`
(`71331A96`). Keep exact sizes, full-ROM identity, descriptors, source records,
zero padding/reserved bytes, handler table, command opcodes, canonical payload,
provenance, and dependency fail-closed.

Opcode 15 is a separate, harness-only raw selected-defender contract. Its
canonical Bank04 records `$0037/$004B` dispatch through Bank06 `$9172`; the
raw resolver may execute only the `$91C8` selected-defender stores after every
named RAM owner is captured at that exact command point. In particular,
`$91F1-$91F5` compares new X to `$06D5`; only equality reaches
`$91F6-$91F8` and replaces `$06D5` with old Y, otherwise `$06D5` is preserved.
The canonical Rev1 `$9208-$9216` tail sets new `$057C=07` and `$059E=X` before
selector 4 reaches `$C711`; the selector is observed, not executed. LIVE keeps
opcode 15 deferred because `$0499` and the related raw lifecycle owners are not
faithfully retained. Do not add shadow mirrors or claim the deterministic
harness is a natural FCEUX `$91C8` capture; that capture remains open research.

Bank06 state 4 adds the actor-local `$0547/$0551` command offset to `$9F2E`.
Fixed `$C006->$CBE0` maps Bank04, copies one five-byte record into `$C7-$CB`,
restores the bank, and `$8B90` dispatches `$C7` through 24 exact handlers. The
bounded Bank04 stream contains 680 aligned records and ends at `$AC75`; code
resumes at `$AC76`. `$938B-$9620` is an exact formation-stream assignment path,
not proof of every play-selection condition.

The pure direction API reproduces the target-application guard at
`$92D4-$92DD`, the target-minus-actor `$88DA-$899D` octant decision, and the
`$8A8E` map. For court-reachable deltas, a 2:1 magnitude ratio is cardinal,
inclusively; otherwise it is diagonal. The C API also preserves the 6502's
wrapping 16-bit doubling at synthetic extremes. Direction codes `0..7` are
right, left, down, down-right, down-left, up, up-right, up-left. The guard skips
the `$92FE` jump to `$88DA` for zero delta and thereby retains the prior
direction; the C API rejects that no-write vector without mutating output.
Aligned command decode exposes raw fields, handler CPU, and a bounded
handler-entry effect category only. Do not turn those categories into semantic
play names.

`--gameplay-cpu-steering-harness` is also deterministic and console-only. Its
typed input is one selected actor, all ten canonical TGCT X/Y coordinates,
possession, TGOR orientation, a possession-consistent ball holder, one explicit
opposing linked/matchup actor, difficulty `0..2`, and an optional validated
explicit target coordinate. Validate every coordinate and coherence field
transactionally. Its printed canonical FNV1a32 snapshot must cover all ten
coordinates and every context field, domain-separating the optional target when
present. Default and explicit targets are deterministic harness/native policy.
The supported automatic selected-primary flow composes its source-produced
target through TGMO once; ordinary actors use explicit formation/marking
coordinates. Never describe these choices or the link assignment as
ROM-exact. Only the
resulting nonzero TGAI octant is exact. A
zero delta must report keep-direction/no-write rather than inventing a prior
direction.

`--gameplay-cpu-steering-movement-harness` is the console-only wrapper around
the transactional composition boundary also called directly by the scene.
Its typed input embeds the complete TGAI
snapshot plus a valid TGMO movement state, rating, condition, GAME SPEED,
object state, and movement flags. The selected actor coordinate must exactly
match the TGMO state before every step. A nonzero exact TGAI direction must map
through the validated same-pack TGMO direction table to its NES held-input
bits, then advance the role-coherent TGMO kernel: primary clamp path for the
offensive holder and secondary path otherwise, with exact one-update latency,
accumulator, animation, and clamp behavior. The CLI must reconcile
the resulting selected-actor coordinate into the next steering snapshot.
TGAI's zero-vector no-write has no held-input equivalent; use neutral only as
the explicitly documented native harness policy and retain TGMO latency. Keep
this API transactional and cover all eight directions, cardinal/diagonal
movement, zero-vector neutral, primary/secondary role coherence, clamp,
snapshot re-evaluation, and
malformed state/profile inputs.

`TecmoGameplayScene` loads TGAI-2 and evaluates the supported automatic
selected primary first, then eligible non-controlled non-selected actors from
one immutable post-human-input ten-coordinate snapshot. Candidates commit
together. The primary command runs once before ordinary-loop exclusion and its
source target composes through TGMO; ordinary actors use scene-owned native
targets and fixed opposing roster links. Keep unsupported primary gates/states,
zero-vector bridging, object state/flags, native targets, and shot timing
explicitly approximate/fail-closed.
The live state must retain its no-command sentinel and no pending advance until
the original play/formation command lifecycle is reconstructed. Do not claim a
complete CPU policy, shot/pass/steal choice, ROM actor-link ownership, or full
live parity. `$B081-$B32E` is converted separately as the per-frame candidate
selector and must not be classified as ordinary targeting.
`--gameplay-cpu-steering-test`,
`--gameplay-cpu-steering-inspect`, `--gameplay-cpu-steering-harness`, and
`--gameplay-cpu-steering-movement-harness` are console-only and must not
become an in-game debug mode. Verify with
`tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` and
see `docs/gameplay-cpu-steering.md`.

`--gameplay-movement-harness` is a deterministic console-only developer tool;
it is never reachable from normal game flow and must not grow into an in-game
debug mode. Verify the importer, seven source spans, parser/provenance/dependency
mutations, state transactions, speed/diagonal/gate/clamp vectors, and scene
handoff with
`tools\Run-GameplayMovementTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.
Verify TGFT importer/parser/provenance/dependency mutations, cadence, active
decay, bench recovery, team asymmetry, and malformed-state transactions with
`tools\Run-GameplayFatigueTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

TGFL-1 positions are now a live dependency, while its pose/state and script
overrides remain unconnected. The slicer intentionally represents the canonical
view rather than the original streamer's staged PPU-prefetch order. Verify the
camera/live boundary with
`tools\Run-GameplayCameraProjectionTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.
Verify TGCT-1 world decode, camera 0/1/7/8/255/256/257/511/512 slices,
transactional mutation rejection, provenance, and the unchanged legacy loader
with `tools\Run-GameplayCourtTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

## Runtime Architecture Notes

This is a native port, not an emulator wrapper. Current modules of interest:

- `src/tecmo_game.c`: runtime orchestration and high-level render dispatch
- `src/tecmo_asset_pack.c`: ROM import orchestration and native entry builders
- `src/asset_pack/tecmo_asset_pack_arena.c`: ROM-only native arena background-layer and sprite-group importers
- `src/asset_pack/tecmo_asset_pack_reader.c`: generic TAP1 read/list/dump API
- `src/asset_pack/tecmo_asset_pack_source_map.c`: sanitized iNES source-map serialization
- `src/asset_pack/tecmo_asset_pack_writer.c`: generic TAP1 builder/write API
- `src/asset_pack/tecmo_asset_pack_d9f6.c`: bounded D9F6 nametable decoder and edge-case self-test
- `src/asset_pack/tecmo_asset_pack_finale.c`: ROM-only TFIN-1 post-PASS finale importer
- `src/asset_pack/tecmo_asset_pack_gameplay.c`: strict TGPL-1 gameplay-core importer
- `src/asset_pack/tecmo_asset_pack_gameplay_court.c`: strict TGCT-1 court importer and legacy center-nametable builder
- `src/asset_pack/tecmo_asset_pack_gameplay_court_orientation.c`: strict TGOR-1 court-orientation importer
- `src/asset_pack/tecmo_asset_pack_gameplay_backcourt.c`: strict TGBC-1 live backcourt-detector importer
- `src/asset_pack/tecmo_asset_pack_gameplay_camera.c`: strict TGCP-2 camera/projector/clamp importer
- `src/asset_pack/tecmo_asset_pack_gameplay_movement.c`: strict TGMO-1 ordinary-actor movement importer
- `src/asset_pack/tecmo_asset_pack_gameplay_ball_dribble.c`: strict TGBD-1 held-ball animation importer
- `src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c`: strict TGFT-1 fatigue-evolution importer
- `src/asset_pack/tecmo_asset_pack_gameplay_cpu_steering.c`: strict TGAI-2 CPU command/target/direction evidence importer
- `src/asset_pack/tecmo_asset_pack_gameplay_close_shots.c`: strict TGCS-1 numeric close-shot importer
- `src/asset_pack/tecmo_asset_pack_gameplay_dunk_cutaway.c`: strict TGDK-1 screen/palette/CHR/staged-sprite importer
- `src/asset_pack/tecmo_asset_pack_gameplay_jump_shots.c`: strict TGJS-2 ordinary-jump importer
- `src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.c`: strict TGFL-1 raw free-throw lineup importer
- `src/asset_pack/tecmo_asset_pack_gameplay_audio.c`: strict TFSX-1 frontend and TSFX-1/TDMC-1 gameplay-audio importer
- `src/asset_pack/tecmo_asset_pack_start_menu.c`: ROM-only TSGM-1 blue start-game menu importer
- `src/asset_pack/tecmo_asset_pack_opening.c`: ROM-only TISC-1 TECMO/rabbit and NBA opening-screen importer
- `src/asset_pack/tecmo_asset_pack_post_arena.c`: ROM-only READY/WARRIORS/CLIPPERS/BUCKS/PASS importers
- `src/asset_pack/tecmo_asset_pack_util.c`: shared importer diagnostics, byte encoding, and local file helpers
- `src/asset_pack/tecmo_asset_pack_import_layout.h`: shared ROM import layout and provenance contracts
- `src/tecmo_intro_screen.c`: strict TISC-1 opening-screen loading and rendering
- `src/tecmo_intro_trace.c`: explicitly enabled local trace diagnostics only
- `src/tecmo_intro_arena.c`: strict TATL/TASG loading, native arena drawing, capture debug scaffolding
- `src/tecmo_intro_finale.c`: strict TFIN-1 loading, finale phases, title bands, and rendering
- `src/tecmo_gameplay_scene.c`: native launch, input, state, animation, audio-event, result, and rendering integration
- `src/tecmo_gameplay_dunk_cutaway.c`: strict TGDK-1 loader, palette resolver, stage scheduler, and OAM-priority renderer
- `src/tecmo_gameplay_camera.c`: strict TGCP-2 parser and pure/production camera/projector state APIs
- `src/tecmo_gameplay_movement.c`: strict TGMO-1 parser, transactional locomotion/clamp kernel, and developer-harness vectors
- `src/tecmo_gameplay_ball_dribble.c`: strict TGBD-1 parser and transactional held-ball geometry/phase resolver
- `src/tecmo_gameplay_fatigue.c`: strict TGFT-1 parser and transactional active-decay/bench-recovery state evolution
- `src/tecmo_gameplay_cpu_steering.c`: strict TGAI-2 parser plus console-only command inspection, raw opcode-15 source-contract resolver, shared full-snapshot direction evaluator, and transactional live/CLI TGMO movement adapter
- `src/tecmo_gameplay_court.c`: strict TGCT-1 parser, full-world decoder, and camera-positioned viewport slicer
- `src/tecmo_gameplay_court_orientation.c`: strict TGOR-1 parser and possession-synchronized orientation state API
- `src/tecmo_gameplay_backcourt.c`: strict TGBC-1 parser and transactional frontcourt/return detector
- `src/tecmo_gameplay_free_throw_projection_test.c`: test-only TGFL-1 -> TGCP-2 checkpoint composition
- `src/tecmo_gameplay_audio.c`: strict gameplay-audio loader, event sequencer, DMC decoder, and music/SFX mixer
- `src/tecmo_frontend_audio.c`: strict frontend cue contract, stable playback checks, and shared SFX-engine adapter
- `src/tecmo_start_game_menu.c`: strict TSGM-1 menu loading, update, transition, and rendering
- `src/tecmo_intro_stage.c`: intro sprite staging and arena transition state model
- `src/tecmo_bank07.c`: fixed-bank helper counterparts
- `src/win32_platform.c`: temporary Windows platform layer

Keep new opening-sequence components out of `tecmo_game.c` when possible. Add focused modules and let `tecmo_game.c` call into them.

`TecmoRuntime` embeds large native asset/state structures and must remain off
the thread stack. Win32 owns it through `VirtualAlloc`, while command-line test
paths use heap allocation; every initialization attempt must be paired with
`tecmo_runtime_shutdown` and the matching release. Do not compensate for asset
growth by increasing the PE stack reserve. Exercise the asset-pack-only,
shortcut-shaped startup path with:

```powershell
.\tools\Run-Win32LaunchSmokeTest.ps1 -Build
```

Passing `-DecompRoot <LOCAL_DECOMP_ROOT>` additionally checks the existing
explicit console `--root ... --flow-test` developer path; it does not change
the GUI launch root.

The smoke test requires `tecmo_port_game.exe` to have PE subsystem 2 and keeps
`tecmo_port.exe` at subsystem 3. Win32 selects `TECMO_MODE_FIRST_SPRITE` after
runtime initialization and presents native frame 0 before the first update.
Original intro/title B input and intro Left/Right debug scrubbing are ignored,
so normal play cannot fall back into the modern menu or skip opening steps.

The global Win32 Player 1 keyboard mapping is arrows for directions, Z for
NES A, X for NES B, Enter for START, and either Shift or Space for SELECT.
Escape and Tab are deliberately unbound. Player 2 retains numpad
8/2/4/6, 1, 3, 9, and 7 for directions, A, B, START, and SELECT.

### Roster and season-management boundary

Roster rows follow Bank02 `$AE4C-$AE9C`: jersey numbers begin at nametable
column 6 and names at column 9. Keep the native origins at x=48 and x=72.
Static roster rating bytes are not season statistics; player detail therefore
shows the fresh-season `.000/.000/.000` percentage row and zero totals until a
strict mutable per-player stat source is ported.

TSAV-1 persists only season type, team control, team wins/losses, and schedule
index. Entering GAME START may prepare the next ROM schedule matchup but must
not advance that index, alter records, synthesize scores, or write TSAV. Only
`tecmo_season_commit_game_result` accepts a completed, matching pending result
and atomically persists it. The native scene now launches that pending matchup;
it ends only after the matching non-tied result commits successfully, then
returns to the existing season result rows without reinitializing the session.

League Leaders category navigation is supported from ROM `$AD3D-$AD58`.
Bank00's `$AC88/$AC5E` priority metasprites and per-player accumulator/ranking
path are not yet carried by TSNS/TSAV, so the native screen must not substitute
the unrelated Bank01 cursor or render empty result templates as valid data. A
ROM-font marker identifies the current category and confirmation displays the
explicit unsupported-results boundary. Do not add fabricated roster or court
statistics to make this screen look populated.
