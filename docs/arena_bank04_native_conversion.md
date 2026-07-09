# Arena Bank 04 Native Conversion Map

This note maps the Bank 04 arena intro route in `C-0127..C-0130` to native C
scene concepts. The lifted files are read-only evidence. The conversion target
is a native arena intro script/state machine, not copied assembly, emulator
replay, or committed table payloads.

Source policy:

- Keep labels, chunk IDs, scratch variable names, and high-level behavior.
- Do not copy ASM instructions, original bytes, decoded sprite/table payloads,
  emulator logs, screenshots, or CHR/palette data.
- Runtime code should express these behaviors as `TecmoArenaIntroScene`,
  `TecmoArenaIntroPhase`, `TecmoArenaCamera`, `TecmoArenaPaletteCycle`,
  `TecmoArenaSpriteGroup`, and `TecmoArenaIntroScript` concepts.

## Route Summary

`L88E7` is the arena intro lead in this slice. It prepares the screen setup,
loads a palette/setup snapshot through a fixed helper, initializes scroll and
sprite stream state, emits two sprite groups, then runs a timed motion loop
until the intro handoff timer wraps. The loop updates camera/motion counters and
re-emits the two sprite streams every step.

`L88A9` is a palette fade helper. It derives a four-step fade amount from the
caller's stage value, applies that stage to the active palette work area, marks
palette state dirty, and asks the fixed frame service to apply the update.

`L88D4` is a timer/service initializer used by `L88E7`. It clears the current
step state, runs a fixed service delay, then seeds the next intro phase and its
timer.

`L8988` is the dual sprite-stream emitter. It clears the sprite staging count,
emits the two configured streams in priority order through the fixed stream
helper, then runs the fixed cleanup/hide-unused-sprites helper.

`C-0128` and `C-0130` are tiny local seed chunks. They should become named
metadata entries for this intro route, not hard-coded runtime magic arrays.

## Scratch-State Names

Use these names when porting the route into C. The original scratch symbols are
kept here only to make source-map review unambiguous.

| Original scratch | Native name | Role |
| --- | --- | --- |
| `$03` | `phase_timer` | Timer seeded by `L88D4` for the next intro phase. |
| `$04` | `phase_state` | Small state selector used around fixed frame service calls. |
| `$20`, `$21` | `sprite_stream_pos_hi[2]` | High/extended position component for the two stream emits. |
| `$30` | `fade_step` | Derived palette fade amount for the current fade stage. |
| `$57`, `$58` | `scroll_seed` / `stream_seed` | Route-local seeds set before the first dual-stream emit. |
| `$88` | `camera_motion_counter` | Countdown that gates the active camera/sprite motion window. |
| `$8A` | `handoff_timer` | Loop timer that advances until it wraps and exits the route. |
| `$0301` | `motion_phase_counter` | Incrementing phase counter that selects motion windows. |
| `$030E..$031D` | `active_palette_work` | Destination palette work area after fade application. |
| `$032E..$033D` | `base_palette_work` | Source palette/setup work area used by the fade helper. |
| `$034F` | `palette_dirty` | Marks palette work as ready for the fixed frame service. |
| `$0352` | `intro_update_mode` | Route update mode selected before the arena loop. |
| `$058D` | `sprite_stage_count` | Count/state reset before stream-to-sprite staging. |
| `$07EB`, `$07EC` | `sprite_stream_pos_lo[2]` | Low position component for the two stream emits. |

## C-0127 Line-by-Line Map

The rows below cover every executable line in the lifted chunk, grouped only
where consecutive lines form one native operation.

| Reference lines | Label / branch | Native concept | Conversion notes |
| --- | --- | --- | --- |
| 7-14 | `L88A9` entry | Fade stage decode | Treat the caller input as a bounded palette fade stage. Convert it to `fade_step`; do not expose CPU arithmetic in scene code. |
| 15-25 | `L88B3` loop and `L88BD` branch | Palette fade application | Iterate the non-universal palette entries, subtract the stage amount from each base entry, and clamp underflow to the route's blank/dark palette entry. Store into `active_palette_work`. |
| 26-32 | `L88A9` tail | Palette commit | Set `palette_dirty`, request one fixed frame-service update, then return. Native C should be `tecmo_arena_palette_cycle_apply_stage(scene, stage)`. |
| 34-44 | `L88D4` | Phase timer initializer | Clear `phase_state` and `phase_timer`, run a short fixed service delay, then seed the next phase timer/state. Native C should represent this as a script `WAIT` followed by `SET_PHASE`. |
| 46-52 | `L88E7` setup prelude | Arena screen/setup load | Select the arena intro screen, load the adjacent setup snapshot through the fixed copy helper, then run the timer initializer. The runtime concept is `TECMO_ARENA_INTRO_PHASE_SETUP`. |
| 53-61 | `L88E7` state seeds | Intro phase and camera initialization | Seed the active motion counter, handoff timer, update mode, and global intro step, then request the shared update/service helper. Keep these as named script fields rather than raw literals. |
| 62-69 | `L890E` loop | Dual stream seed copy | Copy two local seed pairs from `C-0128` into `sprite_stream_pos_lo[]` and `sprite_stream_pos_hi[]`. In native C this is route metadata loaded by the importer. |
| 70-74 | First emit call | Initial sprite group emit | Set route-local scroll/stream seeds and emit both sprite groups once before the long wait. |
| 75-76 | Fixed wait call | Presentation hold | Wait long enough for the first arena composition to be visible before the motion loop begins. Model as a script `WAIT`, not a CPU helper call in runtime scene code. |
| 77-83 | `L892C` loop head | Per-step frame advance and motion gate | Advance by a short fixed delay, increment `handoff_timer`, and skip motion updates when `camera_motion_counter` has left the active window. |
| 84-94 | `L892C` first motion block | Primary sprite/camera drift | While `motion_phase_counter` is below its cap, increment it and move stream 0 toward lower fixed-point coordinate values. Borrow updates the high component. |
| 95-105 | `L8950` window | Secondary stream easing window | During the middle motion window, add a small counter-motion to stream 1 before the common drift is applied. This creates a slower relative movement for the second group. |
| 106-115 | `L8968` and `L8975` | Common drift and camera countdown | Apply the common stream 1 drift, including high-component borrow, then reduce `camera_motion_counter` by a fixed step. Native C should express this as `MOVE_CAMERA` plus per-group anchor offsets. |
| 116-120 | `L8979` loop tail | Re-emit and handoff test | Re-emit both sprite groups after motion updates. Continue until `handoff_timer` wraps; wrapping is the handoff condition. |
| 121-122 | `L8983` | Handoff return | Return to the caller once the route timer expires. Native C should emit `TECMO_INTRO_STEP_HANDOFF`. |

## C-0128 Seed Chunk Map

| Reference line | Chunk | Native concept | Conversion notes |
| --- | --- | --- | --- |
| 7 | `C-0128` | Initial dual-stream positions | The chunk contains two seed pairs consumed by `L890E`. Convert them into named importer metadata such as `arena/intro/script.stream_initial_position[2]`. Do not document or commit the byte payload. |

## C-0129 Line-by-Line Map

The rows below cover every executable line in the lifted chunk, grouped only
where consecutive lines form one native operation.

| Reference lines | Label / branch | Native concept | Conversion notes |
| --- | --- | --- | --- |
| 7-10 | `L8988` entry | Sprite staging reset | Clear the current sprite staging count and start a two-pass emit. Native C should initialize a fresh `TecmoArenaSpriteGroupFrame`. |
| 11-13 | `L898F` loop prologue | Emit pass preservation | Preserve the pass index while preparing fixed-helper parameters. In C this is just a loop over two stream descriptors. |
| 14-21 | Stream parameter load | Per-group descriptor assembly | Combine the per-pass selector bytes from `C-0130` with the live position pair from `sprite_stream_pos_lo[]` and `sprite_stream_pos_hi[]`. The result is a `TecmoArenaSpriteGroupEmit` descriptor. |
| 22-29 | Fixed stream helper call | Sprite group emit | Set the remaining helper flags and emit one stream through the native equivalent of the fixed stream-to-sprite staging helper. The stream table itself remains ROM-derived asset metadata, not copied into this doc. |
| 30-33 | Loop tail | Second pass | Restore the pass index and emit the other stream. Preserve the pass order because it is also draw/priority order. |
| 34 | `L8988` tail | Sprite cleanup | Run the native equivalent of the fixed unused-sprite cleanup helper after both streams have been staged. |

## C-0130 Parameter Chunk Map

| Reference line | Chunk | Native concept | Conversion notes |
| --- | --- | --- | --- |
| 7 | `C-0130` | Per-pass stream parameters | The chunk provides two small parameter arrays consumed by `L8988`. Convert them into named descriptor fields such as `base_x_selector` and `group_selector`; do not document or commit the byte payload. |

## Native Script Shape

The route should compile to a small native script similar to:

```c
static const TecmoArenaIntroStep arena_bank04_intro_steps[] = {
    { TECMO_INTRO_STEP_SET_PHASE, TECMO_ARENA_INTRO_PHASE_SETUP },
    { TECMO_INTRO_STEP_LOAD_SETUP_SNAPSHOT, TECMO_ARENA_SETUP_BANK04 },
    { TECMO_INTRO_STEP_WAIT, TECMO_ARENA_WAIT_SETUP },
    { TECMO_INTRO_STEP_SET_CAMERA, TECMO_ARENA_CAMERA_INTRO_START },
    { TECMO_INTRO_STEP_SHOW_SPRITE_GROUP, TECMO_ARENA_GROUP_INITIAL },
    { TECMO_INTRO_STEP_WAIT, TECMO_ARENA_WAIT_PRESENTATION_HOLD },
    { TECMO_INTRO_STEP_MOVE_CAMERA, TECMO_ARENA_MOTION_ACTIVE_WINDOW },
    { TECMO_INTRO_STEP_SHOW_SPRITE_GROUP, TECMO_ARENA_GROUP_EACH_FRAME },
    { TECMO_INTRO_STEP_HANDOFF, TECMO_ARENA_HANDOFF_POST_INTRO },
};
```

The identifiers above are proposed native names, not original payload values.
The importer can attach source-map evidence to `C-0127..C-0130`, but runtime
code should consume named steps and descriptors.

## Porting Notes

- `L88A9` should become a palette-cycle operation over native palette entries.
  Keep the clamp/blank behavior, but hide palette work RAM details behind
  `TecmoArenaPaletteCycle`.
- `L88D4` should become script timing. The fixed service call belongs in the
  scheduler or fixed-helper model, not in high-level arena scene code.
- `L88E7` should own one arena intro phase with explicit setup, hold, active
  motion, repeated sprite-group emission, and handoff.
- The two stream positions should be fixed-point or split low/high native
  fields until the exact coordinate interpretation is fully proven.
- `L8988` should call a native fixed-helper model that accepts structured
  stream descriptors and appends sprite records to a frame-local sprite group.
- The basket/goal should still be modeled as one anchored object. If one of the
  emitted streams contributes goal pieces, the C port should bind those pieces
  under a shared `goal_anchor` rather than compensating individual sprites with
  frame offsets.
- The script should be validated with native tests for phase order, waits,
  handoff timing, palette stage progression, camera motion, and sprite group
  anchor movement. Do not validate by replaying emulator logs at runtime.
