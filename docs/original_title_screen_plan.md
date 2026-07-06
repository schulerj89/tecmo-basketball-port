# Original Title/Menu Source Map

This note tracks the first original-game screen target without committing ROM bytes, ASM dumps, extracted graphics, generated rosters, or tile sheets.

## Current Target

The first practical target is the original title splash/title-menu path, not full gameplay. The local decomp currently points to Bank 04 as the highest-confidence source for the first-screen presentation path, with Bank 03 likely taking over for follow-on menu/presentation flow.

## Local-Only Source Groups

- `bank04_title_splash_core`: title render loop, title text table, and title helper wrapper.
- `bank04_intro_staging`: intro/title setup, per-frame loop, pattern records, transition tables, and fade/scroll helpers.
- `bank04_large_intro_tables`: coarse structured intro/presentation script tables that need sub-table mapping before native rendering.
- `bank03_menu_presentation_flow`: follow-on menu and presentation loops after the title/intro path.
- `ppu_chr_and_fixed_helpers`: local CHR source plus fixed-bank NMI/IRQ/render helper behavior that must become explicit native C state.

Run the local mapper:

```powershell
.\tools\Find-OriginalScreenSources.ps1
```

Resolve the title text render path through the fixed dispatcher, Bank 06 character mapping, and the 2x2 glyph tile table:

```powershell
.\tools\Find-TitleChrMapping.ps1
```

Render the first source-backed title probe:

```powershell
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --render-test-mode original-title build\original_title_test.png
```

It writes:

```text
build\original_screen_sources.json
build\title_chr_mapping.json
build\title_mapped_chr_probe.png
```

Those reports/probes are intentionally ignored by Git. Public docs may keep chunk IDs, ranges, labels, and conclusions, but must not include copied ASM, ROM bytes, extracted CHR bytes, generated proprietary data, screenshots from original assets, or absolute private paths.

## Current Title CHR Findings

- Bank 04 title text does not map directly to CHR tile IDs.
- The title loop dispatches `A=0x38` through fixed helper `$C711`; the local dispatcher tables resolve that to `06:$9E50`.
- Bank 06 maps characters through the `06:$A290` helper and then reads four tile IDs per glyph from the `06:$AF05` table.
- Bank 04 setup writes `$0352=0x1F` and `$0100=0x06` before entering the `$BA16` setup path; that pattern/VRAM setup still needs a native model before the CHR-backed quick launch can be called pixel-accurate.

## Next Native-Port Gates

- Model the Bank 04 `$BA16` pattern setup path into explicit native pattern-table/VRAM state.
- Replace fixed-bank helper effects such as frame waits, render writes, and setup wrappers with explicit native C functions.
- Resolve palette initialization for the title/menu path.
- Map title/menu tile IDs to local CHR bank(s) while keeping extracted bytes local.
- Convert the Bank 04 pattern/script records into safe native structs or a local-only generated cache.
- Replace the probe font/colors with mapped title/menu CHR, palette, and layout data while keeping generated assets local.
