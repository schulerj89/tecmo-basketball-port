# R3B Current-Main Integration QA — Evidence

## Inputs and repository identity

The private ignored Rev1 ROM was validated before use at 393,232 bytes with
SHA-256 `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The ROM and decompilation tree were local QA prerequisites only. The final
corrected build produced the console and GUI binaries with zero compiler-
warning lines.

The code merge was `91f158456d72537f0a8b6ae032cf0b0ade053493`, tree
`94c9c2802fe5a5f40ead49a6b591d13e41b4e30d`, with first parent
`bdc2fbb8...` and second parent `7897871...`. Both source and merge signatures
were `Good`. The frozen correction checkpoint is signed `Good` at
`20dcf9a4d30f8d4e557ab61df5af8fc34458c82c`. The R3/R2E collision ledger was
empty.

## Corrected fixture evidence

The former proof-only seed had FGA 400, 3PA 200, and FTA 120 while makes were
`300+key`, `50+key`, and `90+key` through key 323. The corrected seed uses
FGA 800, 3PA 500, and FTA 500. This is the only implementation correction;
formula/runtime/ranking semantics and make vectors were preserved. Five
populated render hashes changed and category 3 TOTAL POINTS remained the same.

The corrected first/repeat captures are under
`build/integration-qa-r3-20260804T184202450Z/season-leaders-frames-corrected/`
and `.../season-leaders-frames-corrected-repeat/`. Both captures are 640x480
and have identical hashes:

| Frame | SHA-256 |
| --- | --- |
| category 0 page 0 | `600E13073B9D8509E7E5648E8AFA5221E7E038CB51D28C40AA952E5B4B80C1AB` |
| category 0 page 6 | `F07071A9032AB6CD6B2307ED4C007AE1995B5C8CD4E37A1F205D0890368AAE14` |
| category 0 page 12 | `9C2C058CA7EB355C48ED6533536088A641D7866B16EB57C5CF01410F1FEF4FD1` |
| category 3 | `794DA4AE2CC6FB0B75B1F30A4F682B565F5B16A3DBC26BD0E594ABC9763A182E` |
| category 5 | `74871EF3FFE4EA643CD707A95B29389EC487552AB9B4D7571F3B56A526EB96FE` |
| category 6 | `D2561DF4460C843B85127C1B6D4AA59DBDC0640DCF186562796BAF0FCB5F1FBD` |

The exact first/repeat paths and UTC write times are preserved in
`docs/finish-tasks/R3-player-stats-leaders/PROOF.md`. The historical 11:19
table in that file retains its original hashes and is explicitly superseded;
it is not presented as a current-QA regeneration.

## GameplayScene proof

Final corrected proof directory:
`build/integration-qa-r3-20260804T184202450Z/gameplay-scene-proof-final-corrected/`

- `PROOF-MANIFEST.json` SHA-256:
  `D36F043B73BD9E1CE0313F1D8446E1C19B54F5F82EEE1ABFBAFA333D39207E50`
- manifest code head: `20dcf9a4...`; `clean=true`; status `DRAFT`; two repeats;
  14 stored/decoded frames at 640x480.
- asset pack SHA-256:
  `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`
- both 1920x1440 contact sheets SHA-256:
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`
- both native videos SHA-256:
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`

The two scene repeats produced identical event-frame hashes for pretip-start,
live-handoff, human-movement, offensive-pass, defensive-switch,
cpu-target-deferred, and shot-path. The original-reference status remains
pending; no emulator-perfect parity claim is made.

## GameplayPresentation proof

Final corrected proof directory:
`build/integration-qa-r3-20260804T184202450Z/gameplay-presentation-proof-final-corrected/`

- `manifest.json` SHA-256:
  `D1898DA237372F445BAD05DEA26143838EEDCDA094047AD14976A62604B04252`
- the runner exited 0 with active frames 1–16, terminal frame 17, numeric
  variant 2, two-pass equal hashes, and the six negative framing cases passed.
- `gameplay-layup-contact-sheet.png` (2560x2400) SHA-256:
  `85995E1654354BFFF874AEA8510F91FD6E0AB1BCECD49C5138EBC116FB9B6A6C`

The manifest's `clean=false` is a truthful record of the pending tracked
`PROOF.md` labeling edit at capture time; the code head was the clean signed
correction checkpoint `20dcf9a...`.

## Visual review boundary

The six corrected League Leaders frames were inspected individually at original
resolution using the game screenshot QA checklist. FIELD GOALS page 0 had a
centered title, six readable rows, aligned team/name/metric columns, and
sub-1.000 percentages. Pages 6 and 12 were readable continuation layouts.
TOTAL POINTS, 3 POINT SHOTS, and FREE THROWS had readable titles, labels,
values, and no visible clipping, garbage glyphs, or alignment collisions.
These are native-port frames, not original-reference images.
