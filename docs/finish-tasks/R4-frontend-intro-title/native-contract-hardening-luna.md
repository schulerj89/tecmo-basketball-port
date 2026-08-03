# R4 frontend intro/title native contract hardening

Status: implementation complete; review/merge handoff pending.

## Scope

This lineage hardens the native ROM-derived TATR/TATL/TASG boundaries and the
owned arena-stage diagnostic surface. It changes only the owned arena/title
runtime, arena-specific importer, stage/scene contract, and this document.

The serialized TATL-1 and TASG-2 payload layouts remain unchanged. The shared
asset-pack self-tests intentionally use a synthetic CHR fixture, so importer
validation keeps the exact 256 KiB CHR size/source-range gate while the owned
decoded runtime objects persist the canonical full-CHR identity and reject a
supplied `chr/all` whose FNV1a64 differs. This preserves the fixed shared
fixture and still fails closed before native pixels are drawn for a cross-CHR
pack.

Non-goals are shared asset-pack/runtime changes, capture replay, decompilation
runtime use, a new palette-cycle renderer, and changing the production 540
frame handoff.

## Changes

- `src/tecmo_title_screen.c` now validates both TATR sprite CHR halves with
  one aligned, underflow-safe range helper. Parsing and availability both
  reject malformed top or bottom offsets; stale availability cannot survive a
  rejected load.
- TATL runtime decoding rejects every background palette byte above `$3F`.
  The owned arena importer applies the same byte-level gate before copying the
  palette into TATL.
- TATL and TASG entry loads invalidate the destination before every read or
  decode. Missing, malformed, and palette-invalid reloads therefore clear the
  previous available object, counts, identity, and status rather than leaving
  a valid prior object visible.
- Decoded TATL/TASG objects persist `chr_byte_count=262144` and
  `chr_fingerprint=0x96A64F53B240ABB4` (FNV1a64). Runtime availability checks
  require those exact metadata values, the exact supplied size, the full hash,
  and safe per-tile/per-pair ranges. The native title path retains and checks
  its existing same-pack CHR identity contract, now with both TATR halves
  covered.
- `include/tecmo_intro_stage.h` publishes the ROM timing convention:
  `$88E7` wait `#$96`, two-frame `$892C` sampling, `$C4` loop tick, and
  handoff frame 540. `tecmo_intro_stage_self_test` covers frames 149/150 and
  539/540 without changing the production state machine.
- The legacy five-rectangle arena scene remains an attachment/self-test
  concept only. Its phase, camera projection, update cap, and full self-test
  now derive from `tecmo_intro_arena_transition_state`; the old independent
  192-frame linear timeline is gone. Its goal anchor matches the production
  TASG anchor `(165,350)`.

## Schema and function surface

The owned decoded schemas add `chr_byte_count` and `chr_fingerprint` to
`TecmoArenaTileLayer` and `TecmoArenaNativeSpriteGroups`; no TATL/TASG byte
offset, header, or payload size changes. The changed gates are
`parse_attract`, `tecmo_title_asset_chr_available`,
`arena_decode_tile_layer`, `load_arena_tile_layer_entry`,
`arena_decode_sprite_groups`, `load_arena_sprite_groups_entry`, both arena
`*_chr_available` functions, and both arena-specific importer builders.
Stage boundary coverage is in `tecmo_intro_stage_self_test`; the legacy scene
projection is in `tecmo_arena_intro_init`, `tecmo_arena_intro_update`, and
`tecmo_arena_intro_scene_self_test`.

## ROM/decomp evidence audit

Evidence used was the supplied Rev1 ROM (SHA256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`) and the
local decompilation as research only. No ROM, ASM, capture, frame, trace, or
private path is committed.

High-confidence anchors:

- Bank04 `$88E7` is the arena entry/wait path; the first `$892C` iteration is
  after `#$96` frames, and `$892C` advances through a two-frame wait. The
  `$8983` path is the completion/return boundary. The existing native route
  remains frame 540, with the `frame >= next_loop_frame` convention made
  explicit by boundary self-tests.
- The arena screen route begins at Bank04 `$88E8`. The fixed screen
  descriptor for screen `$18` is the `$DC85` table plus screen index `$18`
  (the resulting descriptor span is `$DD2D-$DD33`). TATL still uses the
  descriptor's CHR pair/stream and the fixed lower selector tables `$FD7C` and
  `$FD80`.
- The fixed D9F6 decoder is `$D9F6`. Arena sprite setup uses Bank04 seed bytes
  `$8984`, emitter `$8988`, and parameters `$89BD`; the fixed sprite pointer
  table is `$A7DB`. These anchors support the existing two TASG groups and
  55/16 piece counts.
- `$89DD-$8A2C` is a mixed Bank04 data/control table. The audit found no exact
  source-controlled schedule tying its bytes to frame-indexed writes of
  PPU `$3F00-$3F1F`. `$8988` supplies the native stream/emitter setup, not a
  proven dynamic palette schedule.

Confidence is high for the TATR bounds, TATL palette contract, transaction
semantics, CHR identity, and 540-frame stage boundaries. Confidence is
deliberately not claimed for a native arena dynamic-palette schedule.

## Dynamic-palette decision

Production pixels were intentionally left unchanged. The native production
draw continues to use the static TATL palette. The capture parser's palette
windows in `src/tecmo_intro_arena.c` remain diagnostic/migration scaffolding
and are not promoted into the native draw.

The deferred evidence gate is precise: before adding a schedule, obtain either
an exact source/control-flow proof or ignored emulator evidence that records
the Bank04 loop frame, the responsible `$88E7/$892C/$8988` path, and the
corresponding PPU `$3F00-$3F1F` address/value writes across the capture palette
window. The evidence must map the writes to native stage frames, distinguish
background from sprite palette writes, and reproduce the resulting palette
bytes. Legacy capture-normalization constants alone are insufficient.

## Verification

All generated assets and reports below are ignored build proof; they are not
part of the commit.

- `.\build.ps1` — passed, both console and game targets built.
- `.\build\tecmo_port.exe --arena-scene-test` — passed.
- `.\build\tecmo_port.exe --assetpack-test` — passed, including the fixed
  shared arena importer fixture.
- `.\tools\Run-IntroSequenceTests.ps1 -RomPath <LOCAL_ROM.nes>` — passed:
  21 tests, one bounded skip, zero failures. The sanitized report is
  `build/intro_sequence_test_report.json`.
- Representative arena renders passed at native frames 0, 240, 260, 276,
  280, 292, 300, 308, and 539. Goal visibility remained 0/5/10/10/15/15/16
  at the expected checkpoints; the 55-piece jumbotron was visible at frame 0.
- IRQ lower-band geometry passed at frames 308/324/340/348, including the
  distinct origin/clip progression. Final frame 539 registration passed: the
  post ends at output Y429, the black opening begins at Y430, and the cream cap
  begins at Y432.
- Manual visual inspection showed the expected full arena at frame 0, the
  lower-band goal/opening composition at clean frame 539, and the unchanged
  NBA title attract artwork at frame 621. SHA256 proof hashes were
  `1ED80BB957D115DEFA41A346596987801D39EE15A5B22405344D27DC760F5DCD`
  (arena frame 0),
  `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381`
  (arena clean frame 539), and
  `97A2811A207DAB9641C2134822D78FB60BC7BE34DEE9210FCA11DBC531DCCA17`
  (title frame 621).
- Ignored malformed probes rejected with exit code 1 and created no PNG:
  TATR top offset out of range, TATL palette byte `$40`, TASG top offset zero,
  and a flipped `chr/all` byte. The valid pack was restored after each probe.

## Reproducible proof manifest

From the worktree root, with the supplied ROM available locally:

```powershell
.\build.ps1
.\build\tecmo_port.exe --arena-scene-test
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-IntroSequenceTests.ps1 -RomPath <LOCAL_ROM.nes>
```

The malformed-pack probe used only a temporary copy of the generated
`build/intro_sequence/tecmo_intro_sequence_test.assetpack`; it changed one
payload field per run and removed the copy and PNG afterward. No source
script, ROM, capture, or decoded proprietary payload was retained.

## Remaining gaps and handoff

The dynamic arena palette schedule is deferred behind the evidence gate above.
The possible `$C4` wrap off-by-one is documented and covered at frames 539/540;
the accepted 540-frame production handoff was not changed without exact new
evidence. The five-rectangle scene geometry remains conceptual and is not a
claim of production pixel fidelity.

Commit SHA: pending final commit.

Sol should merge this commit onto the accepted base `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
without squashing or editing shared paths. No merge, rebase, push, reset, or
force operation was performed in this lineage.
