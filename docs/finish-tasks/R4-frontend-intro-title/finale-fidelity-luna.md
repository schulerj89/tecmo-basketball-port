# R4 frontend intro title / finale fidelity

## Scope and non-goals

This worker line owns the native finale route importer, TFIN semantic parser,
finale state machine/renderer, finale-specific `Run-IntroSequenceTests.ps1`
coverage, and this evidence note. It does not change the CLI scene-mode
dispatcher, central asset-pack registration, menus, audio, gameplay, build
files, Win32 code, or the attract/title continuation implementation.

Runtime consumes only the strict native TFIN entry and same-pack `chr/all`.
ROM/ASM/decompilation and the ignored FCEUX frames are importer research and
verification evidence; none of those bytes or capture artifacts are packed or
committed.

## Reproducible source evidence

The canonical Rev1 ROM is:

```text
C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes
size 393232
SHA256 076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4
```

The decompilation used for address-level reconciliation is
`C:\Users\joshs\Projects\disassem\tecmo-basketball-decompilation`.
The ignored original capture is
`C:\Users\joshs\Projects\tecmo-basketball-port\temp-videos\fceux-finale-frames-1350-2550\frames.csv`
and its `fceux-<frame>.png` checkpoints.

The capture is 256x224 with the first eight scanlines cropped. Native proof
PNGs are a 256x240 viewport rendered at 2x with output origin `(64, 0)`:
capture `y` maps to native `y + 8`, and native `(x,y)` maps to output
`(64 + 2*x, 2*y)`. Thus capture title rows 121..134 and 139..140 are native
rows 129..142 and 147..148; caption capture rows 201..214 are native rows
209..222.

Bank04 dispatch `$82CF-$82F9` selects `$851C`, `$83EA`, `$852E`, `$83AE`, and
`$8310`; the validated route source window is `$82CF`, size 913,
FNV1a32 `FAEF1D02`. The relevant screen/transition evidence is `$8303-$8427`
and `$850C-$863B`, including the `$8A0D` short palette, `$89FD` helper
palette, `$8A1D` special palette, selector operands, and C000 wait loops.
Bank04 `$83AE` executes `lda #$09; jsr $8A6E`; `$8A6F` derives the C036
team color and mirrors it to palette work-area absolute slot 9 (`$032E,x`
and `$030E,x`).

The fixed Bank07 title/IRQ source is `$FE14` (126 bytes, FNV1a32
`E688E7F2`), with the validated title split/IRQ chain (`$83A3 -> $BA16`,
mode `$0100=06`, `$0352=$1F`). The fixed screen descriptor source is
`$DC85` (322 bytes, FNV1a32 `66D469E1`); Bank00 sprite geometry is sourced
from the validated pointer/stream `$A90F/$A9D2` contract (geometry fingerprint
`E0B9063F`). The Bank06 title handler is `$9E50` (90 bytes, FNV1a32
`9E459370`), its character map is `$A273` (91 bytes, FNV1a32 `C103E39D`),
and glyph quads are read from `$AF05`.

Caption names are imported from the Bank06 `$AC4A` table (272 bytes, FNV1a32
`AA8FC37D`) and validated as SUNS id `$14`, SPURS id `$17`, and BULLS id
`$03`. The fixed team-color table `$DC19-$DC35` is imported/validated as
29 bytes, FNV1a32 `1451114F`; BULLS id `$03` resolves `$DC1C = $15`. The
semantic TFIN field carries that validated `$15` value; the runtime does not
invent a team color.

## Capture-backed route and title timeline

The native local-frame convention is the first rendered frame of each route
at local 0. The accepted windows are:

| Route/source | Capture frames | Native duration | Presentation gates |
|---|---:|---:|---|
| `$851C` opening | 1591..1674 | 84 | black 0..6, terminal black local 83 |
| `$83EA` short/SPURS | 1675..1733 | 59 | black 0..5, terminal black local 58 |
| `$852E` selector | 1734..1785 | 52 | black 0..7, pulse local 24, tail local 51 |
| `$83AE` staged/BULLS | 1786..1974 | 189 | black 0..6 |
| `$8310` title | 1975..2591, then hold | 617 | black 0..14; first nonblack local 15 |

SUNS reveals across opening locals 29..32 (absolute 1620..1623), SPURS
starts at local 7 (first glyph absolute 1682) and remains visible through
local 57; local 58 is black. BULLS reveals across staged locals 29..33
(absolute 1815..1819). The short route uses decoded screen `$20` page 1.
Its setup is locals 6..13 with no metasprite; duplicate two-frame anchors
are visible at 14..27, the driver wait is 28..57, and the hardware OAM Y
offset is modeled as stored Y + 1.

The selector uses repeated water page 1 through the second move, with
visible first-motion locals 8..23, black pulse 24, page-1 hold 25, and
second-motion locals 26..50. Palette stages start at locals
`{8,12,16,20,25}`. Stages 0..2 brightness-cap the selector sprite; stage 4
uses the special palette unchanged. Opening and staged background/sprite
palettes use four-frame caps from local 7: stages 0 at 7..10, 1 at 11..14,
2 at 15..18, and 3/full at 19 onward. Staged absolute palette slot 9 is
overridden to the semantic BULLS `$15` before the cap is applied, in both the
background and sprite palette work areas.

The title uses the ordinary two-page renderer. Its screen-4 baseline contract
is page 0 row 18 tile `$FF`, palette 2 (blank), and page 1 row 18 tile `$F1`,
palette 2 (line), for all 32 columns. Full-coordinate title bands are
`0..144` primary, `144..152` secondary, and `152..240` primary; title slots
remain row 16. Secondary page starts at page 0 and preroll uses
`secondary_iterations = cursor + 1`: local 15 is scroll 2/page 0, local 142
is scroll 256/page 1, and local 143 is the dispatch/no-write frame.

The title write/tail convention is intentionally explicit: locals 144..487
are the 344 slot-write progression frames (slots 1..43), local 488 is the
first tail frame with slot 44 already present and retreat count 1, locals
488..615 are retreat counts 1..128, local 616 is the trailing dispatch wait,
and local 617 is the terminal hold/handoff. `$8A` remains zero through
absolute frame 2118, becomes `$01` at 2119, advances every 8 frames, and
reaches `$2C` at 2463; the C000 `$7F` loop renders 2464..2591.

## TFIN schema, provenance, and compatibility

TFIN remains version 1 (`TFIN-1`) with a 192-byte header. Screens still begin
at offset 192; semantic metadata is confined to the existing header extension
116..191. The `TFM1` semantic record contains route durations/gates/pulse/tail
bytes, caption route/reveal/row data, glyph references, the four-byte N glyph,
and staged team color at absolute byte 180. Bytes 181..191 remain reserved and
must be zero. Import writes the complete record transactionally before the
pack is emitted; the parser checks same-pack screen cells, title baseline tile
and CHR identities, all CHR ranges, and the `chr/all` fingerprint.

There is deliberately no TFIN-2 version bump. This is a compatibility change
within the owned TFIN-1 contract: old TFIN-1 payloads whose former all-zero
extension lacks the now-required route/caption/title/team semantics are
rejected fail-closed, rather than silently falling back to normalized timing
or a hard-coded team color. The tests mutate byte 180, byte 181, title-band
reserved bytes, and the screen-4 baseline cells while proving screen payload
bytes remain unchanged for header-only mutations.

## Changed functions and test proof

The importer now validates the fixed team-color table, writes the semantic
route/caption/title/team record, imports the screen-4 baseline provenance, and
retains strict Rev1 CHR/table fingerprints. The runtime parses the semantic
record, models route state boundaries, page identity, OAM Y+1, brightness caps,
the staged slot-9 override, split title bands, title preroll, tail, and hold.
The PowerShell suite covers state strings, title native rows 129/147 (and
the corresponding duplicate-row guards), caption native bounds 209..222,
short page/anchor/OAM checkpoints, selector page-1/stage/scroll checkpoints,
opening/staged RGB/count/bounds at all four cap stages, malformed semantic and
baseline data, screen-byte round trips, ignored-capture masks for opening
locals 7/82, short locals 6..58, selector locals 8..51, staged locals 7/29/188,
title locals 15/142/143/144/151/159/487..490, and repeated render SHA256
determinism.

Reproducible local proof outputs are generated under ignored paths:

```text
build/intro_sequence_test_report.json
build/intro_sequence/
build/finale-proof-new/
```

The full-resolution ignored FCEUX checkpoints remain at the capture path
above. No ROM, PNG, capture CSV, trace, decoded private payload, or save state
is part of this lineage.

The final full-suite command was:

```text
.\tools\Run-IntroSequenceTests.ps1 -ProjectRoot . -RomPath "C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"
```

It passed with 0 failures and 1 unrelated skipped bounded-reference case;
the finale capture manifest compared 47 ignored PNG masks, all passed. The
final report SHA256 is
`F1D565C348513477DA19A0E42AE082CD0259DE78C6DD98F995EFF8BDBF2F271C`.
Selected ignored proof hashes are:

```text
build/intro_sequence/intro-finale-opening-clean-frame7.png
  4B44F57667484B443B9209A004E465671D17B5D7F025463EB276D38D43A4BC35
build/intro_sequence/intro-finale-staged-clean-frame29.png
  A5D175752C1A11F6D36D9149867C9F1489C080D6808E6AF918B2DD7592027C35
build/intro_sequence/intro-finale-title-clean-frame488.png
  B078F5D0E040487EEC3F1E184422F69718DE048EFB37D2FC1B1C3A3BF2B6C177
build/intro_sequence/finale-color-staged-team-stage1.png
  18BABC1C37FBFDCE0C28550AA1EC0103075589DC2FE00CC50BEC264A61917E62
```

For auditability, an earlier post-color suite run reported two failures: the
selector RGB bounds still used the pre-OAM-Y+1 rows, and title locals 615,
616, and the terminal hold were still classified as nonblack despite the
source-backed fully retreated/black pixels. Those were test-expectation-only
corrections; the subsequent full run above passed. The later mask expansion
added opening locals 7/82 and staged locals 29/188 and retained a clean
47-comparison pass.

## Visual observations, approximations, and deferred boundaries

The native masks match the sampled original frames after capture-to-native
row normalization, including short setup/loop/dispatch, selector page-1
repeat and both moves, staged geometry, and title preroll/write/tail rows.
The color checkpoints additionally match the source-backed cap cadence and
the staged 1,885-pixel slot-9 team-color region. The native title handoff is
represented as the terminal hold state; attract continuation remains outside
this ownership line. Scanline behavior is represented by the validated
three-band semantic contract and ordinary page renderer, not by fabricated
capture payloads or a private inherited-line helper.

## Sol acceptance observations

Sol personally inspected original/native pairs for opening local 7 (dark-red
fade silhouette and lines), short local 14 (basket, ball, and SPURS), selector
local 8 (dark-red passer/ball and blue line), selector local 25 (water strip
and clipped ball), staged local 11 (BULLS dunk, pink uniform, ball, and rim),
staged local 188 (full caption, uniform, and rim), and title local 159 (TE and
magenta baseline). After applying the documented eight-scanline capture crop
mapping, every sampled mask and nearest-NES-palette-index comparison was
zero-difference. The only expected canvas distinction was the native
240-line viewport versus the 224-line capture. These are Sol's visual
acceptance observations, separate from the worker's own inspection notes.

## Final worker handoff

Parent/base: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.

Code/test implementation commit: `6000a0504e8a640c17698411103090d105f21245`
(`R4 finale: align Rev1 route timing and color provenance`).

Durable-evidence document commit: `e97f2d2498bf96ee7a552e71e9f87d1eadce8456`
(`R4 finale: record fidelity evidence and proof manifest`).

This final documentation verification update is the next ordered commit in
the worker lineage; its exact SHA is reported in the worker handoff. The
document intentionally does not self-reference that content-addressed SHA,
and contains no unresolved merge-SHA placeholder.

Sol merge command after review:

```text
git merge --no-ff codex/r4-frontend-intro-title-finale-luna
```
