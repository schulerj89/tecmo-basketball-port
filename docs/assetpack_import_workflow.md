# Asset Pack Import Workflow

This note defines the local asset-pack workflow for importer-driven intro work.
The pack is ignored build output and the final importer input contract is a
single local iNES ROM: `<LOCAL_ROM.nes>`.

The decompilation project is still useful as development reference while writing
extractors and mapping ROM offsets. It is not a final runtime or import input:
the pack builder and importer test gates must not require decompilation files,
copied ASM, emulator logs, or loose capture files.

## Pipeline

1. Keep the private ROM local. Do not commit the ROM, generated packs, reports,
   rendered original-asset screenshots, or decoded proprietary payload data.
2. Build the pack from the iNES file:

   ```powershell
   .\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
   ```

   `--build-assetpack` writes only data directly derived from the iNES file. It
   must not silently import `build\*.ndjson`, decompilation files, or data found
   through `--root`/`TECMO_DECOMP_ROOT`. Passing a `.nes` file is the final
   importer contract.
3. Verify the directory before treating runtime output as pack-backed:

   ```powershell
   .\build\tecmo_port.exe --assetpack-list build\tecmo.assetpack
   ```

   The list should contain `system/manifest`, `system/source-map`, raw PRG/CHR
   entries, and the current native `arena/intro/*` logical entries.
   `system/manifest` records `input_contract=ines-only`; the source map records
   `"input_contract":"ines-only"` and names the logical entries without
   embedding asset payload data.
4. Runtime lookup order remains: `TECMO_ASSETPACK`, then
   `<project-root>\build\tecmo.assetpack`, then `build\tecmo.assetpack`.
   When native arena entries are present, old arena capture inputs are disabled
   unless `TECMO_ALLOW_LOOSE_INTRO_CAPTURE=1` is set for a local diagnostic run.

## Current Pack Entries

`--build-assetpack` currently writes these entries for the reference iNES image:

| Entry | Written by | Runtime consumer |
| --- | --- | --- |
| `system/manifest` | iNES pack builder | Metadata for the iNES-only input contract. |
| `system/source-map` | iNES pack builder | Metadata/source audit for raw iNES offsets and sanitized native arena logical entries. |
| `prg/bankNN` | iNES pack builder | Reserved for import reconciliation. |
| `prg/fixed` | iNES pack builder | Reserved fixed-bank alias. |
| `chr/all` | iNES pack builder | `tecmo_load_chr_data` through the CHR asset-pack loader. |
| `chr/bankNN` | iNES pack builder | Reserved bank entries; current renderers read `chr/all`. |
| `arena/intro/script` | iNES pack builder | Native arena intro scene script contract, with source-map provenance. |
| `arena/intro/background-layer` | iNES pack builder | Native arena background/CHR/MMC3 band contract; dynamic tile state still requires extractor population. |
| `arena/intro/palette-cycle` | iNES pack builder | Native palette-cycle contract sourced from the Bank04 setup route; runtime palette stages still require extractor population. |
| `arena/intro/goal-sprite-group` | iNES pack builder | Native anchored goal object contract, with source-map provenance. |

These entries are intentionally no longer imported by `--build-assetpack`:

| Former entry | Required future work |
| --- | --- |
| `roster/table.tsv` | Extract roster records directly from PRG bytes at known ROM offsets. |
| `title/original-text.txt` | Extract title/menu text directly from PRG bytes. |
| `title/glyph-map.tsv` | Extract glyph mappings directly from PRG/CHR bytes. |
| `intro/arena/capture.ndjson` | Replace emulator-log capture import with a ROM-derived intro-state extractor or emulator replay owned by the importer. |
| `intro/post-arena/capture.ndjson` | Same as above for the post-arena READY/WARRIORS frames. |
| `intro/captures/source-map` | Reintroduce only after capture data is generated from the ROM-only extractor. |

The intro composite trace is not pack-backed yet. `tecmo_intro_trace_load` still
looks for ignored local `build\intro_composite_trace.json` files. The proposed
future namespace is `intro/composite/`, with `intro/composite/trace.json` as the
trace entry and `intro/composite/source-map` for source metadata once that data
is generated from the ROM-only importer.

For the concise migration checklist, see
`docs/rom_only_import_next_steps.md`.

## Importer Gates

Use placeholders or environment variables for private paths. The importer gates
take the ROM path directly:

```powershell
.\build.ps1
.\build\tecmo_port.exe --build-assetpack <LOCAL_ROM.nes> build\tecmo.assetpack
.\build\tecmo_port.exe --assetpack-list build\tecmo.assetpack
.\tools\Run-AssetPackTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
```

`Run-AssetPackTests.ps1` derives PRG/CHR expectations from the iNES header,
verifies raw entry completeness, checks that known logical/capture entries are
absent, checks `system/source-map`, and uses `--assetpack-list` as an explicit
directory gate. It validates `chr/all` directly from the pack directory because
the full runtime still needs ROM-derived roster/title data before it can launch
without a decomp root.

`Run-IntroSequenceTests.ps1` builds a fresh ROM-only test pack, temporarily
isolates loose capture files and canonical stale packs, and records the runtime
render path as a skipped ROM-only gap. Capture-dependent intro frames cannot be
judged from a `.nes` alone until the importer derives the needed runtime state
from the ROM itself.

These are the current public gate categories:

| Script or command | Status | Input contract |
| --- | --- | --- |
| `--build-assetpack <LOCAL_ROM.nes>` | ROM-only importer gate | `.nes` only. Must not read decompilation roots, emulator logs, loose captures, or generated reports. |
| `--assetpack-list build\tecmo.assetpack` | ROM-only directory gate | Generated pack only; used to confirm the importer did not include stale logical/capture entries. |
| `tools\Run-AssetPackTests.ps1 -RomPath <LOCAL_ROM.nes>` | ROM-only automated gate | `.nes` only, with `TECMO_ROM_PATH` allowed as a private local convenience. |
| `tools\Run-IntroSequenceTests.ps1 -RomPath <LOCAL_ROM.nes>` | ROM-only migration gate | `.nes` only for pack checks; runtime render is intentionally recorded as a skipped gap until intro state is ROM-derived. |
| `tools\Run-NativeFlowTests.ps1` and `tools\Run-ScreenshotTests.ps1` | Older runtime/screenshot probes | Still require `-DecompRoot` or `TECMO_DECOMP_ROOT`; useful for analysis, not evidence that importer/runtime flow is ROM-only. |
| `tools\Find-*.ps1`, `tools\Import-IntroArenaCapture.ps1`, and emulator Lua probes | Development reference helpers | May consume decompilation files, rebuilt local ROMs, emulator logs, or ignored reports. Outputs stay local and must not become final importer inputs. |

Focused manual renders can still be useful while debugging:

```powershell
$env:TECMO_ASSETPACK = (Resolve-Path build\tecmo.assetpack).Path
.\build\tecmo_port.exe --render-test-mode intro-license build\intro_license_test.png
.\build\tecmo_port.exe --render-test-mode intro-arena-frame320 build\intro_arena_frame320_test.png
.\build\tecmo_port.exe --render-test-mode intro-ready-frame35 build\intro_ready_frame35_test.png
.\build\tecmo_port.exe --render-test-mode intro-warriors-frame74 build\intro_warriors_frame74_test.png
.\build\tecmo_port.exe --render-test-mode play-step8 build\play_step8_test.png
.\build\tecmo_port.exe --render-test-mode play-step9 build\play_step9_test.png
.\build\tecmo_port.exe --render-test-mode play-step10 build\play_step10_test.png
```

Before judging a manual render as ROM-only, isolate loose fallbacks or require
the render output to show the expected missing-capture state for capture-dependent
frames. A passing PNG alone is not enough.

## Data Boundary

Committed defaults must not include raw ROM bytes, PRG/CHR dumps, copied ASM,
decoded private payload rows, local absolute paths, generated rosters, or
screenshots derived from original assets. Public docs may name commands, chunk
IDs, labels, entry names, schemas, counts, and unresolved gates when they do not
reproduce proprietary payload data.
