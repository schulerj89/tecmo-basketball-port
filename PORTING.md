# Porting Guide

This project is a native C port of Tecmo NBA Basketball. The goal is not to
wrap an emulator, replay emulator logs at runtime, or make the game depend on a
decompilation checkout. The final runtime should own game concepts in C and load
its data from ROM-derived asset packs.

## Core Rules

- Runtime code should use native game concepts: screens, scenes, phases,
  timers, camera movement, tile layers, palettes, sprite groups, teams, players,
  play state, and scripted steps.
- NES storage concepts such as PRG banks, CHR banks, CPU addresses, PPU
  addresses, OAM, mapper registers, and IRQ timing are allowed in importers,
  source maps, low-level asset decoders, and research tools. Keep them out of
  high-level gameplay and scene code when a native concept can represent the
  behavior.
- The decompilation folder is a read-only reference for understanding where
  original code and data live. It must not be required by final runtime paths or
  normal asset-pack builds.
- The normal import path should be ROM-only:

```powershell
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
```

- Do not commit ROMs, rebuilt NES images, PRG/CHR bytes, lifted ASM, decoded
  proprietary tables, emulator logs, generated trace JSON, private screenshots,
  or other original-derived payloads.
- Prefer focused C modules over expanding `tecmo_game.c`. Let `tecmo_game.c`
  orchestrate; put import, scene, script, asset, and renderer logic behind clear
  module APIs.

## Importer Boundary

The importer is allowed to understand the NES because the ROM is stored as NES
data. Its job is to convert that storage into native assets.

Good importer responsibilities:

- Validate iNES headers and mapper expectations.
- Read PRG and CHR banks.
- Use reference/source maps to locate tables and graphics.
- Decode ROM data into named asset-pack entries.
- Produce safe source-map metadata that explains where an entry came from
  without embedding original bytes in docs.

Bad runtime dependencies:

- Loading loose decompilation files.
- Loading Lua/FCEUX capture output.
- Requiring local paths such as `TECMO_DECOMP_ROOT` for normal play or render
  tests.
- Replaying emulator-shaped logs as the primary game implementation.

## Runtime Boundary

Runtime code should consume asset-pack entries as native assets and update C
state directly.

Prefer APIs shaped like this:

```c
void tecmo_arena_intro_init(TecmoArenaIntro *intro,
                            const TecmoAssetPack *pack);
void tecmo_arena_intro_update(TecmoArenaIntro *intro);
void tecmo_arena_intro_draw(TecmoFramebuffer *fb,
                            const TecmoArenaIntro *intro);
```

Avoid making gameplay or scene APIs expose emulator-shaped write streams unless
the module is explicitly a low-level importer, decoder, or diagnostic.

`TecmoRuntime` is a large owner of native scene assets and state. Entrypoints
must allocate it off the thread stack, clean up partial initialization through
`tecmo_runtime_shutdown`, and release it with the matching allocator. Keep the
normal Windows stack reserve useful for call depth; do not use a larger linker
stack as a substitute for explicit runtime ownership.

The Windows game target is `tecmo_port_game.exe`, linked with the GUI subsystem
while retaining `mainCRTStartup` so it shares the console target's argument
parsing. The generated shortcut explicitly supplies the port project root, and
normal Win32 initialization permits an empty legacy roster so the original-game
path uses strict ROM-derived asset-pack entries rather than loose decomp roster
files. It selects the native TECMO/rabbit intro immediately after runtime
initialization and presents frame 0 before updating. The console
`tecmo_port.exe` remains the CLI/test surface, including explicit
`--root <LOCAL_DECOMP_ROOT>` developer workflows and access to the modern
diagnostic menu.

## Scripted Screens

Many opening screens are scripted. Port those scripts into native C concepts
instead of preserving 6502 or emulator terminology in the runtime.

For example, an intro script should look like:

```c
typedef enum TecmoIntroStepType {
    TECMO_INTRO_STEP_SET_PHASE,
    TECMO_INTRO_STEP_WAIT,
    TECMO_INTRO_STEP_MOVE_CAMERA,
    TECMO_INTRO_STEP_FADE_TO_PALETTE,
    TECMO_INTRO_STEP_SHOW_SPRITE_GROUP,
    TECMO_INTRO_STEP_HANDOFF
} TecmoIntroStepType;
```

The importer may derive those steps from ROM tables, but the runtime should run
the native script.

## Arena Intro Direction

The arena intro should become a native scene, not a capture replay.

Target concepts:

- `TecmoArenaIntroScene`
- `TecmoArenaIntroPhase`
- `TecmoArenaCamera`
- `TecmoArenaTileLayer`
- `TecmoArenaPaletteCycle`
- `TecmoArenaGoal`
- `TecmoArenaSpriteGroup`
- `TecmoArenaIntroScript`

The basket/goal should be one anchored object. Backboard, rim, net, support, and
post pieces should be positioned relative to a shared goal anchor, while the
camera or scene layer moves around it. Do not fix basket alignment by applying
separate frame offsets to individual parts unless that is explicitly modeling a
native object animation.

Asset-pack entries should move toward native names, for example:

- `arena/intro/background-layer`
- `arena/intro/palette-cycle`
- `arena/intro/sprite-groups`
- `arena/intro/script`
- `arena/intro/ready-screen`
- `arena/intro/warriors-transition`
- `arena/intro/clippers-transition`

Temporary capture-shaped entries may remain only as migration aids until the
native scene is validated.

The TECMO/rabbit and NBA opening screens are native ROM-only scenes. The
importer emits `intro/tecmo-presents-screen` and `intro/nba-license-screen` as
TISC-1. The first combines decoded screen `$00` background cells with the
20-piece ROM rabbit OAM compositor, exact background/sprite palettes, resolved
`chr/all` offsets, and its nine-stage fade schedule. The NBA entry contains
decoded screen `$02`, its background palette, and the six-stage delayed fade
schedule; it has no sprites. Runtime must reject malformed TISC-1 data and CHR
fingerprint mismatches without falling back to the former hardcoded tables or
loose trace JSON. The exact native handoffs are title-to-license frame 133 and
license-to-arena frame 277. Loose trace parsing is opt-in diagnostic
scaffolding only via `TECMO_ALLOW_LOOSE_INTRO_TRACE=1`.

The arena background is now on the native path: the ROM importer decodes
screen `$18` into a versioned `TecmoArenaTileLayer` with exact tile IDs,
attribute-derived palette indexes, background palette bytes, and resolved CHR
offsets. Runtime rendering must load that layer and `chr/all` from the same
asset pack. Do not reintroduce generated tile-sheet patterns or captured
nametable playback as a normal fallback.

Arena sprites are also on the native path. The ROM importer emits TASG-2 at
`arena/intro/sprite-groups`; runtime validates the exact two-group, 71-piece
contract and draws the goal before the jumbotron using stored CHR offsets,
palette indexes, flips, anchors, and transition scroll. Missing or invalid TASG
data must fail the exact arena render instead of falling back to hardcoded goal
pieces, synthetic palettes, or captured OAM. TASG-2 keeps its existing header,
group, and piece strides and interprets piece bytes 10..11 as signed
`connector_overlay_y_adjust`. Exactly the center `dx=16`, `dy=32` goal
connector piece uses `-1`; the remaining 70 pieces use zero. Runtime draws the
canonical ROM-derived second 8x8 tile at `y+8`, then draws an adjusted copy of
that tile using a connector overlay palette. Overlay indexes 0 and 1 are
transparent, while indexes 2 and 3 retain their exact ROM palette colors. This
bridges opaque-black internal rows without changing the shared goal anchor,
piece offsets, canonical post position or extent, or goal motion.

Goal motion reproduces Bank07 `$D861` bytewise and is driven by Bank04's
stream1 (`$07EC/$21`) timing and coordinate bytes. For raw negative relative Y
bytes (`dy - $40` in `$C0-$FF`), convert the byte to a magnitude and subtract
it from the stream low byte. If that subtraction borrows, decrement the page,
then preserve D861's fallthrough: add the stream low byte to the subtraction
result a second time and increment the page on carry. Non-negative relative
bytes use the normal low-byte add and carry. Admit page `$00`, plus page `$FF`
only when its low byte is `$F0-$FF`; reject every other page before narrowing
to OAM Y. This produces the ROM-exact visible goal timing: frame 240=0, 260=5,
276=10, 280=10, 292=15, 300=15, and 308=16, with 16 in the final pose.
Jumbotron positioning and the TASG-2 masked connector overlay remain unchanged.

The TATL importer's 51-row source mapping remains exact, but runtime drawing
must model the arena IRQ as two independently positioned bands. Draw rows
`0..37` at the global `-$0301` scroll and clip them below the lower-band restart.
Rows `38..50` are the lower large-crowd/pedestal band. Its logical screen origin
is `motion_counter_88 + $7B`, and its first complete scanline and upper clip are
`motion_counter_88 + $7C`; clear/restart the viewport from that clip and draw
the lower rows relative to row 38. The correction versus a uniform tile stack
changes with the transition: `+5` native pixels at scroll `$50`/motion `$6A`,
`-3` at `$58/$5A`, `-11` at `$60/$4A`, and `-15` at the final `$64/$42`.
Do not encode the final `-15` as a constant. At clean frame 539, the post ends
at output Y 429, the black pedestal opening begins at Y 430, and its cream cap
begins at Y 432.

READY and WARRIORS are ROM-only native scenes. `arena/intro/ready-screen`
contains the decoded screen, five palette stages, and the 12-record attribute
sweep at native frames 24 through 46. It blacks out at frame 56 and hands off
at frame 58. `arena/intro/warriors-transition` contains the two-page layer,
split-band CHR mappings, 46-piece player group, progressive Bank06 WARRIORS
glyphs, two late tile patches, and frame-214 handoff to screen `$1B`. Runtime
must load these entries and `chr/all` from the same pack and validate all
resolved CHR offsets before marking either scene available.

CLIPPERS is also ROM-only. `arena/intro/clippers-transition` decodes screen
`$1B`, the four palette stages, both horizontal nametable pages, and the fixed
lower band used by IRQ handler `$FD84`. The upper 200 scanlines use CHR
`$2C/$2E`; scanlines 200..239 reset horizontal scroll and use `$2C/$FA` so the
Bank06 `$9EAE` team-name glyphs remain fixed. Bank06 pointer `$AD76` selects the
length-prefixed `CLIPPERS` string at `$ACA3`; character map `$A273` and glyph
quads `$AF05` generate the two tile rows. Do not source those tiles from a PPU
capture. Palette stages begin at frames 10, 14, and 18, the wordmark is ready
at frame 32, `$88` begins advancing at frame 40, and `$88 >= $14` changes the
upper scroll to `$FF` at frame 80. The native chain reaches route `$883D` at
frame 151 and must remain in the intro mode until that next route is ported;
never fall through to the placeholder play-setup court.

The entire post-PASS finale is ROM-only and native. The importer emits
`intro/finale-sequence` as TFIN-1, and runtime consumes that entry with
`chr/all`. TFIN-1 represents five named two-page scenes, shared sprite geometry
with scene-specific palettes and imported anchors, reverse-transition timing,
and the progressive title as 44 resolved 2x2 glyph slots across virtual pages.
It does not store imported title text. The title renderer preserves three
horizontal bands: the primary progressive-write scroll, the independently
advanced pre-roll/tail scroll, and the fixed lower band. The final script runs
its load boundaries, short loop, reverse transition, staged wait, title
pre-roll/write/tail, final dispatch wait, and then remains in a persistent
terminator hold. Missing or malformed TFIN-1 data is a hard native-render
failure; there is no decompilation, Lua-log, or capture fallback.

The next ROM-only boundary is implemented as two strict entries. TATR-2
(`title/attract-continuation`) decodes command `$14` screen `$01` and carries
the initial/final sprite palettes, resolved 49-piece NBA emblem, attribute
states, and bounded completion/reset timing. TTLE-1 (`title/start-screen`)
decodes screen `$03` and carries its palette, resolved CHR cells, and the exact
blank/visible `$222B-$2234` prompt rows. Runtime never consumes the local video,
FCEUX screenshots, Lua status, OAM dumps, or PPU dumps used for verification.

After the attract route resets, the first START enters a ten-frame title load
window. The input must be released before a second START is armed. That second
press runs nine seven-frame blank/visible prompt cycles through confirmation
frame 126 and hands off at frame 127 to the original blue start-game menu.

The blue menu is now a ROM-only native boundary. `menu/start-game` uses TSGM-1
and contains two precomposed 32x30 pages, nine exact transition palette stages,
resolved background cells, the 49-piece NBA emblem, the root cursor, settings
overlays/digits, and native input/route/timing metadata. The importer decodes
screen `$04`, composes the root and season records through the ROM character
map and box rules, and rejects any result that does not match the Rev1 raw,
decoded, and composed fingerprints. Runtime consumes only TSGM-1 and `chr/all`
from the same asset pack after frame 8; frames 0-7 also require TTLE-1
`title/start-screen` for the retained title image. Local video, Lua traces,
PPU/OAM dumps, save states,
screenshots, and decompilation files remain verification-only.

The root cursor is resolved directly from Bank01's selector `$30`, tile `$24`
record to the exact 8x16 pair at `chr/all` offset `$C240`. The source record,
resolved CHR pair, and resulting TSGM-1 payload each have independent Rev1
fingerprints so a selector-mapping regression is rejected during import and at
runtime.

Title-out/menu-in timing uses local palette checkpoints 0, 2, 4, 6, 8, 20,
24, 28, and fully bright 32. The stable root has seven selections. Up/Down
wrap immediately and repeat every eight held frames; NES A dispatches, while B,
START, SELECT, Left, and Right do nothing on the root. SEASON GAME moves
to the six-item second page over 32 frames, advancing the background eight
pixels and the emblem five pixels per frame; B performs the exact reverse.
Within that six-item boundary, GAME START hands off to `PLAY_SETUP` and TEAM
DATA hands off to `ROSTERS`; the other four season-management selections remain
unported no-ops. MUSIC wraps OFF/ON, SPEED wraps FAST/NORMAL/SLOW, and PERIOD
clamps across 2/3/4/8/12 minutes. A accepts the highlighted setting and B
cancels it on release. The native helper `$D723` runs with `$07F6=0`, so held
A/B never activates. Root, season, MUSIC, and SPEED consider the previous A/B
byte only when the current NES controller byte is zero; any current button
suppresses that released action. Root's `$9F87[0]=$80` mask accepts released A
only, generic `$C0` rows accept released A/B with A priority for raw A+B, and
current Up/Down still takes the generic direction path. Consequently A+Down
moves first and releasing both activates the moved selection. The native byte
map is A `$80`, B `$40`, SELECT `$20`, START `$10`, Up `$08`, Down `$04`, Left
`$02`, and Right `$01`.

Popup setup uses the ROM `$AB77` transfer order. Row zero is installed before
the first yield, so local setup frame 0 already shows one row; each following
frame adds one row. MUSIC reaches all six rows on frame 5 and enters its input
helper on frame 6. SPEED reaches eight rows on frame 7 and enters on frame 8.
PERIOD reaches six rows on frame 5, keeps frame 6 as an extra full cursorless
delay, and enters on frame 7. Teardown frame 0 is still full, then removes one
bottom row per frame: the final removal/helper update is frame 6 for MUSIC and
PERIOD and frame 8 for SPEED. Popup destinations, the restored root, and both
season-slide destinations defer the displayed cursor by one OAM commit frame;
TSGM metadata byte 148 supplies `cursor_commit_delay_frames=1`.

`$E481` is a post-return fade, not a universal root dispatch. Root TEAM DATA
and the supported season GAME START/TEAM DATA exits show stage 8 on frames 0-1,
stage 7 on 2-3, stage 6 on 4-5, stage 5 on 6-7, black stage 4 on 8-10, and hand
off once on frame 11. ALL STAR's `$8221` route remains an explicit handoff to
`PLAY_SETUP`. PRESEASON now enters its native `$9966` submenu and does not run
the later `$E481` fade prematurely.

The ALL STAR/GAME START and TEAM DATA destinations are still bounded native
placeholders. In normal play, B/Escape returns from them to the exact root or
season page and selection that dispatched the route; a season return restores
the fully slid-in page. If a placeholder has no recorded blue-menu origin,
normal play ignores B/Escape instead of entering the modern diagnostic menu.
Explicit debug/test placeholder routes keep their original modern-menu return.
Committed MUSIC, SPEED, and PERIOD values survive the placeholder round-trip.
A neutral-input latch swallows B while held, its release edge, and the first
fully neutral frame so the restored season page cannot re-consume that release
as an immediate slide-out.

PRESEASON uses the strict `menu/preseason` TPRE-1 entry. Import composes the
CONTROL, DIFFICULTY, and DIVISION overlays from Bank03 ROM records over the
existing TSGM-1 screen, and resolves all four division team screens, palettes,
CHR cells, team tables/coordinates, and P1/P2 markers from the Rev1 ROM. Both
Bank01 `$8036` marker records identify CHR selector `$30`; the importer resolves
the seven referenced 8x16 pairs through that ROM field into a 224-byte CHR
contract with FNV1a32 `1E505537`.
CONTROL row zero opens EASY/MEDIUM/EXPERT, preserving the committed difficulty
on B; rows 1-6 proceed to division selection. MAN VS MAN transfers the second
division/team selector to controller 2. Other control modes keep both selectors
on controller 1. Up/Down and Left/Right wrap with the generic eight-frame held
repeat, A/B remain release-triggered, and a same-division second player cannot
select the first player's team. Team B reconstructs the active player's
division screen; division B returns to CONTROL.

The team-entry stack fades at frames 3/5/7 and is black at 9. Team input begins
at frame 16; its new screen is black for palette frames 16-17, capped on 18-21,
22-25, and 26-29, and full from 30. Team exit is full at frame 0, capped on
1-2, 3-4, and 5-6, black from 7, and completes at 32. Rebuilt overlays are
drawn while black; the division helper is already live while the display fades
through black counter 0, capped counters 1-4/5-8/9-12, and full counter 13.
The accepted-input `$E1=5` seed remains frozen throughout setup, teardown,
team-entry, and team-exit, then decrements only on interactive selector frames.

The supported boundary is the fully interactive second-team screen, not game
startup. P2 A retains the accepted-input `$E1=5` side effect but cannot execute
the `$B277-$B282` team-confirmation state advance or fixed `$E481` launch
chain. TPRE-1 is exactly 26736 bytes / FNV1a32 `D9EE49F4` and depends on the
same pack's 14112-byte TSGM-1 (`DF89006B`) and 262144-byte `chr/all`
(`F6F6E854` / `96A64F53B240ABB4`). Source-map provenance covers the Bank03
flow, records, input/coordinate/ownership/team tables, the unexecuted boundary,
Bank01 cursor/player records, all four descriptors/streams/palettes, fixed
input/loader/fades, and full CHR. Exact-size preflight and deep parsing reject
missing, malformed, oversized, cross-pack, or wrong-revision assets. No trace,
decompilation file, screenshot, dump, state, or video is a runtime dependency.

An accepted release reaches `$D788` and seeds directional `$E1=5`. Generic
direction reaches `$D79D`, writes eight, and the same-loop tail decrements it so
held direction repeats on the eighth following frame; generic release actions
branch before this directional gate. PERIOD instead checks
`(current|previous)&$0C` first, so direction release (including zero-delta
Up+Down) can consume and lose released A/B. PERIOD released A accepts, released
B cancels, and raw A+B is consumed with `$E1=5` but does neither. Season slide
steps 1-31 preserve `$E1`; step 32 runs the destination helper immediately and
ticks 5 to 4. The cursor commits on the following displayed frame. Other
unported root routes cross an explicit native handoff rather than silently
replaying 6502 code or consuming capture data.

TSGM-1 has exact payload FNV1a32 `DF89006B`. Runtime validates the complete
14112-byte entry, byte-148 cursor delay, and the six ROM-derived popup cursor
anchor bytes at 149..154: MUSIC `(47,200)`, SPEED `(47,167)`, and PERIOD
`(71,200)`. Import derives these from the three popup flow selector indexes,
Bank03 `$9F30/$9F13` coordinate tables, and Bank01 `$8031` cursor `dy=-4`.
The remaining header tail stays zero-reserved, along with the full 262144-byte
CHR fingerprints (FNV1a32 `F6F6E854`, FNV1a64 `96A64F53B240ABB4`). Exact-size
asset-pack reads reject forged TSGM, TTLE-1, and `chr/all` directory sizes before
allocation. Missing or malformed TTLE-1 is a hard render failure for start-menu
frames 0-7; there is no loose-file, decompilation, capture, or cross-pack
fallback.

The seven-tile MUSIC overlay intentionally preserves the original visible
`SIC` suffix from the underlying `GAME MUSIC` row. This overlap was confirmed
against bounded emulator evidence and is not a text-composition defect.

Native audio now begins at the opening. The ROM importer emits `audio/music`
as TMUS-1, a strict semantic asset for music IDs 5 (gameplay), 6
(presentation), 7 (opening), and 8 (period stinger). The exact payload is 36784
bytes / FNV1a32 `05C00ECB`, with 37 deduplicated voices, 75 imported pitch
periods, and 2251 native instructions. Notes, rests, voices/envelopes, legato,
signed pitch deltas, bounded loops, and resolved phrase calls/returns are C
concepts at runtime. `$C0` retains one live loop counter per channel, matching
the fixed engine rather than assigning persistent state per command. ROM
addresses, pointers, and raw engine opcodes are not.
Rev1 validation covers Bank04 `$8AA4-$9F05` (`06F2A750`), directory
`$8CD0-$8CE1` (`59366EC4`), requested tracks 5/6/7/8 (`1270498B`, `BD91FCF1`,
`69F85EC2`, `8122C6CF`), fixed engine `$F2F2-$F9D0` (`FC6A0BC1`), and period
table `$F93B-$F9D0` (`3F5A394D`). Only the ROM and resulting asset pack are
runtime inputs.

Sequencing uses the NTSC rational cadence `39375000/655171` from the 44.1 kHz
audio sample clock, independently of frame rendering and GAME SPEED. ID 7 is
queued exactly once on entry to the rabbit/TECMO opening and continues through
the existing title/menu transitions without a restart; its imported program
ends after 2614 inclusive ticks (43.4950 seconds), measured from fixed `$F7EE`
consuming queued ID 7 through the first NMI where active mask `$063E` clears.
The MUSIC setting only
allows or rejects future ID-5 queues. OFF does not stop an active song, preview
a choice, or globally mute IDs 6-8. The current native synthesizer covers the
two pulse voices, triangle, and noise with imported pitch/duty/envelope state;
there is no DMC or claim of cycle-level nonlinear APU fidelity yet. Win32 feeds
44.1 kHz mono 16-bit PCM through eight 1024-sample `waveOut` buffers. Opening
music is queued before the ring fill; later track changes can sit behind at most
8192 queued samples (185.8 ms). A missing device or rejected TMUS-1 asset remains
a clean silent runtime. Device failure deliberately freezes sequencer state;
focused tests also exercise the renderer as a deterministic advancing null
sink. Loose decompilation,
FCEUX/Lua output, captures, frames, screenshots, logs, states, dumps, and
`temp-videos` are never audio dependencies.

The importer validates the raw finale dispatch chain as `$851C` wait 50 ->
`$83EA` wait 30 -> `$852E` wait 0 -> `$83AE` wait 75 -> `$8310` wait 1 ->
`$FFFF`, with screens `$1C`, `$20`, `$1F`, `$22`, and `$2D`. Selector 2 uses
first seed `$78`, second seed `$D8`, and delta `-8`; the swap holds the last
emitted `$E8`, while the outward pass begins at `$D0`. Native C models each
route from 742 imported core frames plus 156 dispatch-wait frames, or 898. Five
one-frame asynchronous load gates reach 903; six selector black/fade
normalization frames preserve the exact `$E8` hold, `$D0` outward start, and
`$10` endpoint before the persistent hold begins at bounded native frame 909.
The ROM's `$8A48` and `$850C` state gates are conditional, so these 11 native
scheduling frames are not claimed as ROM-exact wait durations.

## Validation Rules

Validation should prove native behavior, not just that a captured frame can be
redrawn.

Use layered validation:

- Unit tests for imported asset structure and source-map coverage.
- Unit tests for native scripts: phase order, waits, camera movement, palette
  timing, handoff points, and object anchors.
- Unit tests for object composition, especially goal pieces sharing one anchor.
- Render tests for deterministic frames such as arena, READY, WARRIORS, and
  CLIPPERS.
- Local-only visual comparisons against a known-good emulator or old-commit
  reference screenshot.

Normal gates should stay close to:

```powershell
.\build.ps1
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --assetpack-test
.\build\tecmo_port.exe --music-test
.\tools\Run-AssetPackTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-MusicTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-Win32LaunchSmokeTest.ps1 -Build
```

The Win32 smoke test creates and inspects an isolated shortcut, launches with
the port root while an invalid decomp environment root is present, and removes
the test shortcut afterward. Supplying `-DecompRoot <LOCAL_DECOMP_ROOT>` also
exercises the explicit console `--root ... --flow-test` development path.

When a feature becomes ROM-only, update tests so missing decomp roots, missing
Lua logs, and missing loose capture files are expected to pass.

## Naming Guidance

Names in high-level code should describe the ported game behavior:

- Use `camera_y`, `phase`, `fade_step`, `goal_anchor`, `sprite_group`,
  `tile_layer`, and `palette_cycle`.
- Avoid leaking `ppu_addr`, `oam_stage`, `mapper_write`, `irq_latch`, or
  `capture_frame` outside low-level import/diagnostic code.

Low-level names are acceptable inside modules whose purpose is explicitly NES
decode, asset import, or historical comparison.

## Migration Policy

Existing capture loaders and decomp-root paths can remain temporarily when they
are useful for comparison, but they should be treated as debug-only scaffolding.
For every area migrated to ROM-only native C:

1. Add or update importer output.
2. Add native runtime structures and update/draw APIs.
3. Add tests proving no loose capture/decomp dependency remains.
4. Keep local comparison tooling ignored.
5. Remove normal runtime lookup of the old capture/decomp path once parity is
   proven.
