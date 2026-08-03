# R4 frontend intro/title native contract hardening

Status: implementation and handoff documentation complete; awaiting Sol's
ordered merge.

## Lineage metadata

- Luna ID: `019fc848-b87f-7e32-8954-51097efa933a`
- Title/model-thinking: `gpt-5.6-luna/max`
- Branch: `codex/r4-frontend-intro-title-native-hardening-luna`
- Worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r4-frontend-intro-title-native-hardening-luna`
- Base and last-good: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- Created at: `2026-08-03T15:40:36.000Z`
- Pin state: not pinned; no pin or unpin operation was performed.
- Revision history and creation faults: this was one delegated Luna lineage
  created at the timestamp above. Initial worktree, branch, base, merge-base,
  and clean-status checks passed. The source revision was committed first as
  `f1e7005115f98d0ef189639a2364e740246b6d8c`; this document is the ordered
  documentation-only follow-up. No creation faults, duplicate lineage,
  fork, merge, rebase, push, reset, force, or cross-lineage edit occurred.

## Scope and non-goals

This lineage hardens the native ROM-derived TATR, TATL, TASG, and arena-stage
boundaries in the owned files only. The serialized TATL-1 and TASG-2 payload
layouts remain unchanged. Runtime remains native C consuming semantic assets;
ROM, ASM, decompilation, captures, frames, and traces were research or test
evidence only.

Owned tracked paths changed by the implementation commit are:

- `include/tecmo_intro_arena.h`
- `include/tecmo_intro_arena_scene.h`
- `include/tecmo_intro_stage.h`
- `src/tecmo_intro_arena.c`
- `src/tecmo_intro_arena_scene.c`
- `src/tecmo_intro_stage.c`
- `src/asset_pack/tecmo_asset_pack_arena.c`
- `src/asset_pack/tecmo_asset_pack_arena.h`
- `src/tecmo_title_screen.c`
- `docs/finish-tasks/R4-frontend-intro-title/native-contract-hardening-luna.md`

Non-goals are shared asset-pack/runtime changes, capture replay,
decompilation-runtime use, a new palette-cycle renderer, changing production
pixels, changing the production 540-frame handoff, or editing
`tools/Run-IntroSequenceTests.ps1`.

## Defect closure

- TATR now validates both the top and bottom sprite CHR halves with the same
  16-byte alignment, exact-size, underflow-safe range contract. A malformed
  top offset is rejected during parse and availability checks; the public
  title load starts from a zeroed destination, so rejected data cannot leave
  stale title state available.
- TATL background palette bytes above `0x3F` are rejected by both the native
  decoder and the owned arena-specific importer.
- TATL and TASG entry loads invalidate their destination before every read or
  decode. Missing, truncated, malformed, palette-invalid, or CHR-invalid
  reloads therefore clear availability, counts, identity, and status instead
  of retaining a previous valid object.
- Decoded TATL and TASG objects persist `chr_byte_count=262144` and the full
  CHR FNV1a64 fingerprint `0x96A64F53B240ABB4`. Runtime availability requires
  those exact metadata values, the exact supplied `chr/all` size, the full
  fingerprint, and safe tile or sprite-pair ranges. The importer enforces the
  exact 256 KiB size and source bounds; it intentionally does not hash its
  synthetic shared self-test CHR fixture. The runtime hash gate still rejects
  a cross-CHR pack before native pixels are drawn.
- The legacy five-rectangle arena scene no longer claims an independent
  192-frame production route. Its phase, camera projection, update limit, and
  self-test derive from `tecmo_intro_arena_transition_state`; it remains an
  attachment/self-test projection of the shared 540-frame stage. Its goal
  anchor is `(165,350)`, matching the production TASG anchor.
- The arena dynamic-palette question was audited and deliberately left
  unchanged. Production uses the existing static TATL palette until exact
  source or ignored emulator evidence maps the relevant PPU writes.

## Schema and function surface

The owned decoded schemas add only these in-memory fields:

- `TecmoArenaTileLayer.chr_byte_count`
- `TecmoArenaTileLayer.chr_fingerprint`
- `TecmoArenaNativeSpriteGroups.chr_byte_count`
- `TecmoArenaNativeSpriteGroups.chr_fingerprint`

The public identity constants are `262144` bytes and
`0x96A64F53B240ABB4` (FNV1a64 of the canonical full `chr/all`). No asset-pack
header, entry, byte offset, or serialized payload size changed.

The main changed gates and loaders are `parse_attract`,
`tecmo_title_asset_chr_available`, `arena_decode_tile_layer`,
`load_arena_tile_layer_entry`, `arena_decode_sprite_groups`,
`load_arena_sprite_groups_entry`, and both arena `*_chr_available` functions.
The arena-specific importer applies exact CHR source bounds and palette
validation. Stage boundary coverage is in `tecmo_intro_stage_self_test`; the
legacy scene projection is in `tecmo_arena_intro_init`,
`tecmo_arena_intro_update`, and `tecmo_arena_intro_scene_self_test`.

## ROM and decomp evidence audit

The reference ROM identity used for research and the ROM-only runner is the
supplied Rev1 image, size `393232`, SHA256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`. No ROM,
ASM, capture, frame, trace, save state, decoded proprietary payload, or
private ROM path is committed.

High-confidence anchors:

- Bank04 `$88E7` supplies the arena wait path, with wait `#$96` (`150`)
  frames. The `$892C` loop advances in two-frame steps, and `$8983` is the
  completion/return boundary. The native production route remains handoff
  frame `540`.
- The public timing formula is `150 + ((0xC4 - 1) * 2) = 540`. Self-tests
  cover both sides of the wait boundary (`149/150`) and the handoff boundary
  (`539/540`) without changing the production state machine.
- The arena screen route begins at Bank04 `$88E8`. Screen `$18` uses the
  `$DC85` descriptor table and index `$18`, producing the descriptor span
  `$DD2D-$DD33`; TATL continues to use its CHR pair/stream and fixed lower
  selector tables `$FD7C` and `$FD80`.
- The fixed D9F6 decoder is `$D9F6`. Arena sprite setup uses seed bytes
  `$8984`, emitter `$8988`, parameters `$89BD`, and the fixed sprite pointer
  table `$A7DB`. These anchors support the existing two TASG groups and their
  `55` jumbotron / `16` goal piece counts.
- `$89DD-$8A2C` is a mixed Bank04 data/control table. The audit found no exact
  source-controlled schedule tying those bytes to frame-indexed writes of PPU
  `$3F00-$3F1F`. `$8988` supports native stream/emitter setup, not a proven
  dynamic palette schedule.

Confidence is high for TATR bounds, TATL palette rejection, transactional
invalidations, exact runtime CHR identity, and the 540-frame stage boundaries.
Confidence is intentionally not claimed for a native arena dynamic-palette
schedule.

## Dynamic-palette decision

Production pixels were intentionally left unchanged. The production arena
draw continues to use the static TATL palette. The palette windows in
`src/tecmo_intro_arena.c` remain diagnostic/migration scaffolding and are not
promoted into the native draw.

The deferred evidence gate is precise. Before adding a schedule, obtain either
an exact source/control-flow proof or ignored emulator evidence that records
the Bank04 loop frame, the responsible `$88E7/$892C/$8988` path, and the
corresponding PPU `$3F00-$3F1F` address/value writes across the capture palette
window. The evidence must map writes to native stage frames, distinguish
background from sprite palette writes, and reproduce the resulting palette
bytes. Legacy capture-normalization constants alone are insufficient.

## Verification summary

All generated assets, screenshots, and reports below are ignored build proof;
none is part of either commit.

- `.\build.ps1` passed; both console and game targets built with `/W4` and no
  new warnings observed.
- `.\build\tecmo_port.exe --arena-scene-test` passed with
  `ARENA INTRO SCENE SELF TEST PASS`.
- `.\build\tecmo_port.exe --assetpack-test` passed, including the fixed
  shared arena importer fixture.
- The full required runner passed with `21` tests, `1` bounded skip, and `0`
  failures. Its sanitized report has `passed=true`,
  `private_paths_included=false`, and `raw_output_persisted=false`.
- Representative arena renders passed at native frames `0, 240, 260, 276,
  280, 292, 300, 308, 539`. Goal visibility was `0/5/10/10/15/15/16/16/16`
  at those checkpoints; the `55`-piece jumbotron was visible at frame `0`.
- Lower-band IRQ geometry passed at frames `308/324/340/348`, with the
  expected scroll/motion values and distinct origin/clip progression. Final
  frame `539` registration passed: the post ends at output `Y429`, the black
  opening starts at `Y430`, and the cream cap starts at `Y432`.
- Manual visual inspection showed the full arena at frame `0`, the lower-band
  goal/opening composition at clean frame `539`, and unchanged NBA title
  artwork at attract frame `621`.
- The unrelated `.\build\tecmo_port.exe --flow-test` attempt made without the
  required native root/assets was not counted as a regression result; it
  stopped during setup with `Failed to initialize runtime from .`.

### Proof refresh recovery note

One intermediate PowerShell batch refresh attempt failed at parse time because
the temporary command had a missing newline before `$n = 0`. The renderer was
never invoked and that failed command produced no proof artifact. The already
successful direct renders and the successful full runner remained authoritative;
the three handoff artifacts were retained and independently re-hashed as
`intro-arena-frame0.png` = `1ED80BB...DCD`,
`visual-arena-clean-frame539.png` = `DCEF43...381`, and
`visual-title-attract-frame621.png` = `97A281...A17`. No regeneration loop or
test result was inferred from the parser failure.

## Reproducible proof manifest

Run from the worktree root. The ROM placeholder is deliberate so a private ROM
path is not committed; use the supplied Rev1 ROM identified above.

### Build and automated runner commands

```powershell
.\build.ps1
.\build\tecmo_port.exe --arena-scene-test
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-IntroSequenceTests.ps1 -RomPath '<LOCAL_ROM.nes>'
```

Expected results:

- `build/tecmo_port.exe` exists after the build.
- The two self-tests exit `0`.
- The runner exits `0` and creates
  `build/intro_sequence/tecmo_intro_sequence_test.assetpack` and
  `build/intro_sequence_test_report.json`.
- The report has `test_count=21`, `skipped_count=1`, and
  `failure_count=0`; all raw output and private paths remain absent.

### Arena render artifacts

The durable runner invocation for the arena set is the full
`Run-IntroSequenceTests.ps1` command above. The direct command family used to
regenerate and hash the same files was:

```powershell
$env:TECMO_ASSETPACK=(Resolve-Path 'build\intro_sequence\tecmo_intro_sequence_test.assetpack').Path
.\build\tecmo_port.exe --root (Get-Location).Path --render-test-mode <MODE> <OUTPUT>
```

Every row below was generated with exit `0` and `PNG=True`; the listed SHA256
is the expected proof hash for the current native build.

| Artifact | Mode | SHA256 |
| --- | --- | --- |
| `build/intro_sequence/intro-arena-frame0.png` | `intro-arena-frame0` | `1ED80BB957D115DEFA41A346596987801D39EE15A5B22405344D27DC760F5DCD` |
| `build/arena_rom_exact/intro-arena-frame240.png` | `intro-arena-frame240` | `2ABE277251B0AFF9DE30341B4500A359552CBDB72241C7ED4560EDE396AEC5A7` |
| `build/arena_rom_exact/intro-arena-frame260.png` | `intro-arena-frame260` | `C82353138E0451348CA1981E94F7FD72AB7C2B1BD6D23A303E817958025525F3` |
| `build/arena_rom_exact/intro-arena-frame276.png` | `intro-arena-frame276` | `7A4C36A54BE33BE2F4E0D8995F7BDF951B0B156F3702A87DC3E2B57D7092DBA3` |
| `build/arena_rom_exact/intro-arena-frame280.png` | `intro-arena-frame280` | `8615BB91A3099098CD994A14EFF881C8805D915442EA9BBBDE5CD7DA929DB3E1` |
| `build/arena_rom_exact/intro-arena-frame292.png` | `intro-arena-frame292` | `A328619B718C2D63F75D683C0B523D63964B3B46A3286DDB77F2414AD8D20C35` |
| `build/arena_rom_exact/intro-arena-frame300.png` | `intro-arena-frame300` | `5884B85D9EEA5660A396A08AE57339E41B878177342B091D15E2787CB2A9E1EB` |
| `build/arena_rom_exact/intro-arena-frame308.png` | `intro-arena-frame308` | `9C0AC7076E764DA8F1D7BFD104B50AFE3C2AFF9B38A925CDDC452F64421E5F20` |
| `build/arena_rom_exact/intro-arena-frame539.png` | `intro-arena-frame539` | `90C79FB704EF0A28B26225E0069AE8E1C27B8ED510087B3E0ADD1948EB3EF876` |
| `build/arena_rom_exact/intro-arena-clean-frame308.png` | `intro-arena-clean-frame308` | `8BD2C496C561438E095FD1C2BB1FA41E9B25FC2B295F4F6C0135F12C95805FEC` |
| `build/arena_rom_exact/intro-arena-clean-frame324.png` | `intro-arena-clean-frame324` | `F7B0215F6AAC5CF1EFF05666B0340963D6C22A77EB267DC33A8391EDC0743CD1` |
| `build/arena_rom_exact/intro-arena-clean-frame340.png` | `intro-arena-clean-frame340` | `FB2C66D36DBC6544B60F3626EBBD6CD7838B61237CDE093E11596BC4E6E791B9` |
| `build/arena_rom_exact/intro-arena-clean-frame348.png` | `intro-arena-clean-frame348` | `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381` |
| `build/arena_rom_exact/intro-arena-clean-frame539.png` | `intro-arena-clean-frame539` | `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381` |

### Title and visual inspection artifacts

The full runner creates these title PNGs under `build/intro_sequence` with
exit `0` and `output_created=true`, then removes them: `title-attract-frame6`,
`title-attract-frame621`, `title-screen`, `title-confirm-frame1`,
`title-confirm-frame10`, `title-confirm-frame30`, `title-confirm-frame60`,
`title-confirm-frame120`, and `title-confirm-frame126`. Their exact transient
filenames are `build/intro_sequence/<mode>.png` for each listed mode.

The durable visual inspection command was:

```powershell
$env:TECMO_ASSETPACK=(Resolve-Path 'build\intro_sequence\tecmo_intro_sequence_test.assetpack').Path
.\build\tecmo_port.exe --root (Get-Location).Path --render-test-mode title-attract-frame6 build\intro_sequence\visual-title-attract-frame6.png
.\build\tecmo_port.exe --root (Get-Location).Path --render-test-mode title-attract-frame621 build\intro_sequence\visual-title-attract-frame621.png
.\build\tecmo_port.exe --root (Get-Location).Path --render-test-mode intro-arena-clean-frame539 build\intro_sequence\visual-arena-clean-frame539.png
```

Each command exited `0` and created a PNG. Expected hashes are:

- `build/intro_sequence/visual-title-attract-frame6.png`:
  `8E9FE1698698E0A81F09FACE1F442321CE8512CDC830A07BB053267D33D94D58`
- `build/intro_sequence/visual-title-attract-frame621.png`:
  `97A2811A207DAB9641C2134822D78FB60BC7BE34DEE9210FCA11DBC531DCCA17`
- `build/intro_sequence/visual-arena-clean-frame539.png`:
  `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381`

### Malformed and transaction probes

The ignored durable probe harness is `build/arena-contract-probes.ps1`; it
was invoked exactly as follows:

```powershell
.\build\arena-contract-probes.ps1
```

For every case, the harness copied
`build/intro_sequence/tecmo_intro_sequence_test.assetpack`, changed only the
named field, ran a fresh CLI process, recorded `exit_code` and `png_created`,
then removed the temporary pack and PNG. Expected and observed results were:

| Case | Temporary pack | Temporary PNG | Exit | PNG created | Runtime observation |
| --- | --- | --- | --- | --- | --- |
| `tatr-top-range` | `build/intro_sequence/probe-tatr-top-range.assetpack` | `build/intro_sequence/probe-tatr-top-range.png` | `1` | `False` | top TATR range rejected |
| `tatl-palette-range` | `build/intro_sequence/probe-tatl-palette-range.assetpack` | `build/intro_sequence/probe-tatl-palette-range.png` | `1` | `False` | `0x40` palette byte rejected; `exact_layer=0` |
| `tasg-top-range` | `build/intro_sequence/probe-tasg-top-range.assetpack` | `build/intro_sequence/probe-tasg-top-range.png` | `1` | `False` | top TASG tile rejected; `sprite_groups=0` |
| `cross-chr` | `build/intro_sequence/probe-cross-chr.assetpack` | `build/intro_sequence/probe-cross-chr.png` | `1` | `False` | full CHR fingerprint mismatch rejected |

The full runner also checks malformed title and clean-arena modes. Its
relevant transient PNG names are
`build/intro_sequence/malformed-title-attract-reserved.png`,
`build/intro_sequence/malformed-title-start-prompt.png`, and
`build/intro_sequence/malformed-clean-arena-mode-0.png` through
`build/intro_sequence/malformed-clean-arena-mode-3.png`; each is expected to
exit `1` and have `png_created=false`. The matching malformed title asset
packs are `malformed-title-attract-reserved.assetpack` and
`malformed-title-start-prompt.assetpack`; the harness removes them after each
case.

The source-level transaction contract is covered by invalidating the output
at the start of every TATL/TASG decode and entry load. The four custom probes
run fresh processes, so this manifest does not claim a same-process valid-then-
invalid reload harness. The code-level guarantee is that after a rejected
reload availability is false and the previous counts, identity, and status
cannot remain observable.

## Remaining gaps and handoff

The dynamic arena palette schedule remains deferred behind the exact evidence
gate above. The possible `$C4` wrap off-by-one remains documented and covered
at frames `539/540`; the accepted 540-frame production handoff was not changed
without exact evidence. The five-rectangle scene geometry remains conceptual
and is not a claim of production pixel fidelity.

Implementation commit: `f1e7005115f98d0ef189639a2364e740246b6d8c`, parent
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`, message
`Harden frontend intro and title native contracts`.

Ordered merge range and instructions: start at the accepted base, apply the
implementation commit first, then apply this documentation-only follow-up
commit. Preserve the order `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` ->
`f1e7005115f98d0ef189639a2364e740246b6d8c` -> docs-only follow-up. Sol should
cherry-pick or merge the two commits in that order without squashing if the
review lineage is to remain visible. No self-merge, rebase, push, reset, or
force operation was performed.
