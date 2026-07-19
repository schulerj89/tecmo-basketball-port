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
.\build\tecmo_port.exe --gameplay-audio-test
.\build\tecmo_port.exe --gameplay-state-test
.\build\tecmo_port.exe --team-management-test
.\build\tecmo_port.exe --season-test
.\tools\Run-MusicTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
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
.\build\tecmo_port.exe --render-test-mode gameplay-jump-frame12 build\gameplay_jump_12_test.png
.\build\tecmo_port.exe --render-test-mode gameplay-dunk-frame16 build\gameplay_dunk_16_test.png
```

These are development tools, not main-menu entries.
`gameplay-close-shot-frameN` remains a compatibility alias for the canonical
`gameplay-dunk-frameN` numeric-variant-0 checkpoint.

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

PRESEASON's B/Escape return uses its own direct path: it rebuilds the stable
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
queued ID through the first NMI with active mask `$063E=0`. Presentation ID 6
replaces it only on the confirmed title frame-127 handoff, matching fixed
`$E477` after the title loop and before blue-menu root setup. Generic returns to
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
`$AD01-$AD0E`, `$B1D1-$B1E6`; every source-map role must carry the corresponding
FNV1a32 rather than a one-byte or inferred offset.

`audio/gameplay-dmc` is TDMC-1: 2515 bytes / FNV1a32 `AD70E6E8`. It deduplicates
the exact fixed-bank `$C080-$C280`, `$C440-$C710`, and `$C740-$CAF0` inclusive
sample pools and exposes five bounded, non-looping, non-IRQ clips at rates 14
or 15. Bank05 `$B5AB` is held-ball/dribble. `$A9C5` is conservatively named
the dunk sequence and `$ABF5` the layup sequence: bounded local slot-2 evidence
correlates numeric variant 0, its cutaway, and the later `$A9C5` trigger at
action frame 87; slot 1 correlates numeric variant 2 with `$ABF5` at action
frame 34. These are sequence-level names, not impact/rim claims, and neither is
queued by the live scene. The two `$A8D6` clips remain address-bound. DMC
advances independently of music and tonal SFX; no trigger writes `$4011`.
GAME MUSIC gates future track 5 only,
and GAME SPEED has no path into audio cadence. `tecmo_gameplay_audio_stop_all`
models the fixed clear-all path for music, SFX, and DMC. Because none of these
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
expiry to SFX 3, the late-clock countdown to 14, violations to 6, made shots and
free throws to crowd response 11, and moving possession to the proven `$B5AB`
held-ball/dribble DMC clip. `BANK05_9FEC_CUE` remains neutral and is gated at
the bounded foul/restart boundaries. Side-result 12/13, the sequence-named
A9C5/ABF5 clips, and address-named A8D6 clips stay imported without live use.

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
when the scene ends. Actor/camera layout, movement/AI,
ordinary jump timing, ball arcs and make/contact rules, the distance policy
selecting dunk/variant 0 versus layup/variant 2, live close-shot
profile/direction selection and left-facing render
mirroring, dynamic team/court palette selection, foul detection, free-throw
lineup/aim/outcome/rebound and CPU positioning/script behavior, and HUD text are
explicit native approximations.
TGCS stores 208 exact profile/direction resolutions into TGPL pose data, but the
live scene currently selects only profile 0/direction 0 and mirrors
actor-facing-left; the asset breadth must not be read as proof of that narrower
live policy.
The imported TGCT palette bytes and embedded FCEUX RGB profile are exact; that
does not imply frame-identical matchup/state palette selection. The high-level
mapping is proven as variant 0 = dunk and variant 2 = layup; low-level TGCS
APIs and fields retain those numeric ROM identities. The local save states,
FCEUX traces, and screenshots used for correlation remain ignored verification
material, not committed provenance or runtime input. See
`docs/gameplay-state-foundation.md`; verify state with
`tecmo_port.exe --gameplay-state-test` and the compound scene with
`tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

The scene must obtain TGPL-1 `gameplay/core`, TGCT-1 `gameplay/court`, TGCS-1
`gameplay/close-shots`, TMUS-1 `audio/music`, TSFX-1
`audio/gameplay-sfx`, TDMC-1 `audio/gameplay-dmc`, and `chr/all` from the same
explicit pack. Exact-size reads, canonical fingerprints, deep bounds/reserved
checks, CHR revision fingerprints, the music asset's selected pack path, and
source-map provenance fail closed before the scene is marked available. Drawing
preflights every court cell and actor/ball pose so a rejected frame leaves the
destination untouched.

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
- `src/asset_pack/tecmo_asset_pack_gameplay_court.c`: strict TGCT-1 static-court importer
- `src/asset_pack/tecmo_asset_pack_gameplay_close_shots.c`: strict TGCS-1 numeric close-shot importer
- `src/asset_pack/tecmo_asset_pack_gameplay_audio.c`: strict TSFX-1/TDMC-1 gameplay-audio importer
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
- `src/tecmo_gameplay_audio.c`: strict gameplay-audio loader, event sequencer, DMC decoder, and music/SFX mixer
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
