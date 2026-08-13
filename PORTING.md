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

### Gameplay Research Tool Boundary

The tracked gameplay laboratory under `tools/gameplay-lab` is explicitly
outside the native product. It is a revision-locked research instrument that
can read original-game RAM and mapper state, drive two complete controller
tables, and record bounded local telemetry. It has two closed orientation-0,
offense-side-0 MAN VS MAN profiles: the three-point baseline's unchanged
window/acceptance contract (the shared TGLM-4 controller is not yet
smoke-tested against it) and
`ordinary_two_point_make`, which exposes only `x=$0108..$010F`,
`y=$6C..$74` and requires point value 2, terminal MAKE, and score delta 2.
Unknown possessions,
presentations, close routes, fouls, violations, mirrored movement, failed
coordinate progress, and missing hook evidence abort instead of becoming
generalized rules.

The lab's TGLM-4 address map and TGLAB-4 output schema are provenance for
research conclusions, not asset-pack entries. They distinguish raw 16-bit
altitude velocity (`$049A/$04A5`), horizontal velocity (`$04E7/$04F2`), and
vertical velocity (`$04FD/$0508`). Accepted hooks snapshot direct
object-slot-10 H/V and saved object H/V scratch (`$038D-$0390`) inside the
callback before bounded queued emission. At the `$A7A9` entry hook, direct
slot-10 H/V is entry-time evidence captured before `JSR $A790`; the saved words
are scratch and can predate that call, so neither signed direction nor
same-invocation ownership is inferred. Captured CSV, hook events, screenshots,
FM2, status, and emulator logs remain ignored under
`temp-videos/gameplay-lab`. Neither the importer nor native runtime may consume
them. Ported behavior still requires a separately justified ROM-derived asset
contract and native C implementation.

The first original-ROM two-point pilot stopped safely when an AI-controlled
front defender could not be selected and cleared. The tracked driver now
requires a next-frame `$91CB->$0309` store confirmation, permits at most six
confirmed defensive stores across the entire pilot, and lets only the
two-point profile attempt
up to four one-pulse passes with eight-frame holder reacquisition. TGLAB-4 also
captures target-motion, slot-10, scoring, and actual `$8FB9/$9042` swap evidence
at hook time and fails the profile closed when required timing evidence is
missing or malformed. Its exact pending route orders point value 2 through
`$B995->$B9D7` and MAKE through `$91BC->$933B->$942D`, then requires the
`$8C57/$8C78` raw-direction `$05->$00` remap, direction `$00`/phase-low
`$05`/close `$00` at `$AD4E/$B32C`, target `$00A0/$008F`, 16-bit count
`$003C`, slot position shooter `+(2,-1)`, altitude `$3900`, altitude velocity
`$04EC`, raw H/V velocity bounds `$FF88..$FF8F`/`$001D..$0026`, 63 `$B100`
entries, 26 state-08 updates, one +2 score commit, and SFX mailbox `$0B`
throughout the same-frame actual swap.

The first TGLM-4 launch exposed a Lua 5.1 60-upvalue compilation error in the
status writer. The writer was split without changing its acceptance predicate
or 63 emitted status keys, and the tracked script and map now pass the bundled
32-bit Lua 5.1 parser. A later bounded launch reached live setup but aborted at
frame 4214's defensive-A store confirmation deadline before any shot. It
supplied no `$B100`, state-08, or score evidence and closed FCEUX; do not widen
that bounded failure into coordinate/timing/controller retries. TGLM-4 has not
produced a successful pilot and does not extend native two-point support.

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

The Win32 Player 1 adapter maps arrows to directions, Z to NES A, X to NES B,
Enter to START, and both Shift and Space to SELECT. The literal physical B
key, Escape, and Tab are unbound. Player 2 keeps numpad 8/2/4/6 for
directions and numpad 1/3/9/7 for NES A/NES B/START/SELECT. This platform
mapping is kept outside native game semantics and is exercised by the headless
`--controls-test`. The PC event bridge retains one bounded pending logical
press per mapped controller/button, so a mapped down/up pair drained between
60 Hz updates is presented for one update while a physically held key remains
held. This is PC event-queue robustness, not ROM-exact input timing. The
bridge's `tecmo_win32_keyboard_begin_controls_frame` and
`tecmo_win32_keyboard_end_controls_frame` calls must bracket exactly one
`tecmo_runtime_update_players` call: begin exposes the effective current input,
and end restores the physical levels and clears the one-update pulse.

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
Within that six-item boundary, TEAM CONTROL, SCHEDULE, GAME START, STANDINGS,
and LEADERS enter native `TECMO_MODE_SEASON_MENU`; TEAM DATA enters
`TECMO_MODE_TEAM_DATA`. GAME START prepares only the ROM-scheduled pending
matchup, then launches it through the native gameplay scene. The schedule and
records remain unchanged until the matching non-tied result commits. MUSIC
wraps OFF/ON, SPEED wraps FAST/NORMAL/SLOW, and PERIOD
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
and all six season-page departures show stage 8 on frames 0-1, stage 7 on 2-3,
stage 6 on 4-5, stage 5 on 6-7, black stage 4 on 8-10, and hand off once on
frame 11. PRESEASON's `$9966` and ALL STAR's `$8221` routes enter their native
submenu construction directly and do not run the later `$E481` fade first.

PRESEASON's B/X return directly rebuilds the stable root on the PRESEASON
row; it reinitializes MUSIC/SPEED/PERIOD and does not enable the shared neutral
gate. ALL STAR, TEAM DATA, and season-management destinations use the recorded
return path, preserve those committed settings, and restore the exact root or
fully slid-in season row. Return controls remain submenu-specific: most use B,
while PROGRAMMED uses START/SELECT because B decrements the selected record.
The recorded-return neutral latch swallows the held return input, its release
edge, and the first fully neutral frame before the restored menu can process
input. Explicit debug/test routes keep their modern-menu return. Gameplay
launches from PRESEASON and SEASON through explicit native handoffs. ALL STAR
remains at its mapped prelaunch boundary.

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

Final second-team A retains the accepted-input `$E1=5` side effect and now emits
an explicit native gameplay launch action. The runtime transfers the selected
teams, difficulty, ownership, and menu settings without replaying the
`$B277-$B282` state advance or fixed `$E481` chain. TPRE-1 is exactly 26736
bytes / FNV1a32 `D9EE49F4` and depends on the
same pack's 14112-byte TSGM-1 (`DF89006B`) and 262144-byte `chr/all`
(`F6F6E854` / `96A64F53B240ABB4`). Source-map provenance covers the Bank03
flow, records, input/coordinate/ownership/team tables, the recorded launch boundary,
Bank01 cursor/player records, all four descriptors/streams/palettes, fixed
input/loader/fades, and full CHR. Exact-size preflight and deep parsing reject
missing, malformed, oversized, cross-pack, or wrong-revision assets. No trace,
decompilation file, screenshot, dump, state, or video is a runtime dependency.

TEAM DATA now extends the ROM-only supported boundary through player detail and
back to the roster. The importer emits `menu/team-data` as TTDT-1, exactly
96372 bytes / FNV1a32 `812628F0`. It contains decoded screens `$0C/$0D/$0E`,
29 selector records, 29 team records, 27 bounded logo expansions, four dynamic
profile palettes, 348 player records, 24 resolved portrait cells per player,
two cursor records, a ROM font map, and strict timing/input metadata. Runtime
requires TTDT-1 plus the same pack's 262144-byte `chr/all`
(`F6F6E854` / `96A64F53B240ABB4`). Exact-size preflight, canonical payload and
CHR fingerprints, deep bounds checks, reserved-byte checks, and resolved-CHR
validation reject missing, malformed, oversized, wrong-revision, and
cross-pack dependencies without drawing partial output.

Root and season TEAM DATA first use TSGM-1's post-return fade through its
frame-11 dispatch. TTDT-1 then holds rendering off locally through frame 3,
turns rendering on black at 4, applies palette caps 0/1/2/3 at 7/11/15/19,
and reaches the stable selector/cursor on frame 20. Measured from a released
selector A, profile entry is black at 8, rendering is off at 10, rendering
returns black at 16, palette caps advance at 19/23/27/31, and the profile is
stable at 32. Profile PLAYERS DATA changes only OAM/cursor state and is stable
on the next frame; STARTERS and PLAYBOOK enter natively on the next frame. The
two six-player roster pages slide in 32 frames at eight pixels per frame. Roster
A reaches black at 8, render-off at 10, render-on black at 15, palette caps at
18/22/26/30, and stable player detail at 31. Detail B uses the 32-frame reverse
timing back to the same roster row. Direction repeat and A/B release semantics
remain ROM-derived; B restores the exact root or season origin.

All three profile A routes are native. `PLAYERS DATA` opens the roster.
`STARTERS` edits five unique starters from seven eligible bench players,
supports player detail, and carries the reset confirmation. `PLAYBOOK` edits
four unique slots from eight plays and carries the replacement carousel and
reset flow. Their strict 21061-byte `menu/team-management` TTMG-1 payload
(FNV1a32 `D192EAC6`) requires the same pack's TTDT-1 and `chr/all`; malformed or
cross-pack dependencies fail before partial rendering. Player detail and both
management editors are terminal and cannot launch gameplay.

## Native NES Color Profile

The native renderer embeds the exact 192-byte `FCEUX.pal` RGB profile shipped
with FCEUX 2.6.6 (FNV1a32 `9F872B25`) and never reads an emulator installation
at runtime. This makes raw native frames use the same color interpretation as
the local reference environment while preserving the ROM palette indexes in
every asset. `--video-test` checks the full embedded profile, representative
colors, and six-bit index masking. Changing this profile is a global visual
contract change and requires deliberate regeneration of all colored PNG
checkpoints.

The post-arena finale uses three independently positioned horizontal bands.
Bounded raw FCEUX comparison confirmed that its magenta underline remains after
the progressive title text has moved away; that underline-only tail is not an
unrendered asset. Intro regression coverage therefore includes title-write
frames 192, 288, 384, and 448 in addition to the load and tail endpoints.

Profile palettes are selected through Bank06 `$A3A5/$A3A9/$A3AD` and sourced
from `$AC0B-$AC4A`; ATL uses `$AC0B-$AC1A` (FNV1a32 `34F6B8DC`). Logo cells
come from Bank06 `$A2E4-$AC4A` layout/tile/attribute tables and Bank03 `$8017`
origins. ATL therefore resolves to the exact E4-backed 10x6 tile/palette matrix
at `(16,48)` (pair fingerprint `6F28E5C6`), rather than a capture-derived image.
Bank02 supplies rosters, profiles, direct All-Star player pointers, and the
`$AD5B` ability-bar algorithm. Bank03 `$8D5C/$A25C/$B432`, Bank00
`$8001/$8071`, and fixed `$C42E/$CAF1/$D5C5` supply portrait selection,
layout, metatiles, attributes, and composition. Fixed `$DC19-$DC35` is instead
the exact 29-entry home gameplay-uniform color table. Bank01 `$BF1F` supplies the
condition seed/threshold path. Source-map provenance records each focused span,
screen descriptor/stream/palette, fixed input/loader/fade helper, and full CHR;
no capture, trace, video, screenshot, log, dump, save state, Lua output, or
decompilation file is a runtime source.

`tools\Run-TeamDataTests.ps1 -RomPath <LOCAL_ROM.nes>` builds a private ROM-only
pack, runs the strict parser and native flow (including direct All-Star mapping,
positions, conditions, ability meters, return origins, and exact transition
checkpoints), verifies 15 deterministic PNG hashes, checks malformed-payload
rejection, and removes its temporary pack/log/screenshots. The broader
asset-pack and native-flow regressions retain the same TTDT coverage.

An accepted release reaches `$D788` and seeds directional `$E1=5`. Generic
direction reaches `$D79D`, writes eight, and the same-loop tail decrements it so
held direction repeats on the eighth following frame; generic release actions
branch before this directional gate. PERIOD instead checks
`(current|previous)&$0C` first, so direction release (including zero-delta
Up+Down) can consume and lose released A/B. PERIOD released A accepts, released
B cancels, and raw A+B is consumed with `$E1=5` but does neither. Season slide
steps 1-31 preserve `$E1`; step 32 runs the destination helper immediately and
ticks 5 to 4. The cursor commits on the following displayed frame. Root
departures cross explicit native handoffs rather than silently replaying 6502
code or consuming capture data.

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

Native audio now begins at the arena handoff in the opening sequence. The ROM
importer emits `audio/music` as TMUS-1, a strict semantic asset for music IDs 5
(gameplay), 6 (presentation), 7 (opening), and 8 (pregame matchup stinger).
The exact payload is 36784 bytes / FNV1a32 `05C00ECB`, with 37 deduplicated
voices, 75 imported pitch periods, and 2251 native instructions. Notes, rests,
voices/envelopes, legato,
signed pitch deltas, bounded loops, and resolved phrase calls/returns are C
concepts at runtime. `$C0` retains one live loop counter per channel, matching
the fixed engine rather than assigning persistent state per command. ROM
addresses, pointers, and raw engine opcodes are not.

Fixed `$F7D5-$F7DB` establishes the voice timing fields after shifting the raw
voice byte: attack uses bit 7, decay bits 5-6, and release bits 2-4. Command
`$91 00` resets both channel pitch-delta bytes; nonzero `$91` operands remain
signed additions. Focused native anchors cover the real raw `$08`/`$07`
voices, track-6 pulse-1 reset instructions 492/716, 100000 ticks each of the
looping IDs 5/6 without pitch drift, and clean ID-8 termination at 396 inclusive
ticks.

Rev1 validation covers Bank04 `$8AA4-$9F05` (`06F2A750`), directory
`$8CD0-$8CE1` (`59366EC4`), requested tracks 5/6/7/8 (`1270498B`, `BD91FCF1`,
`69F85EC2`, `8122C6CF`), fixed engine `$F2F2-$F9D0` (`FC6A0BC1`), and period
table `$F93B-$F9D0` (`3F5A394D`). Queue provenance is independently anchored at
Bank04 `$826A-$826E` (`FCDCAFEF`) for opening ID 7, the first arena route pointer
at `$82CF-$82D0` (`07FD2C8D`), and fixed `$E477-$E47B` (`0ADC9176`) for menu ID
6. Only the ROM and resulting asset pack are runtime inputs.

Sequencing uses the NTSC rational cadence `39375000/655171` from the 44.1 kHz
audio sample clock, independently of frame rendering and GAME SPEED. The
TECMO/rabbit and NBA-license scenes are silent. ID 7 is queued exactly once at
the native license-to-arena frame-277 handoff; Bank04 `$826A` queues it one NMI
before the first route pointer at `$82CF` enters arena routine `$88E8`. Its
imported program ends after 2614 inclusive ticks (43.4950 seconds), measured
from fixed `$F7EE` consuming queued ID 7 through the first NMI where active mask
`$063E` clears. The first START enters title setup, whose imported TFSX timing
preserves the five proven `$E3FA` yields (`$D92E` supplies three and `$DB25`
supplies two after Bank03 clears `$034E`) and hard-stops any remaining opening
program on title frame 5. Confirmed title
frame 127 then queues presentation ID 6 at fixed `$E477`, after title input
completes and before the blue-menu root is built. Entering that mode through a
generic runtime reset does not restart ID 6.
The MUSIC setting only allows or rejects future ID-5 queues. OFF does not stop
an active song, preview a choice, reject ID 6, or globally mute IDs 6-8. The
TMUS-1 music synthesizer covers two pulse voices, triangle, and noise with
imported pitch/duty/envelope state; music does not use DMC. Gameplay DMC
playback is the separate TDMC-1 path described below. Neither path claims
nonlinear, cycle-exact NES APU mixing fidelity. Win32 feeds
44.1 kHz mono 16-bit PCM through eight 1024-sample `waveOut` buffers. Scene
handoffs preserve this queue rather than flushing it, so a track change can sit
behind at most 8192 submitted samples (185.8 ms). A missing device or rejected
TMUS-1 asset remains
a clean silent runtime. Device failure deliberately freezes sequencer state;
focused tests also exercise the renderer as a deterministic advancing null
sink. Loose decompilation,
FCEUX/Lua output, captures, frames, screenshots, logs, states, dumps, and
`temp-videos` are never audio dependencies.
Bank06 `$A145-$A149` queues ID 8 with `A9 08 20 0C C0` (FNV1a32
`1E564AC0`), providing a separate revision-checked source-map anchor for the
pregame-matchup label.

Frontend audio uses a distinct strict `audio/frontend-sfx` TFSX-1 entry
(1792 bytes, FNV1a32 `985DC7ED`) while reusing the native TSFX semantic
sequencer/mixer. It contains only original SFX 8 and 10, with 3 voices, 75
periods, and 87 semantic instructions; runtime never reads ROM, ASM,
decompilation, trace, capture, screenshot, state, dump, or raw opcode data.
The imported metadata fixes the title stop at title-setup frame 5, SFX 10 at
fresh confirmation frame 1, 126 visible animation frames, track 6 and menu
handoff at frame 127, and SFX 8 on genuine accepted Player 1 A-release events
inside the blue-menu state machine. Directional movement, START, B/cancel,
held/repeat input, rejected chords, and the period A+B consume path are inert
for that cue.

The TFSX importer revision-locks the complete Bank04 directory, SFX 8
`$8BF7-$8C29` (`AC9D4C1F`), SFX 10 `$8B97-$8BF6` (`963DC35E`), Bank03 title
setup/confirmation `$8056-$8090`, transition bridge `$C003`, complete
three-yield flow `$D92E-$D9A4`, complete zero-state two-yield flow
`$DB25-$DB87`, frame-yield helper `$E3FA-$E419`, fixed stop bridges `$C024`
and `$CBAF`, menu accept `$D768-$D792`, menu transition `$E477-$E4A0`, stop
helper `$EC06-$EC25`, and audio mailboxes `$F2F2-$F2F9`. The five-yield source
aggregate is `CA4CA88A`. Runtime canonicalizes the selected container path and
accepts exact TFSX-1 plus exact TMUS-1 from that same canonical pack only. The
focused
`tools\Run-FrontendAudioTests.ps1` suite covers deterministic SFX PCM hashes,
title/menu flow timing, accepted-release single-fire behavior, strict
source-map provenance with exact roles/ranges/hashes, same-path alias
acceptance, distinct-pack rejection, missing/malformed/oversized dependency
rejection, and frontend-specific one-byte mutations of every bounded ROM
source span.

Gameplay audio is ROM-only and connected to the live scene. The importer emits
two same-revision dependencies. `audio/gameplay-sfx` is the exact 2824-byte
TSFX-1 payload
(FNV1a32 `968A5DE6`): seven effects (3, 5, 6, 11, 12, 13, 14), 14 voices, 75
periods, and 131 native semantic instructions. Its safe event vocabulary is
clock buzzer (shot or period expiry), violation cue (bounded dynamic cutaway
correlation), crowd response, side-result 12/13, and countdown (each second
below 0:12).
ID 5 is deliberately exposed only as `BANK05_9FEC_CUE`; the available evidence
does not justify calling it a foul, whistle, collision, shot, rim, or dunk.
The `$9FEC` caller requests it after violation/foul/period-reset flow only when
GAME MUSIC is enabled; that caller condition does not change its neutral name.
The importer fingerprints Bank04 `$8AA4-$8CCF` and `$9D8B-$9E12`, the complete
16-entry SFX directory, the fixed audio engine, and the focused gameplay
request sites. Those request-site fingerprints cover fixed `$E7DB-$E7DF`,
`$E863-$E867`, `$E86D-$E871` and Bank05 `$9FEC-$9FF0`, `$AD01-$AD0E`, and
`$B1D1-$B1E6`; the source map records each bounded hash and the fixed JSR anchor
where the fingerprint begins two bytes earlier. In the Rev 1 ROM, `$AD01-$AD0E`
has FNV1a32 `B7141C72` and requests crowd response 11, while `$B1D1-$B1E6` has
FNV1a32 `CFCD9759` and requests side result 12/13 only above 0:01. The latter
uses the scoring side before possession handoff. Both requests share the
last-write-wins SFX mailbox, so 12/13 is the next consumed request when the
clock gate passes; 11 remains at 0:00 or 0:01.

`audio/gameplay-dmc` is the exact 2515-byte TDMC-1 payload (FNV1a32
`AD70E6E8`). It stores three deduplicated fixed-bank source pools and five
bounded clips matching Bank05 `$A8D6`, `$A9C5`, `$ABF5`, and `$B5AB`. `$B5AB`
is held-ball/dribble. `$A9C5` and both `$A8D6` clips remain address-bound and
unresolved. Local slot 2 observes numeric variant 0, its cutaway, and a later
`$A9C5` trigger at action frame 87; this does not prove meaning or exclusivity.
Slot 1 correlates numeric variant 2 with the `$ABF5` action sequence at frame
34 without proving an impact/rim meaning. The live variant-0 presentation
currently requests address-bound A9C5 at frame 87; ABF5 is not yet queued. All
clips use exact rates 14/15,
`$4015=1F`, no loop, no IRQ, and no direct `$4011` write.
Accordingly, the native DMC delta counter remains latched across a clip
retrigger, after the sample reader reaches end-of-clip, and when clear-all
disables the reader. The held DAC value continues contributing to output until
a future DMC bit changes it; neither queue nor stop synthesizes a reset to 64
or an abrupt zero sample. Fixed `$EC06-$EC25` (32 bytes, FNV1a32 `F1BCC8E2`),
called at `$E58D`, `$E9A0`, `$E9DE`, and `$ECAF`, supplies the bounded reset
provenance. Native foul/violation presentation entry and completed-period
presentation entry each perform this clear once before replacement audio; a
qualifying violation, direct-foul, or period live return queues the gated
`$9FEC` cue and gameplay track 5. A foul route entering the free-throw sequence
instead queues track 5 there without the same-numbered SFX cue.

Fixed `$F3FA-$F436` consumes music before SFX, and `$F3F2` maps the four SFX
slots to priority masks `$10/$20/$40/$80`. Native mixing therefore advances
music sequencing and oscillator phases even while the corresponding SFX
channel suppresses its output. Music and SFX mailboxes are last-write-wins
until the next exact `39375000/655171` tick; DMC is independent. GAME MUSIC
gates only future track 5, while GAME SPEED never changes audio cadence.
Missing, malformed, oversized, wrong-revision, and cross-pack TSFX/TDMC assets
fail closed. Run the focused private-ROM suite with
`tools\Run-GameplayAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`; it includes
retrigger, end-of-clip, and clear-all DAC-continuity checks.

The live scene queues track 5 at launch and qualifying restarts only when GAME
MUSIC is enabled, and requests track 6 for halftime/final presentation. Its
evidence-bounded mappings are clock expiry to SFX 3, late-clock countdown to
14, violation to 6, and moving possession to the proven `$B5AB`
held-ball/dribble DMC clip. A made dunk and every resolved free throw, including
a miss, request crowd response 11 followed by qualifying side result 12/13;
the final 0:00/0:01 request remains 11. Enabled GAME MUSIC queues track 5 when
foul presentation enters the free-throw sequence, not on terminal settlement.
The ignored bounded slot-3 observation begins setup at frame 10, requests
gameplay track 5 at 26 and consumes it at 27, then changes the terminal result
mailbox from `$0B` to `$0D` at 280 and consumes it at 281. It is live by 300
with no new music-track request or SFX ID 5 through 360. The final result thus
survives the live transition, which queues neither track 5 nor the neutral
`BANK05_9FEC_CUE`. The supported slot-0 jump miss uses clock-gated
11-then-12/13 mailbox ordering without awarding points; zero-clock settlement
retains 11. Layups alone retain crowd response 11 until their separate caller
path is integrated.
SFX ID 5 remains gated at qualifying violation, direct-foul, and period restart
boundaries. Dunk action frame 87 requests address-bound A9C5. ABF5 and
address-named A8D6 clips remain imported without invented live use; the
deterministic state-`$15` diagnostic requests A8D6-short only at its proven
nonterminal pass repeats.

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
.\build\tecmo_port.exe --frontend-audio-test
.\build\tecmo_port.exe --gameplay-audio-test
.\build\tecmo_port.exe --gameplay-state-test
.\build\tecmo_port.exe --team-management-test
.\build\tecmo_port.exe --season-test
.\tools\Run-AssetPackTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-MusicTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayAudioTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayMovementTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayBallDribbleTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayFatigueTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-TeamDataTests.ps1 -RomPath <LOCAL_ROM.nes>
.\tools\Run-TeamManagementTests.ps1 -RomPath <LOCAL_ROM.nes>
.\tools\Run-SeasonTests.ps1 -RomPath <LOCAL_ROM.nes>
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

## Native Gameplay Boundary

`tecmo_gameplay_state` remains deterministic pure state, but
`tecmo_gameplay_scene` now calls it from live preseason and season games. The
scene owns launch settings and controller/team assignment, ten actors, ball and
possession invariants, native input, shot animation, audio-event dispatch,
presentation phases, and final-result handoff. Directions move the owned actor;
NES A starts a visible gather/flight/catch pass on offense or switches defenders;
NES B starts an offensive shot or attempts a defensive steal/contact action.
START and SELECT are inert, and unassigned controllers cannot act. Preseason
results return to the blue-menu PRESEASON row. Season results are matched to the
pending ordinal/teams and committed exactly once before returning to the
existing result rows.

Gameplay launch first runs the strict ROM-only `gameplay/pre-tip` TPTI-2
boundary. Its 7680-byte payload has FNV1a32 `8E6367FC` and requires exact
same-pack TGPL-1, TTDT-1, TMUS-1, TWAR-1, TGJS-2, and `chr/all`. Twenty-nine
revision-fingerprinted spans retain screen `$15`'s descriptor/stream/palette,
Bank06 card waits/text flow, `$A290` character mapping, `$AF05` 16-pixel
metatiles and `$C6/$FA` CHR setup, Bank04 `$86AB-$88A8` close-up,
`$89DD-$8A2C` palette/control data, fixed `$D861-$D92A` sprite staging,
`$AC76-$ADDF` tip setup, Bank05's `$985B-$9A5F` tip update, and the fixed
launch/live bridges. Exact size,
canonical FNV32/FNV64 values, full-ROM identity, zero-reserved/padding checks,
dependency fingerprints, and sanitized source-map provenance fail closed.
Runtime never reads ROM, ASM, decompilation, Lua, captures, screenshots, logs,
states, traces, or dumps.

The asset-backed cards and close-up reach center setup after
`61 + 121 + 61 + 208 = 451` updates. A successful Bank04-clocked human tip
claims state `$17` during the court/ball descent, immediately enters the
60-update toss cut-in, returns to the 30-update court contest, and then hands
off live. The deterministic primary human route reaches those boundaries at
frames `516/576/606`. The independent CPU-only threshold route reaches
`508/568/598`; those values are not a human capture-clock calibration. Only
the first three durations preserve Bank06's inclusive `$3C/$78/$3C` card
waits for the mode card, matchup, and `1ST PERIOD` exactly; later presentation
durations are capture-bounded.
PRESEASON and `REGULAR SEASON` are the exact mode strings. Card letters use
Bank06's exact character mapping, `$AF05` 2-by-2 metatiles, `$C6/$FA` CHR
selectors, and 16-pixel centered cell placement rather than TTDT's 8-pixel
font.
Matchup logos use the ROM layout anchors `(16,32)` and `(16,128)` while the
separate city/team strings retain their original row positions. The 208-frame
screen-`$1A` close-up includes bounded black entry/exit gates and reuses the
strict TWAR-1 background/OAM/CHR contract. Its capture-bounded phase frame 33
starts the ROM-exact Bank04 `$88` motion steps: fixed `$D861` moves the OAM
player left, nametable scroll moves the referee/player layer right, and NES
OAM-Y displays one scanline below the stored value. The center-court route
proceeds through center black, court/ball descent, toss transition/cut-in,
contest, and then live play. Ball descent interpolates Y 71..145 for its first
60 updates and then holds until the source-clocked claim changes phase. The
toss cut-in renders TGPL-1 screen `$1B` nametable page 1. Page 1 is required by
the reference geometry:
the ball occupies X 176..239 and the hands X 67..159; page 0 contains the
opposite phase. The final live-background band uses the validated pre-ASL R1
selector `$40 + away_team` during
pre-tip, then `$40 + home_team` after live handoff. `$3F` is outside the
accepted `$40..$5A` team-selector range and resolves high-bit final-band tiles
through unrelated CHR data, which caused the former oversized lower-right
graphic. The corrected pre-tip band draws exactly one away-team ROM logo,
right/bottom aligned by its validated variable dimensions, without hiding or
duplicating its embedded city/nickname lettering.

Bank06 `$A10A-$A124` checks current NES B only when `$69` bit 0 is set.
Native PRESEASON clears that gate and ignores B during all three cards; the
regular-season route sets it and either controller may cancel. A, START,
SELECT, and directions are ignored there.
Tip contest input has separate Bank05 evidence. Within the exact
`$985B-$988E` setup span (`F372E57C`), `$985E-$986A` is a 13-byte
current-level NES B read/latch with FNV1a32 `423816F1` and FNV1a64
`032F8A7A4F4439D1`; the later gate tests actor height and a countdown before
consuming the latch. The exact `$98E1-$9A5F` update span also reads current B
at `$9920`. These reads prove the button and held-level semantics, but not the
complete original winner/claim settlement.

Bank04 `$86E1-$8817` owns the countdown clock. The native bridge retains its
sampled `$6A`, evolving `$8A`, source-loop ticks, and captured input bytes;
fixed `$CD96-$CDAB` supplies the exact 8-bit mixer. The deterministic
presentation bridge samples `$6A=$85` after the card and route-setup schedule,
seeds `$8A=$87`, and follows the source yields and marker waits. The primary
Away pulse captures `$E1`, derives capped error/countdown 11, and reaches the
state-`$17` cutaway at frame 516 through the ordinary ball-height/countdown
gate. Byte wrap ends capture rather than manufacturing later input samples.
Lower captured error wins; equal errors defer the native claim because the
original single-winner tie settlement remains unproven. B cannot sample a tip
during the close-up or toss phases and cannot cancel those phases.
The public winner query rejects every phase before `JUMP_CONTEST` without
changing caller-owned output; it is available during `JUMP_CONTEST` and after
the live handoff.
The game clock, shot clock, rules, actors, AI, and live camera remain frozen
until the route's live handoff (frame 606 for the deterministic primary human
capture above). Track 8 queues at card entry, and enabled GAME MUSIC queues
track 5 only at that handoff. Bank04 `$AC8C-$ACD9` initializes
object slots 0..10 from state `$AD82`, sprite-slot base `$AD8D`, facing `$AD98`,
X-low `$ADA3`, X-high `$ADAE`, Y `$ADB9`, and facing-indexed pose tables
`$ADC4/$ADCD`. TPTI-2 already fingerprints both containing spans; the
transactional `tecmo_gameplay_pretip_tip_lineup` decoder supplies the complete
setup for ten players and the ball to the canonical TGCT scene. TPTI header
selectors 178/179 preserve the Bank04-selected jumper identities `(4,9)`
(away/home); the scene consumes those selectors rather than assuming actor
slots. The scene uses the exact coordinate, sprite-slot base, and selected
standing pose without a second mirror. During the 60 presentation updates, both
selected actors use a deterministic native presentation schedule: crouch,
takeoff, rise/reach, apex/contact, fall, then land. Pose pointers are drawn from
the existing bounded ordinary-jump pose vocabulary (325/1060/1061/213/469),
and TGCP applies a native 24-pixel maximum projected altitude while canonical
actor Y remains at its exact anchor. This jump trajectory and pose timing are
native approximations; they are independent of timing error, so late/no input
cannot truncate landing. Generic action poses use the validated actor-facing
mirror path; the orientation-encoded flag is restored only for the Bank04
standing pose at landing. The landing step restores both tip anchors and poses
before the route's live handoff. Raw object-state behavior, exact original
winner/claim settlement, and a complete original jump trajectory are not thereby
ported.
The fixed pre-tip lineup retains TGCP's source-backed initial center camera X
`$0100`; it must not inherit the live ball/goal pre-settle, because doing so can
place one Bank04-selected jumper outside the 256-pixel projection window. The
live handoff still settles transactionally around the awarded possession,
so this fixed presentation viewport does not replace live camera behavior.
`tools/New-TipoffVisualProof.ps1` preserves the earlier frame-661..725 visual
proof contract as historical lineage; it is not the timing authority for the
current Bank04-clocked 516/576/606 route. It builds a Rev1 asset pack,
double-renders that historical range while Away holds B throughout
`JUMP_CONTEST`, checks both jumpers' projected
pose/Y/visibility and both host margins, renders the Away-left-facing checkpoint,
and emits numbered PNGs, all-frame active-edge sheets, a stage contact sheet, an
optional ffmpeg MP4, and a hash/command/runtime-state manifest beneath ignored
`build/proof/` output.
Pre-tip state mutation is transactional: phase-frame bounds, accumulated total,
sample flags/errors/sample frames, terminal-state coherence, and integer
overflow are validated before commit, and malformed or unreachable states are
rejected unchanged.

The compound scene strictly loads TGPL-1 `gameplay/core` (23416 bytes,
`2047CCE0`), TGCT-1 `gameplay/court` (6559 bytes, `ECAB7A93`), TGCP-2
`gameplay/camera-projection` (1536 bytes, `53247856`), TGMO-1
`gameplay/movement` (1664 bytes, `6C82A137`), TGAI-3
`gameplay/cpu-steering` (8016 bytes, `D56EE070`), TGOR-1
`gameplay/court-orientation` (640 bytes, `44B0C44E`), THUD-1
`gameplay/hud` (864 bytes, `3D13AA89`), TGCS-1
`gameplay/close-shots` (3144 bytes, `DACDC976`), TGDK-1
`gameplay/dunk-cutaway` (20272 bytes, `E02F2D21`), TGJS-2
`gameplay/jump-shots` (2776 bytes, `A66EE873`), TGSR-4
`gameplay/shot-resolution` (608 bytes, `5376E82B`), TMUS-1 `audio/music`,
TSFX-1, TDMC-1, and the exact `chr/all` revision from one asset-pack path. Exact-size
reads, payload/CHR fingerprints, deep indexes, reserved bytes, and source-map
provenance are hard requirements. Missing, malformed, oversized,
wrong-revision, or cross-pack data fails before availability; a draw preflight
prevents partial framebuffer updates.

Live actor palette binding follows the original byte path. Bank02
`$A8AE-$A8C9` moves selected roster profile byte 2 bit 7 into `$04B0` bit 0;
the second side adds bit 1. Fixed `$F1F2-$F24C` passes `$04B0 & 3` to the
`$D413` compositor. TGCT-1's exact `$F2E2-$F2F1` table supplies four
live-court profile/side palettes, while fixed `$DEAB-$DEDF` selects first-side
`$30`, Lakers first-side `$38`, or the selected team's `$DC19-$DC35`
second-side color. The scene substitutes those colors at table entries
3/7/11/15 and uses the resulting palette for the court, ball, and all players.
Bank01 `$B0ED-$B133` with `$B138/$B148` is a separate exact pose/cutaway
recipe: it injects the selected side color at offsets 6, 7, and 9 and its
six-bit `+$10` variant at offset 13. TGDK uses that recipe. Fixed five-player
roster slots remain native lineup policy, so do not describe starter selection
as ROM-exact.

THUD-1 owns the fixed live two-row scoreboard. Its exact Rev1 source spans are
Bank01 `$BDF0-$BE1E` (writer and `$2041/$2057` destinations), Bank01
`$BE1F-$BECC` (29 offsets plus 29 five-tile team marks), and Bank02
`$AF64-$B07B` (digit writers, character maps, and the initial-dot-surname
formatter). The parser cross-checks all 59 Bank02 character tiles against the
same-pack TTDT-1 `$FA` font records and exact `chr/all` offsets. Rendering is
screen-fixed and is prepared transactionally before any framebuffer write, so
TGCP camera movement cannot displace scores, clock, jersey numbers, or player
names. Team destinations and tile/name mappings are ROM-exact. The complete
two-row ownership, remaining blank columns, colon tile `$16`, black cell backing, and live `$FA` top-row binding
are reference-verified presentation bounds. The three-digit cap for the wider
C score and the holder/shooter matchup fallback used to label a CPU-only side
are native adapter policies and must remain documented as approximate.
The native scene draws this real game-information HUD during the court-visible
`BALL_DESCENT` and `JUMP_CONTEST` phases and throughout live play. It remains
suppressed for matchup cards, close-ups, black setup, and the toss cut-in so
those authored full-screen compositions are not overwritten.

The TGAI-3 production binding owns a fixed opposing roster-slot link, an
explicit target coordinate, direction result, immutable-snapshot fingerprint,
and decision serial per actor. The link remains pose/facing and defensive
reference metadata; it is no longer treated as the non-holder's implicit target
coordinate. Its compatibility CPU record carries an explicit no-command
sentinel: loading the asset alone does not infer an actor-local command
lifecycle. Bound normal play owns the narrower accepted source cursors and
advancement separately in `live_foundation.play_state`; the two ownership
planes must not be conflated.

Numeric close variant 0 has the exact 32-step direct/held-release table and
variant 2 has the exact 16-step arc/longer-trajectory/contactable table. Their
phase bytes and all 208 TGCS-stored profile/direction resolutions into TGPL pose
data are ROM-derived. Bound production launches retain the selected roster
profile-byte-2 bit and the geometry-derived eight-way active-hoop direction;
only the isolated legacy direct-launch checkpoint remains fixed at profile 0 /
direction 0. Resolved uniform pose-cell
polarity is preserved from the fixed `$D413/$D498` compositor and `$D503`
`AND #$41`, then compared with effective facing only while that facing equals
the actor's assigned TGOR goal baseline. Deliberate movement/action overrides,
mixed poses, pre-tip presentation, and encoded tip/action poses retain the
former orientation path; eligible uniform poses mirror only when authored and
goal polarity differ. Fresh orientation-0
examples are Bank05 `$8F47/$8F57` raw `$012A` -> pose 149 -> `$A6E3/$884E`
-> four resolved `$41` attributes (Away-left), versus raw `$016A` -> pose 181
-> `$A723/$8702` -> four resolved `$03` attributes (Home-right). The captured
original frame/OAM independently confirms the visible Away-left bit-$40
polarity; it does not claim a native pose-149 identity. `$40` is the
horizontal-flip bit, not an intrinsic left meaning for arbitrary art.
Numeric variant 1 is exposed only as a neutral source-backed pose identity with
a bounded native schedule and native-policy-sample substitution for its missing raw
predicate. Its complete object/trajectory semantics are unproven, no
dunk/layup/contact meaning is assigned, and the public scene shot-name helper
still returns `"invalid"` for it. Bounded local original execution proves the
high-level mapping variant 0 = dunk and variant 2 = layup. TGCS-1 bytes,
fingerprints, APIs, and source-map fields keep the numeric ROM identities; the
semantic mapping is derived and validated by the loader rather than read from
loose evidence.

TGDK-1 is the complete strict native dunk presentation boundary. Its importer
revision-checks Bank05 trigger/clear-lane code, fixed dispatch/selector/restore
code, Bank00 screen `$0B` stream and base palette, Bank01 controller/stage/palette
recipe, Bank06 sprite emitter/pointers/geometry, and the exact 256 KiB CHR
revision. It decodes both 1 KiB nametable pages, resolves 1920 bounded background
cells, preserves both side streams and all seven 8x16 OAM records, and composites
records in reverse so lower OAM indexes retain NES priority. The trace-visible
schedule is live 1-22, dispatch 23, black 24-27, cutaway 28-62, black/rebuild
63-70, live return 71, A9C5 at 87, and settlement at 132. Stage 0 is assigned at
27 and becomes visible at 28; later assignments and first-visible frames are
32/37/42/47/52/57. Frame 63 is black by palette with retained OAM, and frame 64
clears it. The exact bounded palette checkpoint is profile 1/uniform `$30`;
production now supplies the exact selected roster profile bit and matchup
uniform color. The fixed roster-slot lineup itself remains native policy.

TGJS-2 revision-locks the prior jump spans plus signed math
`$8001-$815A`, made state 08/state 9 `$AC0A-$AC6E`, distance helpers
`$BCA1-$BDC6`, and the logical lookup `$BDF7-$BEF6`.
It depends on the exact same-pack TGPL/TGCS payloads, rederives the 32-entry
family/profile/direction pose matrix from `$8D3D/$8D5D`, and validates every
resolved TGPL pointer and pose record. Production selection follows Bank05's
source-shaped composition: `$8B12` initializes family 0, Bank02 profile byte 2
bit 5 supplies the profile half, `$9054-$90AF->$8DD3-$8E4D->$BF6C` derives the
eight-way active-hoop direction stored in `$05A0`, and `$842C` combines family,
profile, and direction to index `$8D3D/$8D5D`. The selected TGJS pose becomes a
new court-pose owner, clears the retained pre-tip orientation encoding, and the
renderer applies exactly one facing mirror. Deterministic Away/Home horizontal
and diagonal production checkpoints verify the chosen table pose, profile,
direction, facing, endpoint, release transition, and compositor mirror. This
does not establish family 1: `$8B83-$8BC8` also requires near-hoop,
near-defender, defender-side, and raw `$006A<$9C` inputs that live C does not
retain, so production remains fail-closed on family 0. NES B is tested as a
current level rather than a release edge. The miss actor
held/airborne/recovery states and Q8.8
height/velocity both begin at `$02E8`; height clamps on frame 40 and actor
recovery ends at frame 46 while the ball route remains active through
settlement at frame 87. There is no release DMC; the route-10 ground/bounce conditions
gate `$B5AB` at frame 75. TGSR-4 classifies the TGJS terminal flag's set bit 7
as MISS and supplies the non-current, other-team claimant handler/possession
decision. Frame 87 awards zero points, queues crowd 11 and then side result
12/13 only while the clock is later than 0:01, and hands possession to an
explicitly approximate opposing actor. At period expiry it retains the current
side and crowd 11.

Production family selection follows the proven fail-closed boundary: Bank05
`$8B12` resets `$038C` to family 0, while `$8B83-$8BC8` requires complete
near-hoop/near-defender/defender-side plus raw `$006A<$9C` evidence before
family 1. Because live C does not retain `$006A` at shot launch, it remains on
family 0 instead of substituting a frame-hash bit. A terminal miss with no
strictly eligible claimant uses the generic opposing-team compatibility
handoff and emits no B87C settlement trace; it is not rebound/steal parity.

The make branch follows the bounded two-controller capture: B remains current
through frames 1-8 and releases at 9. The already-selected entry pose 325
(`$028A`) remains visible for frames 1-4, followed by 1060 (`$0848`) for 5-8,
1061 (`$084A`) at 9, 213 (`$01AA`) through flight, and neutral 469 (`$03AA`).
Prepared phases
`31/21/11/01/32/22/12/02` occupy frames 10-17, held phase 34 is frame 18, and
TGSR-4 classifies `$91BC->$933B->$942D`'s terminal bit-clear as MAKE at frame
19. Q8.8 flight begins at 20 from velocity `$0308` under imported gravity
`$0028`. Native uninterrupted physics lands at frame 57, uses recovery phases
`56/46/36/26/16/06` through frame 62, and returns to neutral at 63. The
original capture displayed those last boundaries at 59/64/65 because
non-shot main-loop overruns held display frames 38 and 53; those renderer
stalls are intentionally not native shot waits. Three points and shot-clock 24
apply at frame 85. `$AC0A-$AC6E` then supplies the 4-update initial timer plus
eleven 2-update stages; its state-9 completion after 26 updates owns the
handoff. Frame 111 therefore changes possession and queues crowd response 11
without side-result 12/13. Exact ball/camera motion between the captured
checkpoints remains unproven and native-approximate. Releasing B before frame
8 is normalized to the captured frame-9 transition so ordinary input cannot
strand the live scene; no earlier-release ROM timing is claimed. A period
expiry before settlement applies the frame-85 score exactly once without an
invalid possession/shot-clock reset, retains the shooting holder at frame 111,
and then enters the normal settled-action period path. Unknown contexts,
ordinary two-point makes, and other native-policy branches are rejected without
substituting the former synthetic schedule.

Bank05 `$83E9-$842B` plus `$8469-$847A` explains that visible windup more
precisely than the capture alone. State `$1E` leaves the actor pose untouched
during its first `30/20/10/00` phase cycle, then advances the facing index and
selects the next raw pose from `$0846/$0848/.../$0854`; `$842C` eventually
commits the `$01AA` airborne pose. This is why the bounded make and miss
captures legitimately begin with different values (`$028A` and `$030A`) yet
both continue through `$0848`, `$084A`, and `$01AA`.

The live terminal-miss route still owns the separately bounded `$0C/$0D/$0E`
physics and 87-frame ball settlement; the port does not claim that a complete
state-`$1E/$0B` miss scheduler has been reconstructed. A separate visual
adapter now preserves the shooter's actual post-movement entry pose for held-B
pose ticks 1-4, uses 1060 for ticks 5-8, displays 1061 on B release, and changes
to 213 on the following route update. Releasing B early therefore produces a
compact entry/release/flight transition instead of inventing extra physics
frames. The pose order is ASM-correlated and capture-bounded, while its
composition with the shorter miss route and horizontal mirroring remains
explicit native policy. Runtime still consumes only the validated asset pack;
ASM and captures are verification inputs, never runtime dependencies.

TGSR-4 is 608 bytes with FNV1a32 `5376E82B` and FNV1a64
`FACCE42B52382D6B`. Its importer revision-locks Bank05 `$91BC-$943A`
(outcome calculation/bit helpers), `$A6EE-$A9D9` (numeric rim dispatch),
`$B73E-$B87B` (claimant scan/proximity), `$B87C-$B8F5` (claimant-driven
settlement), `$BA56-$BA9C` (full incoming caller gates; FNV1a32 `B779AC48`,
FNV1a64 `367ED7AC43F1ACA8`), `$9042-$9053` (selector toggle;
`CE6C9466`/`EC5906B34DC6D566`), and `$B98B-$B994` (candidate remap table;
`404311FE`/`7CCF6AAD4241C4FE`) as explicit strict source descriptors. The
previous `35FB80C4` was only `$BA65-$BA9C`, not the claimed full caller span.
Runtime requires its exact same-pack TGPL-1 dependency. Missing,
malformed, undersized/oversized, wrong-revision, or cross-pack TGSR data rejects
the scene before it becomes available; no capture, trace, ASM, decompilation,
or ROM is a runtime input.

TGSR-4 also carries the exact 124-byte Bank05 `$BEEF-$BF6A` arc boundary
table (FNV1a32 `9EF1061B`, FNV1a64 `E8A0728513DD8BDB`). Its pure C API
reproduces `$B995` point classification: nonzero shot-flag low bits yield 1;
otherwise raw world X/Y and orientation 0/1 yield 2 or 3 through the original
`$5B..$D6` Y range and low-byte subtraction/high-byte borrow. Same-pack
TGPL-1 already revision-locks the classifier routine at `$B995-$BA3F`, so
TGSR does not duplicate those bytes. This exact classifier is only a rules
foundation. TGJS-2 exposes `$AD4E->$B32C->$B100` as a strict pure API with
explicit raw launch inputs, but live ordinary two-point makes remain
unsupported because exact `$AD6E` launch ownership is missing. The captured
three-point score/handoff cannot be inherited. `$91BC`'s pure evaluator is
understood, and eight py65/ROM goldens agree. A live adapter would still need
shooter/side/control/context/orientation; all ten actors' flags,
matchups, and X/Y; ratings, property, motion, condition, difficulty, CPU
adjustment, scores, and raw `$6A/$53`. Those inputs are unavailable, so the
selector remains approximate and is not wired.

TGSR-4 retains the exact state-`$15` rim-rattle contract in metadata bytes
29..63. It carries state
`$15`, orientation starts `$009D/$0263`, Y `$93`, horizontal magnitude
`$0040`, altitude `$38`, timer 4, the one-to-four-pass derivation, repeat DMC
length `$0A`, eight render-script selection addresses, and two exit
render-script addresses. Those addresses are source selection identities, not
literal sprite or CHR IDs. The native state API saves incoming velocity, moves
one coordinate per update, reverses after each four-update nonterminal pass,
requests address-bound A8D6-short on each repeat, and restores velocity at the
terminal boundary. Seven primary plus four focused provenance spans cover
`$A2DF-$A2F7`, `$AD4E-$AD64`, `$BDF3-$BDF6`, and `$BEEF-$BF6A`
in addition to the primary
sources. The mapper obtains launch-target X from required same-pack TGCS-1
`$BDEF-$BDF6` and cross-checks the snap bytes against TGSR-4; `$AD4E-$AD64`
proves the BDEF/BDF1 loads and target Y `$8F`. The terminal convergence
remains conditional: nonzero `$036F`
or raw `$6A >= $18` enters `$A8E9`; the other branch requests the long A8D6
clip, clears the miss flag, and relaunches state `$05`. Only the deterministic
debug/test route uses observed `$6A=$71` plus a sign-only negative sentinel,
producing the visible positive-first four-pass route, state `$10` at frame 89,
and settlement at frame 103. The incoming horizontal sign is proven, but its
exact magnitude is not; the generic state API preserves and restores the
caller's supplied horizontal and vertical values. The exact raw orientation-0
snap `(157,147)` is mapped relative to the ROM launch target `(160,143)` and
the native shot endpoint; it therefore renders beside the hoop rather than as
direct screen coordinates. Normal live `gameplay-jump-frameN` behavior and
its frame-87 settlement are unchanged; no selector or RNG is invented.

TPNL-1 `gameplay/penalties` is a strict 768-byte pure rules foundation
(FNV1a32 `980DDC76`) with same-pack TGPL-1 and TSFX-1 dependencies. It exposes
bounded classification and presentation data without inferring contact,
collision, possession, or route state. The one bounded live scene route is the
human defensive-B selected-pair/contact bridge: it supplies only the observed
ordinary fallthrough adapter, lets TPNL classify defensive pushing, and passes
the resulting counters/attempts through the separate state request. After that
request succeeds, the scene retains typed presentation identity for the
Bank02 overlay; CPU/special/raw-route callers remain unsupported.

TGVR-1 `gameplay/violation-referee` is a strict 4752-byte ROM-only visual
foundation (FNV1a32 `2EB08CF0`) with exact same-pack `chr/all` and TPNL-1
dependencies. It decodes screen `$05`, maps the seven violation strings with
Bank03's character table, and preserves all Bank04 referee metasprites and
gesture lists. Shot clock selects `9,10,10,10`; out of bounds selects
`3,4,5,5,5`. The controller consumes 44 frames before fixed `$EA14` begins its
4+120-frame release path, making the native violation phase 168 frames. The
nine-frame black loader interval and four-frame visible fade alignment remain
capture-bounded. The scene consumes strict TPNL-1 presentation metadata to
request shared SFX 6 at presentation frame 16 exactly once. The supported
ordinary defensive-pushing route also renders Bank02 `$B0F8-$B398`'s exact
defensive/pushing, roster name/number, counter, and fouled-out cells over this
screen using the existing ROM-derived THUD/TTDT font/CHR assets. `$E95E` orders
selector `$2C` before `$22`; the renderer preserves selector-0 groups
`1,2,2,2` and suppresses court actors/ball. Bonus is only the proven Bank02
side mask—no visible BONUS text is inferred—and Bank02's dynamic PPU completion
timing remains unestablished. Other full presentation/caller parity remains
incomplete.

The `gameplay-out-of-bounds-frameN` checkpoint reaches TGVR through the live
TGMO primary-holder clamp and TPNL selector 1. Visible frames 23, 27, and 31
prove the distinct ROM groups 3, 4, and 5; later checked frames retain group 5.
This path remains limited to TGMO's documented boundary latch.

TGBC-1 adds the separate strict live backcourt detector. Its 512-byte payload
(`810886EF`) imports Bank05 `$970B-$9786` (`C137674F`), pins the complete Rev1
ROM identity, and requires exact same-pack TGOR-1 (`44B0C44E`) and TPNL-1
(`980DDC76`). `$971F-$9786` is implemented exactly for ordinary object state
zero: `$0588` bit 4 records frontcourt progress, orientation 0 establishes at
ball X `<=375` and returns at X `>=386`, orientation 1 establishes at X `>=392`
and returns at X `<=383`, and the violation stores selector 2. The C step keeps
the 6502 unsigned 16-bit subtraction, high-byte sign branch, and low-byte
`$0A/$F8` comparisons. The selector-4 ten-second prefix at `$970B-$971E` is
retained as provenance but is not implemented by TGBC-1.

Production samples the TGBD-attached held-ball coordinate once after ordinary
human and CPU movement, resets the detector at the scene possession-handoff
boundary, resolves BACKCOURT through TPNL, and uses TGVR's exact shared
`3,4,5,5,5` pointing sequence and ROM lettering. This sample/reset scheduling
is a bounded native adapter, not a claim of exact 6502 intra-frame caller
ordering. The referee controller/groups are exact; the existing nine-frame
blackout/fade alignment remains capture-bounded. The deterministic
`gameplay-backcourt-frameN` checkpoint drives that live route and shows
distinct groups at frames 23, 27, and 31.

OUT OF BOUNDS and BACKCOURT restarts now enter an explicit inbound
setup/gather/flight/catch lifecycle instead of handing the ball immediately to
the first roster slot. The setup decoder reuses TGFL-1's revision-locked
Bank06 `$9621-$9764` base branch and `$9879/$9881` table selection to place all
ten actors at the source coordinates. It keeps the `$0308`-shaped primary/passer
and `$0309`-shaped selected defender distinct. `$976F-$985C` is deliberately not
used: that branch is conditional on unowned `$BA` state and was not taken by the
observed inbound route. Bank05 `$B074` obtains its target through
`$037F[$030A]`; the native scene uses that typed candidate when valid and
otherwise labels and uses its existing nearest-teammate adapter rather than
claiming exact receiver selection. During setup and the shared `$32,$22,$12,$04`
gather/flight sequence, clocks, controls, and AI stay frozen. The typed
`$B24F`-shaped catch attaches the ball to the receiver and ordinary live play
resumes on the following update. Focused coverage includes OUT OF BOUNDS and
BACKCOURT for both teams with game music enabled and disabled. Exact restart
player selection, `$976F` behavior, pass duration/substeps, and a complete
object-slot-10 inbound trajectory remain outside this boundary.

TGFL-1 `gameplay/free-throw-lineup` is a strict ROM-only lineup foundation
and live-scene dependency. Its pure resolver remains separate from scene
mutation. The 1216-byte payload has
FNV1a32 `B17B9A3F`, depends on exact same-pack TGPL-1, and retains the complete
Rev1 Bank06 spans `$88B0-$88D9` (`AD834719`), `$9621-$976E` (`998D84B8`),
`$976F-$985C` (`FB7680EF`), and `$985D-$9918` (`AFB31306`). Exact source
records, zero-reserved fields, raw-section and canonical fingerprints, the
full-ROM revision fingerprint, exact-size reads, and source-map provenance
reject missing, malformed, undersized/oversized, wrong-revision, and
cross-pack data.

The pure resolver takes orientation 0/1 and explicit, distinct shooter and
secondary slots. It reproduces the descending raw coordinate and
pose-direction stream consumption, including the shooter-dependent skip,
without clamping or projecting raw 16-bit world X and raw 8-bit world Y.
For non-shooters, `$88B0`'s raw even pose byte offsets map to TGPL pointer
indexes with `offset/2`; the validated set is `517..520`, not `1034..1040`.
The exact base state seeds are raw `$046E/$057C/$0458/$0479/$048F =
00/01/30/C1/00` for non-shooters and `20/01/30/41/00` for the shooter. The
shooter pose remains preserved/undefined because the follow-up routine does
not call `$88B0` for that slot. The API intentionally omits side-control inputs
and therefore does not apply the conditional shooter script override or
secondary raw phase `$15`.

On free-throw entry the live scene synchronizes scoring-team possession through
TGOR-1, selects the corresponding TGFL orientation, and copies every exact raw
X/Y value into the ten canonical actor positions and anchors. It then performs
one typed TGCP-2 settle on the shooter coordinate: orientation 0 reaches camera
`$0066`, orientation 1 reaches `$0198`, and the existing free-throw freeze
holds that camera. The rendered frame uses the matching TGCT slice and TGCP
actor projections. Shooter selection, secondary selection, held-ball
attachment, and this camera composition are native adapter policy, not new ROM
claims. The live scene preserves its existing actor poses; TGFL pose/state and
conditional side-control script overrides are not applied. Aiming, attempt
decrement/outcome policy, rebound, and CPU positioning/scripts remain
approximate or unsupported.
Run `tools\Run-GameplayFreeThrowLineupTests.ps1 -Build -RomPath
<LOCAL_ROM.nes>` for parser/API, provenance, mutation, revision, and dependency
coverage.

TGOR-1 `gameplay/court-orientation` closes the strict binary offensive-
direction ownership slice and is loaded by `TecmoGameplayScene`. Its 640-byte
payload has FNV1a32 `44B0C44E`, requires exact same-pack TGPL-1
(`2047CCE0`) and TGSR-4 (`5376E82B`), and revision-locks Bank05
`$8FAD-$8FE7` (`7C94E5EA`) as the possession transition gate-and-swap,
`$9042-$9053` (`CE6C9466`) as the slots-0..9 `$04B0` bit-`$10` toggle plus
queue-`$17` operation, `$9054-$90AF` (`FE092D62`) as the absolute target-
delta routine, and `$BDEF-$BDF2` (`A27B0F6F`) as target table
`$00A0/$0260`. `$9042` is not generalized into a team-switch label.

Fresh native launch initializes direction 0, previous direction 0, transition
serial 0, target `$00A0`, and the existing initial AWAY possession. This is a
cold-start-aligned native policy; original repeat-game initialization remains
unproven. Same-possession handoff, period restart, and foul restart succeed
without changing orientation state. On a real tracked possession change, the
API transactionally saves current as previous, XORs current, updates the team
and target X, and increments the serial. Scene handoff calls this sync even if
the rules state already changed possession; invalid input rolls both states
back. TGCT-1 stays in its canonical left-to-right orientation.

The same-pack TGPL-1 cross-check at fixed `$E537-$E548` is presentation
selection evidence only: it derives `$0758` from `$04FC` bit 7, the slot-10
horizontal-velocity high byte/sign, and uses IDs `$1B/$2E` from
`$E699-$E69A`. It does not own orientation. TGSR-4 `$B87C-$B8F5` is a
conditional alternate claimant-settlement path rather than a universal post-
shot path. `$035B` is save-before-toggle evidence only and has no direct
reads. The only direct `$035A` stores are `$8FC4` and `$B8E0`; broad
`STA $0300,X` is found only in fixed-bank cold boot at `$CC68`.

TGOR-1 now supplies production TGCP follow direction and `$00A0/$0260`
world-space shot targets; launch Y is the separately proven `$8F`. Shot launch
rejects an inactive or non-possessing actor, mismatched rules/TGOR team state,
or a target that differs from the validated orientation asset. Once resolved,
the scene faces the actor toward that hoop, freezes the endpoint, and lets TGCP
follow by at most its normal seven-pixel update cap. Base actor facing also
comes from the validated team-to-goal mapping: the tracked possession team owns
`attack_direction`, the other team owns its XOR-1 direction, a left hoop means
`facing_right=false`, and a right hoop means `facing_right=true`. Possession
handoff rebases active actors transactionally; deliberate horizontal movement
and supported shot actions are the only native overrides. This mapping remains
native adapter policy rather than a newly claimed ROM animation selector.
TGOR-1 also selects the production TGFL-1 lineup orientation. Exact-size and
canonical payload checks, source records, bounds/reserved/padding checks,
full-ROM SHA/FNV revision identity, same-pack dependencies, source-map
provenance, missing/malformed/oversized and source-mutation failures are
covered by
`tools\Run-GameplayCourtOrientationTests.ps1 -Build -RomPath
<LOCAL_ROM.nes>`.

TGCP-2 `gameplay/camera-projection` supplies that exact projection as a strict
pure API and production live-scene dependency. Its 1536-byte payload
has FNV1a32 `53247856`, requires exact same-pack TGPL-1 (`2047CCE0`) and
TGCT-1 (`ECAB7A93`), and retains fixed-bank Rev1 initializer
`$DE13-$DE2C` (`A5CF7665`), column streamer `$DF05-$DFFF` (`7BC5351D`),
attribute helper `$E0E7-$E13B` (`7FE800D4`), threshold/follow routine
`$E168-$E2E6` (`19038AEA`), forced settle `$EB4F-$EB8C` (`AF5725C0`),
actor projector `$F1CB-$F1F1` (`CB8BD081`), and movement clamp
`$F106-$F1B0` (`CB1D4EAF`, SHA-256
`0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6`).
Exact source records, descriptors, reserved bytes, alignment padding,
raw/canonical fingerprints, full-ROM identity, and source-map provenance fail
closed on malformed, wrong-sized, wrong-revision, mutated, or cross-pack data.

The pure state API initializes camera X `$0100`, scroll/page zero, stream
direction zero, and layout cursor `$20`. Follow updates reproduce orientation
threshold selection, the endpoint/generic two/seven-pixel caps, cursor limits,
scroll carry/borrow page toggles, direction reversal, and coarse-column cursor
updates. Forced settle is bounded to 1024 iterations and transactional. Actor
projection is the exact unsigned 16-bit subtraction: X is visible only when
the difference high byte is zero; Y is unsigned `world_y-altitude` saturated
to zero on borrow for visible actors. Valid offscreen projection is not a
parser failure and returns the deterministic native sentinel
`visible=false, screen_x=0, screen_y=0`; fixed `$F1CB` branches before its Y
calculation, so the zeros are not claimed as ROM-written coordinates. Invalid
calls leave state/output untouched.

Production launch preserves the pure `$DE13` cursor `$20`, applies one bounded
`$DDFB->$DF05` first-column prime to cursor `$21`, seeds actor/anchor and ball
world coordinates at camera `$0100`, then settles once after seeding. Every
subsequent live update follows exactly once after all object/ball mutations,
with ball world X, TGOR direction, and action route 0. Free-throw entry
performs the documented TGFL-driven typed settle; subsequent free-throw
updates and TGDK black/cutaway frames freeze camera state. The first live-return
update resumes it. Possession changes clear only threshold validity and the
endpoint latch, never camera position.

The production validator rejects states that the pure synthetic-test API may
legitimately construct. It requires scroll low byte/page consistency and the
reachable cursor relation: rightward states use
`min((camera_x >> 3) + 1, $34)`, leftward states use
`max((camera_x >> 3) - 1, $0B)`, and a latched threshold set must match one of
the three exact ROM-derived threshold pairs. Direct live-prime settle goldens
are `$0066/$0B` left and `$0198/$34` right; pure unprimed settle remains
`$006E/$0B` and `$01A0/$34`.

The focused runner also composes the assets independently of the production
scene adapter: it loads TGFL-1 and TGCP-2, derives orientation 1, shooter slot
6, and secondary slot 1, then proves the exact ten raw X/Y values,
76 moving updates from the capture-derived cursor `$21`, the unchanged 77th
update, transactional settle at camera `$0198`, and visible slots
1/2/3/5/6/7. The other four slots use the neutral offscreen sentinel.
Secondary slot 1 and cursor `$21` remain explicitly bounded frame evidence.
Pure TGCP tests independently cover seven-pixel left/right movement,
camera-disabled and routes `$01/$12/$13` no-ops, page carry/borrow, continuing
coarse cursor steps, and three-column direction reversals.

TGCT-1 now supplies the strict pure court boundary without changing the
6559-byte payload, `ECAB7A93` canonical fingerprint, entry count, or raw
source spans. The existing 15-row by 48-column little-endian macro layout
expands left-to-right into caller-owned 96-by-30 tile and per-tile-palette
planes (768-by-240 pixels). Decode bounds-checks all 720 macro references,
derives macro-index min/max/unique 0/360/346, and requires the exact tile and
palette-plane FNV1a32 values `6458B5E5` and `7F650645`. It independently
cross-checks the 32 center columns at camera X `$0100` against the legacy 960
tiles and palette indexes expanded from the existing 64 attribute bytes.

The pure camera-positioned slicer accepts X 0..`$0200`. It reports
`first_tile_x = camera_x >> 3`, `fine_scroll_x = camera_x & 7`, and returns
fixed row-major 33-by-30 tile/palette planes: 32 columns for aligned cameras
with a zero unused tail cell on every row, otherwise all 33 columns needed for
fine scrolling. The caller-owned world carries a contract tag, immutable
golden metadata, and plane fingerprints; the slicer recomputes and validates
both hashes before assignment. Decode and slice are transactional on NULL,
unavailable, corrupted, or out-of-range input.

Validation evidence, not a new TGCT runtime span or dependency, ties this
interpretation to fixed `$DDCE`'s init ownership, `$DE13/$DE2D` initialization,
the `$D5C5` macro builder, `$DDFB->$DF05` first prefetch, `$DF6A`'s `$60`-byte
row walker, and `$E0E7` attribute handling.

The live scene now decodes the 768-by-240 world and slices 32 columns when
aligned or 33 when fine-scrolled. It draws through a 256-by-240 framebuffer
subview so partial edge columns clip without writing adjacent margins. Actors,
anchors, ball Q8 coordinates, shot start/end, movement, proximity, passing,
switching, and AI use coherent world coordinates. TGCP applies jump altitude
once to the actor and never again to the ball; offscreen objects are skipped.
Draw preflight revalidates the camera/world, CHR offsets, palettes, and every
pose before writing.

`TecmoGameplayCourtCoordinate` is the canonical object-space contract over
that decoded court: `(0,0)` is its upper-left pixel, X grows right, and Y
grows down; valid integer anchors are X `0..767`, Y `0..239`. Players and
their AI-return anchors store integer coordinates. Ball and shot positions
use `TecmoGameplayCourtCoordinateQ8` with the same bounds plus eight
fractional bits, and conversion/projection floors only after validating the
whole Q8 value. TGOR stores both ROM-backed hoop anchors as complete
coordinates, left `($00A0,$94)` and right `($0260,$94)`, while ordinary shot
flight retains its distinct proven endpoint Y `$8F`.
`tecmo_gameplay_scene_court_coordinates` transactionally snapshots all ten
players, the Q8 ball, and both hoops. Live ownership validation and draw
preflight reject an invalid player, anchor, ball, active shot endpoint, or
hoop before committing output or rendering. The coordinate convention and
hoop values are exact within the cited TGCT/TGOR evidence. The static tip-off
player coordinates and ball anchor are also exact within Bank04
`$AC8C/$ADA3/$ADAE/$ADB9`; the post-handoff live layout and animated tip
geometry remain native or capture-bounded.

The existing TGCP raw APIs remain the exact evidence boundary. Thin
transactional adapters now accept the canonical coordinate types:
`tecmo_gameplay_camera_follow_court` and
`tecmo_gameplay_camera_settle_court` validate/floor the Q8 ball once before
calling the raw follow/settle routines; `tecmo_gameplay_camera_project_court`
and its Q8 counterpart call the raw projector after coordinate validation.
The live scene uses these adapters for launch settle, each single live follow,
pre-tip handoff settle, and drawing. One
`tecmo_gameplay_scene_court_projection` result owns the current camera X, ten
player projections, and ball projection transactionally. Jump altitude is
applied once to the shooter; the ball receives none. Offscreen objects retain
the exact TGCP neutral sentinel. The adapters are integration code and do not
claim additional ROM coordinate conversion semantics.

Live actor/background composition uses one transactional
`tecmo_gameplay_scene_court_frame`. It combines the TGOR-tagged TGCT slice,
all ten TGCP player projections, and the ball projection with the current
scene frame and camera-follow serial, rejecting any camera-X mismatch before
drawing. Stationary visible actors move by exactly the inverse signed camera
delta and retain Y through horizontal camera motion; visibility transitions
retain the exact neutral offscreen sentinel. Sweeps cover fine scroll,
coarse-tile crossings, possession reversal, and both endpoints. Focused
possession travel proves native camera/slice checkpoints at
X `102`, `256`, and `408`; background-only RGBA FNV1a32 values are
`4F52BCC1`, `9CC9CD31`, and `033B45D5`. These freeze the native integration.
They are not emulator-frame hashes and do not elevate the native checkpoint
placement or possession choreography to ROM-exact behavior.

TGMO-1 `gameplay/movement` is the exact ordinary actor-locomotion
boundary. Its 1664-byte payload has FNV1a32 `6C82A137`, requires exact same-pack
TGPL-1 (`2047CCE0`), TGCP-2 (`53247856`), and TTDT-1 (`812628F0`), and
byte-compares its clamp copy with TGCP-2. Seven Rev 1 spans are imported:
Bank02 `$A89E-$A90D` (`0BD2CB61`), Bank04 `$ACE4-$AD25` (`36A1B92C`),
Bank05 `$879B-$8866` (`E05FE645`), `$88F9-$89BC` (`613D0B4C`),
`$8E58-$8F96` (`A32D3C92`), `$BF6C-$BFA7` (`71812CB0`), and fixed
`$F106-$F1B0` (`CB1D4EAF`). Header constants, descriptors, source records,
alignment padding, canonical payload fingerprint, full-ROM SHA/FNV identity,
source-map provenance, and dependencies are strict and fail closed.

The transactional C kernel retains direction bits right/left/down/up
`1/2/4/8`, one-update action-state latency, Q4 fractional accumulation,
`amount-floor(amount/4)` diagonal reduction, `$4A/$EC` compare-before-move
vertical gates, and the period-8 movement animation state. Actor amount uses
TTDT profile byte 0, GAME SPEED `+5/-1/-6`, and
`max(8, adjusted_rating + (condition >> 4) - 6)`. Malformed tags,
coordinates, action/direction values, fractional or animation phases,
condition/speed values, unreachable rating arithmetic, and overflow reject
without committing. The pose-base plus animation-low-nibble lookup is exact;
the live scene now uses it for initial, controlled-player, and TGAI-driven CPU
court frames.
Metasprite CHR selection follows the high two bits of the ROM-generatable
`$01/$41/$81/$C1` slot and rebinds that R2-R5 selector to the pose record tag,
so the `$C1` ball uses R5 instead of inheriting the old hardcoded R3 path. The
scene applies `$8F02`'s exact signed linked-minus-selected comparison to choose
the primary or alternate half. Its fixed opposing roster-slot link remains
scene policy, not reconstructed ROM matchup ownership. Do not
draw the matchup-card team logo in the on-court actor pass; it is presentation
data and previously appeared as a false sprite attachment during the toss.

The fixed span is implemented as its selected-actor dispatcher rather than an
unconditional clamp. Object state 4, action `$0F/$10`, the flags-bit-3
exemption, and the exact conditions that set the boundary-violation latch are
retained around page-0 `$00DF-floor(Y/2)`, page-1 interior, and page-2
`$0220+floor(Y/2)`. The ordinary live adapter supplies object state 0 and flags
0. Only the offensive primary/ball holder may raise the latch. Selector
`$0742=1` is resolved through same-pack TPNL-1 as OUT OF BOUNDS, the latch is
cleared, the exact 4+120-frame presentation is entered, and the existing rules
state owns restart possession and shot-clock/divider reset. Other ROM violation
detectors remain outside this slice.

TGBD-1 `gameplay/ball-dribble` is the strict ordinary held-ball animation
boundary. Its 608-byte payload has FNV1a32 `E2CE6BFF`, requires exact same-pack
TGPL-1 (`2047CCE0`) and TGMO-1 (`6C82A137`), and imports Bank05
`$B52E-$B5BF` (`DB540670`) plus `$B5C0-$B677` (`E9784D28`). The contiguous
source fingerprint is `9579D729`. It retains the exact two-half,
eight-direction, eight-phase height lookup, signed X/Y holder offsets, and the
DMC trigger at animation low nibble 3 with high nibble 0. Header constants,
descriptors, source records, alignment padding, canonical fingerprint,
full-ROM identity, same-pack dependencies, table semantics, movement state,
coordinate arithmetic, and output commits are strict and fail closed.

During ordinary live possession, both human and CPU holders use TGBD to keep
the ball attached. Human/legacy movement and the supported automatic selected-
primary flow advance TGMO/TGBD cadence. Selected primary runs once through
`$8374->$83F3->$8491` before `$8284-$82A5` skips duplicate ordinary dispatch.
Active shots and free
throws retain their separately owned ball routes. `$8F02`'s signed comparison
is exact, but the opposing actor supplying that comparison is still a fixed
scene-owned roster link. The native scene converts the exact height to visible
canonical Y before TGCP projection; this flattening adapter and complete 6502
caller scheduling are not claimed as exact ROM behavior. Deterministic render
checkpoints `gameplay-ball-bounce-frame1` and `frame12` freeze visibly distinct
high and low positions.

Ordinary human passes and autonomous action-`$21` CPU passes share a visible,
actor-neutral transactional lifecycle. Typed automatic ownership plus the
supported ordinary `$05A1=0` context admits selected-primary state 4 through
`$8374->$83F3->$8491->$8B90`. Canonical Bank06 `$8FC5-$8FE7` copies `C9=$21`
to `$046E[X]` at `$8FCA/$8FCC` and advances the five-byte cursor. Bank05's
selected-primary table consumes `$21` at `$89D7` in the same update; then
`$8284-$82A5` skips `$0308/$0309` in the ordinary loop so the command cannot
double-step. Human selected primary stays excluded and raw `$030C/$030D` is
not used as a controller mirror. `$89D7`
writes state `$0F` and seeds packed gather
`$32->$22->$12->$02->$03->$04` through
`$8999/$9C29`; `$8695/$86A8-$86B7` releases directly into shared `$B074` at
the full byte `$04`. The direct route is observed with
slot-10 `$0478=$13`, so `$B074` is not state-`$03`-only. `$B074-$B0FD` locks
the typed `$037F[$030A]` receiver and swaps the `$000E/$037F`-shaped roles at
launch while the `$0308`-shaped native holder remains the passer. Genuine
Bank05 `$B24F` (`AC 0A 03`) alone changes holder/primary at catch; CPU transport
has no controller and therefore cannot mutate human control. The observed
actor-2 route locks receiver actor 4 and genuine `$B24F` later stores 4 to
`$0308`; `$B2FA-$B300` clears raw `$BA` bit 2 without a broader inferred name.
The C flight
duration and linear interpolation remain bounded native adapters because
`$B42F/$BB9F/$BBA0` and the `$B1E7/$B500` five-substep scheduler are not yet
strict assets. An isolated exact `$BD6E-$BDC6` helper preserves uint16
wrap/carry and six logical shifts, but does not make the unseeded trajectory
exact. Broader pass-selection policy, unsupported selected-primary gates/states,
`$B13F` interception/contact, and complete object-slot-10 parity remain
deferred. See
`docs/pass-defender-handoff-evidence.md` for the separately bounded defender
handoff and dynamic-link limitations.

The genuine catch call continues through
`$B2EC->$B2FA->$96B6-$9708`. Human offense retains the receiver's temporary
state-0 endpoint; automatic offense writes action `$18`, selects `$007D` or
`$00D7`, and returns in state 4. The scene owns typed automatic control but not
the raw same-call `$0373/$0095/$0094` route chooser, so it chooses source-valid
long route `$00D7` as a labeled native approximation. Its first exact opcode-2
target drives TGMO. Opcode 21 receives exact typed shot/game clocks and the
unowned raw `$007E` bit-1 clear branch as a labeled approximation, allowing
the stream to advance. The alternate
selected-primary state-6 wait also advances once per update and does not fetch
again until the update after its zero transition. Ordinary passes and inbound
catches share this atomic endpoint.
Selected state 0/action `$17` then has a distinct Bank05 owner:
`$81F2-$822F->$8A6D->$8ACE` enters shot initialization. The pointer dispatch
is exact; launch admission remains a bounded adapter because `$8ACE` consumes
unowned gates. The existing source-backed automatic close playback seam is
wired transactionally.
Far/jump playback still depends on
unowned controller-team state, so LIVE explicitly recovers that rejected
candidate to state 4 with `$17` cleared rather than freezing the holder.

TGFT-1 `gameplay/fatigue` is the strict fatigue-evolution boundary. Its
512-byte payload has FNV1a32 `F80F170D`, requires exact same-pack TTDT-1
(`812628F0`), and imports Bank02 `$B4E6-$B5C7` (`F61DFFF7`) plus fixed
`$ED2F-$ED3E` (`09342B88`). It preserves difficulty cadence reloads `6/4/1`,
active countdown/capacity/condition decay, bench recovery, and Rev1's distinct
second-team recovery-countdown store. Maximum capacity comes from TTDT profile
byte 3. The live scene ticks it once per live-action scene update and supplies
the resulting condition to TGMO on the following update; exact 6502 intra-frame
actor ordering is not claimed. Active lists remain the fixed scene roster slots
`0..4`, so original starter selection and substitutions are still unported.
Opposing directions on one axis are normalized to neutral as a native
integration policy. Initial actor placement/direction, the current fixed
five-player roster-slot/matchup-link binding, and CPU target selection/AI remain
native integration or approximations. Supported automatic selected primary
runs its source command once before ordinary-loop exclusion. Eligible
non-selected automatic actors then advance source streams in Bank06's descending
`9..0` order. Validated source actor/object targets follow the immutable
snapshot, while direction-only records use a bounded target-composition adapter.
Missing target workspaces defer without movement instead of falling back to the
older five deterministic formation points and goal-side defender offsets. Those
fixed points remain legacy/native compatibility policy rather than current
bound-production targeting. Supported actors use the exact TGMO step after TGAI
direction selection where the source input is owned. The
deterministic `--gameplay-movement-harness` is console/test-only and never
enters normal play.

TGAI-3 `gameplay/cpu-steering` is the separate strict CPU target/direction
evidence boundary and a strict production scene dependency.
Its 8016-byte payload has FNV1a32 `D56EE070`, requires exact same-pack TGMO-1,
and retains twelve Rev 1 spans: Bank06 `$81F7-$82D3` (`23BB7271`),
`$87AE-$88AF` (`F866B06C`), `$88DA-$8A95` (`9616E586`),
`$8A96-$8AF3` (`939C6882`), `$8AF4-$8B8F` (`C2E05331`),
`$8B90-$8BE0` (`9AD2BA91`), `$8BE1-$9237` (`344298FE`),
`$9280-$9329` (`C82E6853`), `$938B-$9620` (`47818A62`), fixed
`$C006-$C008` (`14B2472E`) and `$CBE0-$CBF6` (`41C5B5C8`), plus Bank04
`$9F2E-$AC75` (`71331A96`). Full-ROM identity, descriptors, source records,
padding, handlers, commands, canonical payload, provenance, and dependency
fingerprints fail closed.

The exact state-4 transport adds the actor-local `$0547/$0551` offset to
Bank04 `$9F2E`; fixed `$C006->$CBE0` maps Bank04, copies one five-byte record
to `$C7-$CB`, restores the previous bank, and Bank06 dispatches `$C7` through
24 handlers. The bounded Bank04 range contains 680 aligned records and code
resumes at `$AC76`. Bank06 `$938B-$9620` proves a formation-stream assignment
path into those actor offsets. It does not prove every play-selection input.

The pure direction API reproduces `$88DA-$899D`'s target-minus-actor octant
decision and `$8A8E` map. For court-reachable deltas, a magnitude ratio of 2:1,
including equality, selects the dominant cardinal axis; otherwise the result is
diagonal. The API also preserves the 6502's wrapping 16-bit doubling at
synthetic extremes. Direction codes are `0..7` = right, left, down,
down-right, down-left, up, up-right, up-left. The target-application guard at
`$92D4-$92DD` keeps the prior direction by skipping the `$92FE` jump to
`$88DA` on a zero vector; the C API returns false transactionally for that
no-write case. Aligned command inspection
reports raw opcode/arguments, exact handler CPU address, and only a bounded
entry-effect category—not a semantic play name.

The deterministic steering evaluator accepts a selected actor, all ten
TGCT canonical X/Y coordinates, possession, TGOR orientation, a
possession-consistent holder, an explicit opposing linked/matchup actor,
difficulty `0..2`, and an optional validated explicit target coordinate. It
validates the complete snapshot transactionally and prints every coordinate
plus a domain-separated canonical FNV1a32 fingerprint. Default and explicit
targets remain deterministic harness/native policy. The live scene instead
consumes accepted source targets/directions from `live_foundation.play_state`;
direction-only target construction remains a documented adapter. The resulting
nonzero target delta alone consumes the exact TGAI
octant quantizer; zero delta reports a successful keep-direction/no-write
result.

The separate deterministic
`--gameplay-cpu-steering-movement-harness` composes this boundary with TGMO-1
without adding an in-game debug route. The live scene calls the same pure
adapter directly. The CLI adds explicit rating, condition, GAME
SPEED, and frame-count inputs, initializes a CPU actor facing its offensive
hoop, and requires the selected snapshot coordinate to equal the transactional
movement state on every step. Each nonzero exact TGAI direction is converted
through TGMO's validated direction map to NES held bits and passed to the
role-coherent TGMO kernel (primary for the holder, secondary otherwise). The
resulting coordinate is copied into the next
ten-actor snapshot before direction selection repeats. Thus TGMO's
one-update latency, Q4 accumulator, diagonal reduction, animation, and court
clamp are exact within the composition. TGAI's zero-vector no-write has no NES
held-input equivalent, so neutral is a native harness policy; the target/link,
initial-facing, and profile inputs are likewise caller/native policy rather
than reconstructed CPU command behavior.

For live ordinary movement, the scene takes one immutable ten-actor snapshot
after human input, evaluates the supported automatic selected primary first,
then eligible non-controlled non-selected actors in descending `9..0` source
order, and commits candidates together. The primary command executes once
before ordinary-loop exclusion; exact opcode 9 action `$21` may enter the shared
pass transport. Other actors use accepted source targets/directions or remain
inert with typed defer reasons. Shot proximity, direction-only target
construction, pass duration/interpolation, and unsupported selected-primary
gates/states remain approximate/fail-closed.

The Bank06 common target tail has one additional, deliberately narrow live
owner. `$92CA-$92D0` tests only `$BA & 3` before `$8FD9` advances the actor's
five-byte command record. When the scene is ordinary LIVE with no result or
pre-tip abort, violation, free throw, shot, visible pass, lineup, or dunk
presentation, C projects only the proven zero low-two-bit condition. It does
not mirror the `$BA` byte, derive it from a frame counter, or claim its other
flags. All transient contexts remain `missing-ba-lifecycle`. A natural,
non-injected PRETIP-to-CPU regression now proves the selected `$0308` primary
receives its dedicated `$8374-$83F3 -> $8491` command step before the ordinary
actor loop: its `$0A41` record advances once, while a nonselected actor also
advances through ordinary transport. The later `$8284-$82A5` loop still skips
the selected primary and defender, preventing a duplicate command step. This
establishes the bounded Bank06 selected-primary order, not a complete CPU
playbook, holder policy, passing decision, jump/far shot lifecycle, or
downstream `$92DD+` side-effect parity.

Do not use this asset to claim a complete CPU policy, general pass/shot/steal choice,
ROM actor-link ownership, or live collision/contact ownership. In particular, the
nearby `$B081-$B32E` candidate scan is excluded from ordinary movement
targeting until its callers and outcomes are separately proven. The harness
and scene own explicit native links but do not reconstruct the ROM's live
`$06CB,X` assignment. Live state keeps the ROM command offset absent and
advance false rather than fabricating them. Replacing native targets with the
formation/play command offsets, dynamic links/references, target fields, and
command-advance transitions is the next CPU-policy slice.
See
`docs/gameplay-cpu-steering.md` and run
`tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`.

The slicer does not emulate the ROM streamer's staged PPU
prefetch/write ordering; it returns the canonical camera view. TGFL-1 raw
positions are a scene dependency, while its pure resolver and independent
projection test remain available for focused validation. Run
`tools\Run-GameplayMovementTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
TGMO importer/parser/provenance/dependency/state/harness coverage. Run
`tools\Run-GameplayBallDribbleTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
TGBD importer/parser/provenance/dependency/table/phase/sound coverage. Run
`tools\Run-GameplayFatigueTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
TGFT importer/parser/provenance/dependency/decay/recovery coverage. Run
`tools\Run-GameplayCameraProjectionTests.ps1 -Build -RomPath
<LOCAL_ROM.nes>` for revision, provenance, parser, mutation, dependency, camera,
settle, exact one-step transitions, projection, and independent TGFL composition
coverage.
Run `tools\Run-GameplayCourtTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for the
unchanged TGCT-1 loader golden plus full-world, fine-scroll viewport,
transactional failure, provenance, malformed-pack, dependency, and Rev1 source
coverage.

Period completion follows fixed `$E59B->$E823`: regulation M:00 and divider 45
are prepared before selecting the next banner, halftime, overtime, or final
branch. Only a tied overtime restart at `$E601-$E60F` overwrites that duration
with OT minutes; a completed overtime final retains regulation minutes. The
exact fixed wait is `$E80F-$E81E`; an allowed action must be reported on the
update that reaches zero before `$E7D0-$E822` can enter its unbounded settlement
path. Post-banner team-foul clears are `$E6ED/$E6FF`, the shot-24/divider-50
reset is `$E765-$E76F`, presentation release handling is `$EA14-$EA2F` with
`$D2B9-$D2CE`, and Bank06 `$BC3C-$BCF9` supplies the separate unbounded
halftime/final A-release loop. Detailed evidence limits and shot/foul anchors
are in `docs/gameplay-state-foundation.md`.

Free-throw launch control is evidence-derived. Bank05's human state-20 path
reads the scoring side's current NES B level, so only that team's assigned pad
can launch; the other pad, A, directions, START, SELECT, button edges/releases,
and a pressed-only B bit do nothing. Human attempts never auto-fire. With no
controller assigned to the scoring team, native play uses the bounded slot-3
trace's observed 125-update inclusive state-18-to-launch schedule. Bank05
`$96B6-$9708` selects command offsets `$007D/$00D7`, and Bank06 `$8B8E-$8B9D`
maps them from base `$9F2E` to stream/dispatch pointers; they are command
offsets rather than frame timers. The native scene does not implement that CPU
positioning/script system, so its observed schedule remains a bounded
approximation. Timing resets
per attempt and across scene launch/end.

The exact boundary covers court/CHR/imported palette data and the embedded
FCEUX RGB profile, actor-pose decoding, numeric close-shot steps, the narrowed
TGJS/TGSR miss actor/ball timing and three-point-make actor/result schedule,
the state-`$15` one-to-four-pass prefix and canonical debug handoff, the
source-ordered ordinary human pass gather/flight/catch lifecycle, the bounded
OUT OF BOUNDS/BACKCOURT inbound restart, Q8.8 actor
height, terminal outcomes, one post-miss settlement, state/event timing, foul thresholds,
period/halftime/final transitions, and audio programs/mappings.
Post-handoff live actor layout and fixed five-player roster-slot/matchup-link
binding, complete CPU play selection/pass policy and jump/far shot lifecycle,
exact intra-frame fatigue call placement, jump-ball geometry, jump family-1
selection and unsupported outcomes, ordinary two-point makes, make ball motion, the
longer +157-update claimant route, semantic rebounds/blocks/steals,
general make/contact policy, the trigger selecting
dunk/variant 0 versus layup/variant 2, live close-shot profile/direction
selection and left-facing mirroring, state-dependent palette transitions
outside the exact live-court and cutaway contexts,
foul detection, live free-throw camera/full-court integration and lineup
positioning/aim/outcome/rebound, exact inbound receiver/timing/object state, and
CPU positioning/script behavior, plus the HUD fixed-column and unassigned-CPU
actor-selection adapters remain explicit native approximations. THUD-1's font,
team marks, and Bank02 name formatting are exact within the boundary above.
Local original-frame comparisons found no unrendered or garbage
cells. After normalizing the small emulator RGB-output difference, the TGDK
cutaway pixels match the local frame-24 black, frame-32 stage, frame-48 stage,
and frame-64 black checkpoints exactly. Live court, ball, and player colors use
the exact `$F2E2`, profile-bit, side-bit, `$DEAB`, and `$DC19` matchup path
described above. The
returned live court at frame 80 still differs in actor spacing; the bounded
HUD glyph silhouettes now match the local score/clock/jersey-number reference,
and camera/world projection is strict within the supported horizontal slice.
Test with
`tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`; its private
scratch pack, logs, and PNGs remain under ignored `build\` output. The scene
state suite includes the visible human pass, both-team violation inbound cases,
and the natural post-tip CPU opcode-2 advancement regression. Run
`tools\Run-ShotDirectionProof.ps1 -Build -PackPath <LOCAL_ASSETPACK>` for the
four Away/Home horizontal/diagonal jump-shot selector and mirror checkpoints.
The strict runtime checkpoints are `gameplay-start`,
`gameplay-jump-frameN`, `gameplay-jump-rattle-frameN`,
`gameplay-jump-make-frameN`, and
`gameplay-dunk-frameN`; the focused runner preserves jump-miss hashes, couples
jump-make gather/release/decision/flight/recovery/score/possession frames
through 111, and covers meaningful dunk interior frames and settlement through
132. Run `tools\Run-GameplayDunkCutawayTests.ps1 -Build -RomPath <LOCAL_ROM.nes>`
for the focused TGDK parser, provenance, render, mutation, and revision suite.
The former `gameplay-close-shot-frameN` spelling remains an exact compatibility
alias for `gameplay-dunk-frameN`; it does not introduce a third shot kind.

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

## Roster and Season Cleanup Boundary

Bank02 `$AE4C-$AE9C` writes each roster number at nametable column 6 and starts
the name three columns later at `$2249/$2649`; native roster rows now use x=48
and x=72. Player-detail percentages previously multiplied static rating bytes
and presented them as statistics. That was not a valid ROM statistic source.
Fresh TSAV state now renders ROM-style `.000` percentages and zero totals until
the mutable per-player accumulator is ported.

GAME START now has an explicit two-step boundary. Preparing the next matchup
resolves only the ROM schedule ordinal and teams and sets a pending result; it
does not change TSAV-1. `tecmo_season_commit_game_result` validates the pending
ordinal, teams, non-tied completed score, record limits, and save path before
atomically committing one result. The native gameplay scene launches at the
mapped `$8599->$B27F` boundary, returns the same ordinal/teams plus final score,
and remains active if the commit fails. A successful commit ends it and returns
to the season result rows without reinitializing the session.

League Leaders supports the seven-category ROM navigation table at
`$AD3D-$AD58`. The earlier renderer incorrectly placed Bank01 `$8031` over the
category text and treated blank templates as results. That cursor has been
removed; the selection uses an imported-ROM-font boundary marker, while A
displays `PLAYER RESULTS UNAVAILABLE`. Ranked rows remain unsupported because
TSAV-1 contains no per-player season accumulators and TSNS does not yet carry
the Bank00 `$AC88/$AC5E`, `$B0CC-$B17F`, and `$B430-$B4AF` result machinery.
