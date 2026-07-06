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

It writes:

```text
build\original_screen_sources.json
```

That report is intentionally ignored by Git and should contain only paths, labels, address ranges, counts, and implementation notes. It must not include copied ASM, ROM bytes, extracted CHR bytes, generated proprietary data, screenshots from original assets, or absolute private paths.

## Next Native-Port Gates

- Replace fixed-bank helper effects such as frame waits, render writes, and setup wrappers with explicit native C functions.
- Resolve palette initialization for the title/menu path.
- Map title/menu tile IDs to local CHR bank(s) while keeping extracted bytes local.
- Convert the Bank 04 title/pattern/script records into safe native structs or a local-only generated cache.
- Add the first `--render-test-mode original-title` quick launch after those dependencies are mapped.
