# ROM-Only Import Next Steps

This checklist tracks the next importer/runtime work needed to move intro and
arena rendering away from decomp-root and loose-capture dependencies. The final
importer input remains one local iNES ROM: `<LOCAL_ROM.nes>`.

Decompilation files, rebuilt local ROMs, emulator logs, and ignored JSON/NDJSON
reports may guide development, but they are reference material only. They must
not become final runtime inputs, asset-pack inputs, or required importer gate
fixtures.

## Checklist

- Add ROM-derived logical extractors for roster/title/menu data so runtime init
  can launch from `TECMO_ASSETPACK` without `--root` or `TECMO_DECOMP_ROOT`.
- Decode the intro sequence state directly from PRG/CHR bytes and write it under
  a pack namespace such as `intro/composite/`, with a source-map entry that names
  ROM offsets and schemas without embedding proprietary payload rows.
- Replace `intro/arena/capture.ndjson` and `intro/post-arena/capture.ndjson`
  loose-capture loading with ROM-derived intro/arena state or importer-owned
  emulator replay output generated from the `.nes` input.
- Update arena intro render paths to consume pack entries first and report a
  clear missing-ROM-derived-state status when those entries are absent.
- Promote `tools\Run-IntroSequenceTests.ps1` from skipped runtime-render gap to
  active ROM-only render gate after intro/arena state is pack-backed.
- Keep `tools\Run-NativeFlowTests.ps1`, `tools\Run-ScreenshotTests.ps1`,
  `tools\Find-*.ps1`, `tools\Import-IntroArenaCapture.ps1`, and emulator Lua
  probes available as analysis helpers until equivalent ROM-only coverage lands.
- When detailed arena investigations are needed, point to ignored/local reports
  such as `build\intro_arena_capture.ndjson`,
  `build\emu_intro_memory_watch.ndjson`,
  `build\emu_intro_arena_irq_watch.ndjson`, and any future sanitized
  `build\intro_arena_rom_extractor_report.json`; do not duplicate their payload
  data in public docs.

## Done Criteria

- `--build-assetpack <LOCAL_ROM.nes>` creates all runtime-required logical intro
  and arena entries without reading decompilation roots or loose build reports.
- `tools\Run-AssetPackTests.ps1 -RomPath <LOCAL_ROM.nes>` and
  `tools\Run-IntroSequenceTests.ps1 -RomPath <LOCAL_ROM.nes>` both pass as
  active ROM-only gates.
- Manual renders can be judged pack-backed with loose fallbacks isolated, and a
  passing screenshot no longer depends on `--root`, `TECMO_DECOMP_ROOT`, or
  ignored capture files.
