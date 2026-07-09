# Arena Native Asset-Pack Entries

This note defines the target native asset-pack entries for the arena intro,
READY screen, and WARRIORS handoff. It is a design slice only: it names the
entries and the runtime-facing schemas at a conceptual level. It does not
contain original bytes, decoded proprietary tables, emulator logs, screenshots,
or frame captures.

The goal is to replace capture-shaped intro entries with ROM-derived native
entries that can be loaded by the runtime without decompilation folders, Lua
captures, or loose debug files.

## Entry Namespace

Use `arena/intro/` for the arena-intro scene because these entries describe
ported scene concepts, not emulator events.

| Entry | Purpose |
| --- | --- |
| `arena/intro/background-layer` | Static or keyed tile-layer definitions for the arena background. |
| `arena/intro/palette-cycle` | Named palette stages and timing for background and sprite palette changes. |
| `arena/intro/goal-sprite-group` | One anchored goal object composed from relative sprite pieces. |
| `arena/intro/crowd-sprite-groups` | Additional non-goal sprite groups, if they remain distinct from the background layer. |
| `arena/intro/script` | Native scene script that drives phases, waits, camera movement, palette changes, object visibility, and handoff. |
| `arena/intro/ready-screen` | Native READY screen layer, palette, and timing data after arena handoff. |
| `arena/intro/warriors-transition` | Native WARRIORS transition data, including scroll/camera timing and sprite groups. |

These names intentionally differ from current migration entries such as
`intro/arena/capture.ndjson` and `intro/post-arena/capture.ndjson`. Capture
entries may remain as temporary debug scaffolding, but they should not become
the runtime contract for native arena intro playback.

## Conceptual Schemas

The binary format can be compact later. The important contract is the native
shape of each entry.

### `arena/intro/background-layer`

Represents background graphics as a scene layer:

- Layer size in tiles and scrollable bounds.
- One or more tile pages or tile spans, stored as decoded tile IDs and palette
  indexes meaningful to the port.
- Optional named keyframes when the background changes over intro time.
- CHR/tile-bank references expressed as asset-pack references, not mapper
  register writes.
- Source-map metadata that records ROM provenance without embedding table bytes.

Runtime consumers should treat this as a `TecmoArenaTileLayer`, not as PPU
nametable or attribute write playback.

### `arena/intro/palette-cycle`

Represents timed color behavior:

- Named palette sets for background and sprite use.
- Frame durations or script labels for each palette stage.
- Fade or cycle mode flags when the runtime should interpolate or step through
  stages.
- Palette references normalized to renderer palette indexes.

Runtime consumers should ask for the palette at a native scene frame or script
phase. They should not replay `PPUDATA` palette writes.

### `arena/intro/goal-sprite-group`

Represents the basket/goal as one native object:

- A `goal_anchor` position in scene coordinates.
- Relative pieces for backboard, rim, net, support, and post sprites.
- Shared visibility and palette timing.
- Optional object animation keyed by native frame or script label.
- Collision/alignment metadata only if a later scene needs it.

All goal pieces move from the shared anchor. The importer may derive this from
ROM sprite staging, but runtime alignment should not depend on separate
capture-frame offsets per piece.

### `arena/intro/crowd-sprite-groups`

Represents any remaining sprite groups that are not part of the goal object:

- Group name and anchor.
- Relative sprite pieces.
- Visibility range or script labels.
- Palette reference.
- Optional draw priority relative to the background layer and goal group.

If a sprite group is better modeled as background tiles, the importer should
fold it into `arena/intro/background-layer` instead of preserving an
OAM-shaped distinction.

### `arena/intro/script`

Represents scene control as a native script:

- Phase changes such as setup, arena pan, goal reveal, READY handoff, and
  WARRIORS handoff.
- Waits and frame durations.
- Camera moves in scene coordinates.
- Palette-stage changes by name.
- Sprite-group show/hide or animation commands.
- Handoff command naming the next runtime scene.

The script should use native step types such as `set_phase`, `wait`,
`move_camera`, `set_palette_cycle`, `show_sprite_group`, and `handoff`. It
should not expose CPU addresses, IRQ timing, mapper writes, or capture frame
numbers to runtime scene code.

### `arena/intro/ready-screen`

Represents the READY screen as the post-arena destination:

- Background layer or tile layout for READY.
- Palette stages and fade timing.
- Duration and handoff label back to the main intro script.
- Asset references for text or glyph tiles where needed.

This entry is loaded by native READY rendering. It replaces the current
post-arena capture dependency for READY frames once the importer can derive the
state from the ROM.

### `arena/intro/warriors-transition`

Represents the WARRIORS transition after READY:

- Background layer and scroll/camera timeline.
- Sprite groups used during the transition.
- Palette stages and visibility ranges.
- Handoff target for the next intro or menu phase.

This entry should model the transition as a native scene segment. Mapper
snapshots, scroll-register pairs, and OAM frame diffs remain importer concerns.

## Importer Boundary

The importer may understand NES storage because the ROM is stored that way. It
may read PRG/CHR banks, follow ROM source maps, decode tile and palette tables,
simulate enough original intro logic to derive final scene state, and attach
safe provenance metadata.

Importer output should be named native entries:

- NES tile, palette, sprite, scroll, mapper, and script storage becomes
  tile-layer, palette-cycle, sprite-group, camera, and script data.
- Source maps may identify ROM bank, offset, and logical extractor, but they
  must not include copied bytes, lifted ASM, emulator logs, or local paths.
- If execution or replay is required to derive state, the replay output is an
  importer implementation detail. It should be reduced to native entries before
  normal runtime consumption.

## Runtime Boundary

Runtime scene code should load the entries above and update native C state:

- `TecmoArenaIntroScene` owns the active phase, script cursor, camera, palette
  state, tile layers, and sprite groups.
- `TecmoArenaGoal` owns the basket anchor and relative pieces.
- READY and WARRIORS use native post-arena scene data, not capture-frame
  mapping.
- Missing native entries should produce an explicit missing-asset result or
  diagnostic render, not a fallback search through decomp roots or Lua capture
  files in normal ROM-only paths.

Low-level import and diagnostic tools may still use NES names where appropriate.
High-level runtime APIs should remain shaped around scene and renderer concepts.

## Test Hook

The ROM-only asset-pack smoke test now expects the first native arena entries:
`arena/intro/script` and `arena/intro/goal-sprite-group`. Broader entries such
as background layers, palette cycles, READY, and WARRIORS should extend the same
directory/source-map gate in the change that emits them.

Keep the first gate directory-only before checking rendered frames:

- Build from `<LOCAL_ROM.nes>` with no decomp root or loose capture files.
- Verify the expected `arena/intro/*` IDs are present and non-empty.
- Verify `system/source-map` names those logical entries without embedding raw
  asset payloads.
- Verify forbidden capture entries such as `intro/arena/capture.ndjson` and
  `intro/post-arena/capture.ndjson` are absent from the ROM-only pack.

This keeps the gate small: it proves the importer writes native entries from the
ROM-only input before the runtime is required to render parity frames.
