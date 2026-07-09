# Asset Pack Import Workflow

This note defines the local asset-pack workflow for importer-driven intro work.
The pack is ignored build output: it can contain ROM-derived PRG/CHR bytes,
decomp-derived metadata, and emulator-capture rows, but none of those payloads
belong in source control.

## Pipeline

1. Keep private inputs local: `<LOCAL_ROM.nes>`, `<LOCAL_DECOMP_ROOT>`, and
   emulator captures under ignored paths such as `build\`.
2. Distill raw emulator captures before building the pack. For arena work, run:

   ```powershell
   .\tools\Import-IntroArenaCapture.ps1
   ```

   This reads ignored watcher logs such as
   `build\emu_intro_memory_watch.ndjson` and
   `build\emu_intro_arena_irq_watch.ndjson`, then writes the compact ignored
   input `build\intro_arena_capture.ndjson`.
3. Build the integrated pack with an explicit root:

   ```powershell
   .\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
   ```

   With `--root` or `TECMO_DECOMP_ROOT`, `--build-assetpack` writes base iNES
   entries, decomp-derived logical entries, and any present intro captures from
   the local capture root. A plain `--build-assetpack <ROM> <OUT>` is a raw
   ROM pack build and must not silently import stale `build\*.ndjson` files from
   the current directory.
4. Verify the directory before treating runtime output as pack-backed:

   ```powershell
   .\build\tecmo_port.exe --assetpack-list build\tecmo.assetpack
   ```

   The list should contain `system/manifest`, `system/source-map`, PRG/CHR
   entries, decomp logical entries, and capture entries expected for the local
   inputs.
5. Runtime lookup order remains: `TECMO_ASSETPACK`, then
   `<project-root>\build\tecmo.assetpack`, then `build\tecmo.assetpack`.
   Raw file/log fallbacks are temporary migration aids and should be isolated
   when proving a capture-backed path.

## Current Pack Entries

`--build-assetpack` writes these base entries for the reference iNES image:

| Entry | Written by | Runtime consumer |
| --- | --- | --- |
| `system/manifest` | base iNES pack builder | Metadata only. |
| `system/source-map` | base iNES pack builder | Metadata/source audit. Its `logical_entries` array mirrors imported decomp/capture entries in the pack. |
| `prg/bankNN` | base iNES pack builder | Reserved for import reconciliation. |
| `prg/fixed` | base iNES pack builder | Reserved fixed-bank alias. |
| `chr/all` | base iNES pack builder | `tecmo_load_chr_data` through the CHR asset-pack loader. |
| `chr/bankNN` | base iNES pack builder | Reserved bank entries; current renderers read `chr/all`. |

With an explicit root, current integrated imports can add:

| Entry | Runtime loader |
| --- | --- |
| `roster/table.tsv` | roster table loader in `asm_inventory.c`. |
| `title/original-text.txt` | original title text loader. |
| `title/glyph-map.tsv` | title glyph-map loaders. |
| `intro/arena/capture.ndjson` | preferred compact arena capture entry. |
| `intro/arena/emu_intro_memory_watch.ndjson` | arena migration alias for raw watcher-shaped data. |
| `intro/arena/emu_intro_arena_irq_watch.ndjson` | arena IRQ migration alias. |
| `intro/post-arena/emu_intro_memory_watch.ndjson` | post-arena migration alias. |
| `intro/post-arena/capture.ndjson` | preferred post-arena compact entry derived from the memory watcher. |
| `intro/captures/source-map` | capture import metadata. |

Compatibility aliases such as `intro/arena/capture`,
`intro/arena/intro_arena_capture.ndjson`, `intro_arena_capture.ndjson`,
`intro/post-arena/capture`, and `intro/post_arena/capture.ndjson` remain
runtime probes, but new packs should prefer the compact `.ndjson` names above.

The intro composite trace is not pack-backed yet. `tecmo_intro_trace_load` still
looks for ignored local `build\intro_composite_trace.json` files. The proposed
namespace is `intro/composite/`, with `intro/composite/trace.json` as the trace
entry and `intro/composite/source-map` for local source metadata.

## Importer Gates

Use placeholders or environment variables for private paths. Do not commit the
pack, generated captures, reports, or rendered original-asset screenshots.

Complete local retest:

```powershell
.\build.ps1
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
.\build\tecmo_port.exe --assetpack-list build\tecmo.assetpack
.\tools\Run-AssetPackTests.ps1 -Build -DecompRoot <LOCAL_DECOMP_ROOT> -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -DecompRoot <LOCAL_DECOMP_ROOT> -RomPath <LOCAL_ROM.nes>
```

`Run-AssetPackTests.ps1` derives PRG/CHR expectations from the reference iNES
header, verifies expected logical entries, checks `system/source-map`, and uses
`--assetpack-list` as an explicit directory gate. Its CHR render and flow checks
are compatibility smoke tests, not exclusive proof that no fallback was used.

`Run-IntroSequenceTests.ps1` builds a fresh test pack, sets `TECMO_ASSETPACK` to
that pack, temporarily isolates loose capture files and canonical stale packs,
then requires capture-backed modes to report an asset-pack source entry.
Missing-capture failures should therefore point either to absent local capture
inputs during pack build or to runtime source selection regressions.

Focused manual renders can still be useful while debugging:

```powershell
$env:TECMO_ASSETPACK = (Resolve-Path build\tecmo.assetpack).Path
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-license build\intro_license_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-ready-frame35 build\intro_ready_frame35_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode intro-warriors-frame74 build\intro_warriors_frame74_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode play-step8 build\play_step8_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode play-step9 build\play_step9_test.png
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode play-step10 build\play_step10_test.png
```

Before judging a manual render as pack-backed, isolate loose fallbacks or require
the render output to report `intro-capture-source ... assetpack=1` for the
expected entry. A passing PNG alone is not enough.

## Data Boundary

Committed defaults must not include raw ROM bytes, PRG/CHR dumps, copied ASM,
decoded private payload rows, local absolute paths, generated rosters, or
screenshots derived from original assets. Public docs may name commands, chunk
IDs, labels, entry names, schemas, counts, and unresolved gates when they do not
reproduce proprietary payload data.
