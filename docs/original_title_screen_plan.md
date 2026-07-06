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

Map the exact title setup entry plus adjacent helper calls, fixed-helper aggregate counts, fixed-bank vector counts, palette/PPU probe counts, write targets, stream format/effect summary, and table references:

```powershell
.\tools\Find-TitleSetupMapping.ps1
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
build\title_setup_mapping.json
```

Those reports/probes are intentionally ignored by Git. Public docs may keep chunk IDs, ranges, labels, and conclusions, but must not include copied ASM, ROM bytes, extracted CHR bytes, generated proprietary data, screenshots from original assets, or absolute private paths.

## Current Title CHR Findings

- Bank 04 title text does not map directly to CHR tile IDs.
- The title loop dispatches `A=0x38` through fixed helper `$C711`; the local dispatcher tables resolve that to `06:$9E50`.
- Bank 06 maps characters through the `06:$A290` helper, using the baseline `$A273,X` lookup operand for character IDs, and then reads four tile IDs per glyph from the `06:$AF05` table.
- Bank 04 setup writes `$0352=0x1F` and `$0100=0x06` before entering `$BA16`; that exact entry sets the `$05B6` update flag bit.
- The adjacent Bank 04 setup driver, pointer seed helper, stream copy helper, fixed-helper calls, write targets, and table reference counts are mapped in a local-only report.
- The local-only setup report now summarizes the BAA4 stream format/effect model, verifies all 15 stream table entries, verifies all five dynamic selector rows terminate, and records aggregate source/output counts without including payload bytes.
- The BAA4 helper model is one count byte, two private base-offset bytes, and four private source fields per record; each record emits four staged bytes. The largest local stream currently consumes 139 private source bytes and emits 136 staged bytes.
- The BAA4 helper range is now tracked as `04:$BAA4-$BAF0`, which includes the stream finalize helper call at the end of the helper.
- Fixed helper calls are summarized as five known helper targets, six invocations, two frame-wait calls with aggregate wait request 20, two staging seed calls, one setup finalize call, and one stream finalize call.
- The five fixed helper targets used by the title setup path are now verified as five fixed-bank jump entries with five resolved targets spanning `0xC419-0xF2F6`; helper body decoding is still pending.
- The title setup palette/PPU probe checks three Bank 04 title setup ranges and finds zero direct PPU address writes, zero direct PPU data writes, and zero direct palette-address high-byte literals, so title palette state must be decoded through the fixed-helper/queued PPU path.
- The native `original-title-chr` path now loads a setup staging summary from local Bank 04 bytes and renders driver call/write counts, fixed-helper aggregate counts, fixed-bank vector counts, palette/PPU probe counts, stream-copy write counts, verified table-reference counts, stream-table coverage, stream effect shape, and aggregate native staging coverage without committing setup streams.
- The native runtime now boots into the current CHR-backed title diagnostic; Enter opens the launcher and Esc quits.
- The native launcher also exposes the current CHR-backed title diagnostic as a Title Screen mode before the prototype play setup, so the title remains reachable from the menu.
- The native launcher now exposes a CHR Playground mode that renders the selected local-only CHR bank, tile IDs `$80-$AF`, the assembled 2x2 title glyphs, and sample glyph tile mappings for visual QA. It defaults to Bank 31 because that is the verified title glyph bank, but it can switch banks at runtime.
- The native launcher now exposes an Intro Lab mode that renders the selected local-only CHR sheet and assembles `TECMO PRESENTS` through the Bank 06 character map against the selected CHR bank for placement QA. Bank 31 is only the default starting bank, not an intro-source assertion.
- The likely source candidates for the "TECMO PRESENTS" intro are Bank 04 intro/presentation chunks `C-0116..C-0134`, `C-0136..C-0140`, Bank 00 encoded text/layout streams around `C-0191..C-0192`, Bank 00 tile/block candidates around `C-0195`, and Bank 03 intro dispatch/selection chunks `C-0142..C-0143`.
- The current execution-procedure lead is Bank 04 sequence/dispatch setup feeding fixed-bank helper `$C051`, which resolves locally to `$D861`; that helper appears to be the first useful native model for stream-to-OAM-style sprite staging.
- The aggregate native staging pass covers 15 selected streams, 415 records, and 1660 staged writes across local staging range `$0200-$0287`; payload bytes are not retained.
- Detailed fixed helper side effects and palette setup still need a native model before the CHR-backed quick launch can be called pixel-accurate.

## Next Native-Port Gates

- Decode resolved fixed helper bodies into explicit native pattern-table/VRAM staging operations where needed.
- Replace fixed-bank helper effects such as frame waits, render writes, and setup wrappers with explicit native C functions.
- Decode the resolved fixed helper bodies and queued PPU path that initialize title/menu palette RAM.
- Map title/menu tile IDs to local CHR bank(s) while keeping extracted bytes local.
- Convert the Bank 04 pattern/script records into safe native structs or a local-only generated cache.
- Replace the probe font/colors with mapped title/menu CHR, palette, and layout data while keeping generated assets local.
