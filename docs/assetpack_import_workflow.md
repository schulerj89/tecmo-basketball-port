# Asset Pack Import Workflow

This note defines the intended cleanup path for importer-driven intro work. The
goal is to move local ROM, decomp, and emulator-capture evidence into an ignored
`.assetpack`, then have runtime code consume named pack entries instead of raw
decomp files or watcher logs.

## Pipeline

1. Keep inputs local: `<LOCAL_ROM.nes>`, `<LOCAL_DECOMP_ROOT>`, and any emulator
   captures under ignored paths such as `build\`.
2. Distill raw emulator captures before building the pack. For arena work, run:

   ```powershell
   .\tools\Import-IntroArenaCapture.ps1
   ```

   This reads ignored raw watcher logs such as
   `build\emu_intro_memory_watch.ndjson` and
   `build\emu_intro_arena_irq_watch.ndjson`, then writes the compact ignored
   input `build\intro_arena_capture.ndjson`.
3. Build the pack from a locally owned iNES image:

   ```powershell
   .\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
   ```

   The integrated target for `--build-assetpack` is one pass that starts from
   `<LOCAL_ROM.nes>` plus the decomp root/capture inputs and writes both the
   base iNES entries and any present compact import entries into
   `build\tecmo.assetpack`. In that state, use:

   ```powershell
   .\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
   ```

   Expected integrated imports are `intro/arena/capture.ndjson` from the compact
   arena capture, raw watcher-shaped migration entries only while still needed,
   and `intro/captures/source-map` metadata that records source labels without
   raw payload bytes.
4. Current manual gap: in this worktree, the CLI path still builds the base iNES
   pack only. Until `--build-assetpack` calls the import layer, adding
   `build\intro_arena_capture.ndjson` to the filesystem does not prove it entered
   `build\tecmo.assetpack`; add capture entries with the builder/import command
   under development and verify the pack directory before treating a render as
   asset-pack-backed.
5. Extend the pack in the import step with `tecmo_asset_pack_builder_add_memory`
   or `tecmo_asset_pack_builder_add_file`. Use stable logical names, keep
   source-map metadata current, and record only source offsets/counts or schema
   metadata in committed docs.
6. Runtime lookup order should remain: `TECMO_ASSETPACK`, then
   `<project-root>\build\tecmo.assetpack`, then `build\tecmo.assetpack`.
   Raw file/log fallbacks are temporary migration aids.
7. Render and test from the runtime pack entries. If a render still needs
   `build\*.ndjson` raw watcher logs, that data has not finished the import
   cleanup.

## Current Pack Entries

`--build-assetpack` currently writes these entries:

| Entry | Written by | Runtime consumer |
| --- | --- | --- |
| `system/manifest` | base iNES pack builder | Metadata only; no required runtime consumer today. |
| `system/source-map` | base iNES pack builder | Metadata/source audit only; no required runtime consumer today. |
| `prg/bankNN` | base iNES pack builder | Reserved for import reconciliation; no direct runtime loader today. |
| `prg/fixed` | base iNES pack builder | Reserved fixed-bank alias; no direct runtime loader today. |
| `chr/all` | base iNES pack builder | `tecmo_load_chr_data` via the CHR asset-pack loader in `asm_inventory.c`. |
| `chr/bankNN` | base iNES pack builder | Reserved bank entries; current renderers read `chr/all`. |

Existing runtime loaders also probe these logical entries when a richer local
pack is present:

| Entry | Runtime loader |
| --- | --- |
| `roster/table.tsv` | roster table loader in `asm_inventory.c`. |
| `title/original-text.txt` | `tecmo_load_original_title_text` through the title text pack loader. |
| `title/glyph-map.tsv` | `tecmo_load_original_title_glyphs` and `tecmo_load_title_glyphs_for_text`. |
| `intro/arena/capture.ndjson` | `tecmo_intro_arena_capture_load`; preferred compact arena capture entry. |
| `intro/arena/capture` | arena capture compatibility alias. |
| `intro/arena/intro_arena_capture.ndjson` | arena capture compatibility alias. |
| `intro_arena_capture.ndjson` | arena capture compatibility alias. |
| `intro/arena/emu_intro_memory_watch.ndjson` | arena migration alias for raw watcher-shaped data. |
| `emu_intro_memory_watch.ndjson` | arena migration alias for raw watcher-shaped data. |
| `intro/arena/emu_intro_arena_irq_watch.ndjson` | arena migration alias for raw watcher-shaped data. |
| `emu_intro_arena_irq_watch.ndjson` | arena migration alias for raw watcher-shaped data. |
| `intro/post-arena/capture.ndjson` | `tecmo_intro_post_arena_capture_load`; preferred post-arena capture entry. |
| `intro/post-arena/capture` | post-arena compatibility alias. |
| `intro/post_arena/capture.ndjson` | post-arena compatibility alias. |
| `intro/post-arena/emu_intro_memory_watch.ndjson` | post-arena migration alias for raw watcher-shaped data. |

The intro composite trace is not pack-backed yet. `tecmo_intro_trace_load` still
looks for ignored local `build\intro_composite_trace.json` files. The proposed
canonical namespace is `intro/composite/`, with
`intro/composite/trace.json` as the trace entry and
`intro/composite/source-map` for local source metadata. Add those entries before
runtime depends on the composite trace for the finished intro path.

## Intro Retest Checklist

Use placeholders or environment variables for private paths. Do not commit the
pack, generated captures, reports, or rendered original-asset screenshots.

```powershell
.\build.ps1
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
.\build\tecmo_port.exe --bank07-test
.\build\tecmo_port.exe --controls-test
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
```

Source isolation before focused intro retests:

```powershell
$CandidateAssetPack = "build\tecmo.assetpack"
Remove-Item Env:\TECMO_INTRO_CAPTURE -ErrorAction SilentlyContinue
$env:TECMO_ASSETPACK = (Resolve-Path $CandidateAssetPack).Path
```

Temporarily move or rename local capture fallbacks before judging an import
cleanup pass, then restore them after the run. At minimum isolate:
`build\intro_arena_capture.ndjson`, `intro_arena_capture.ndjson`,
`build\emu_intro_memory_watch.ndjson`, `emu_intro_memory_watch.ndjson`,
`build\emu_intro_arena_irq_watch.ndjson`, and
`emu_intro_arena_irq_watch.ndjson`. If local fallbacks cannot be moved, require
source proof instead: the candidate pack directory must contain the intended
entry, `intro/captures/source-map` must identify its source label, and the render
status/footer must name an `assetpack entry` loaded from the candidate
`TECMO_ASSETPACK` path rather than a local file path.

Do not count `intro-composite-preset` as pack-backed while it still depends on
`build\intro_composite_trace.json`; it is a local-trace render until
`intro/composite/trace.json` exists and the runtime status proves that entry won.

Focused intro renders:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-composite-preset build\intro_composite_preset_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-license build\intro_license_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-transition build\intro_arena_transition_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-frame120 build\intro_arena_frame120_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-ready-frame35 build\intro_ready_frame35_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-warriors-frame74 build\intro_warriors_frame74_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode play build\play_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode play-step7 build\play_step7_test.png
```

Full declared checks:

```powershell
.\tools\Run-NativeFlowTests.ps1 -Build
.\tools\Run-ScreenshotTests.ps1 -Build
```

When validating importer cleanup specifically, point `TECMO_ASSETPACK` at the
candidate pack and verify the focused intro renders work without relying on raw
watcher logs in `build\`. A passing screenshot alone is not enough if raw/local
fallbacks were still reachable.

## Next Milestones

1. Add one import command or script that starts from `<LOCAL_ROM.nes>`,
   `<LOCAL_DECOMP_ROOT>`, and ignored captures, then writes all compact logical
   entries into `build\tecmo.assetpack`.
2. Import `build\intro_composite_trace.json` into
   `intro/composite/trace.json` and update `tecmo_intro_trace_load` to prefer
   that pack entry.
3. Make `intro/arena/capture.ndjson` and `intro/post-arena/capture.ndjson` the
   only required runtime capture shapes. Keep raw watcher aliases only until the
   compact import path covers the existing render tests.
4. Remove runtime parsing of raw emulator watcher logs after the compact entries
   cover arena, READY, and Warriors transition renders.
5. Finish the intro sequence by modeling the fade-out and handoff after the
   first TECMO PRESENTS layer, then connect the native sequence through license,
   arena, READY, Warriors, and the next menu/selection state.
6. Gate each milestone with the focused renders above plus the native flow and
   screenshot test runners.

## Data Boundary

Committed defaults must not include raw ROM bytes, PRG/CHR dumps, copied ASM,
decoded private payload rows, local absolute paths, generated rosters, or
screenshots derived from original assets. Public docs may name commands, chunk
IDs, labels, entry names, schemas, counts, and unresolved gates when they do not
reproduce proprietary payload data.
