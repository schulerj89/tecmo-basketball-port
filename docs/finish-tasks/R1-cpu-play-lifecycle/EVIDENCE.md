# R1 CPU lifecycle evidence

Classification in this file is deliberately narrower than a play-name claim:
`exact` means the bytes, address/control path, or bounded native write are
source-pinned; `inferred` means only a conservative descriptive label;
`deferred` means the required source RAM/caller context is not represented.

## Revision identities

| Input | Identity | Use | Confidence |
| --- | --- | --- | --- |
| Canonical Rev1 iNES ROM | SHA-256 `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`; full-ROM FNV1a32 `0650F5B0` | importer/research/test only | exact |
| FCEUX reference binary | SHA-256 `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B` | private trace tool lock | exact identity gate; accepted draft and formal PASS |
| TGAI-1 payload | 7616 bytes; FNV1a32 `D6C4DB35` | runtime semantic asset boundary | exact |
| TGMO-1 dependency | 1664 bytes; FNV1a32 `6C82A137` | same-pack dependency | exact existing contract |

## Existing TGAI-1 source spans

These spans remain the unchanged source-map/payload boundary. FNV values are the
asset descriptors used by the strict importer.

| Bank/address or routine | Bytes | FNV1a32 | What it proves | Confidence |
| --- | ---: | --- | --- | --- |
| Bank06 `$81F7-$82D3` actor/state loop | 221 | `23BB7271` | ten-actor state-4 entry/update boundary | exact |
| Bank06 `$87AE-$88AF` reference direction | 258 | `F866B06C` | actor/reference delta path | exact |
| Bank06 `$88DA-$8A95` target direction | 444 | `9616E586` | bounded target-minus-actor octant path | exact |
| Bank06 `$8B90-$8BE0` fetch/dispatch entry | 81 | `9AD2BA91` | actor offset and command dispatch boundary | exact |
| Bank06 `$8BE1-$9237` handler cluster | 1623 | `344298FE` | 24 handler entry bytes | exact |
| Bank06 `$9280-$9329` target apply | 170 | `C82E6853` | target application/common tail | exact |
| Bank06 `$938B-$9620` formation selector | 662 | `47818A62` | formation pointer/offset selector | exact |
| Fixed `$C006-$C008` trampoline | 3 | `14B2472E` | fixed-bank mapping handoff | exact |
| Fixed `$CBE0-$CBF6` five-byte reader | 23 | `41C5B5C8` | Bank04 read to `$C7-$CB` | exact |
| Bank04 `$9F2E-$AC75` command corpus | 3400 | `71331A96` | 680 aligned five-byte records | exact |

Additional source-pinned handler anchor: Bank06 `$8B90-$8BEF` has SHA-256
`0C08DE6DE50B59BF9EB666114182F1B297DAD916CA1E6864000FBBA5B65153F42`; the
handler table in opcode order is:

```text
$90E0 $934B $9280 $905E $8FFA $8F92 $8F2D $8F12
$8ED7 $8FC5 $8CD0 $8C40 $8E4F $9125 $9146 $9172
$9085 $8C1A $8C1A $8C1A $9032 $8BF6 $8BE1 $8F72
```

The corpus histogram for opcodes 0 through 23 is:

```text
98,143,150,171,2,1,1,2,8,12,1,2,1,2,2,2,2,64,0,0,2,6,7,1
```

The canonical ROM bytes were re-read for the proof hook boundary: `$8B90` is
`BD 47 05` (`LDA $0547,X`), `$8B9F` is `20 06 C0` (`JSR $C006`), `$8BA2` is
`A0 C7` (`LDY #$C7`), and `$8BAE` is `6C A4 00` (`JMP ($00A4)`). The indirect
dispatcher is therefore `$8BAE`, while `$8BE1` is opcode 22's handler. The
handler-table bytes begin at static data anchors `$8BB1/$8BC9`; they are not
runtime executable hooks. Inline comments in
`decomp/lifted/bank06/C-0019` drift two bytes early across this fetch/dispatch
body before realigning at the table, so canonical ROM bytes and mapper-gated
addresses -- not those comments -- are hook authority.

## Out-of-span lifecycle anchors

The importer validates these ranges directly against the exact ROM without
adding them to the TGAI-1 payload or source-map. The first five are used for
source identity; the candidate range is identity-only evidence because its
dynamic callers/outcomes remain outside this slice.

| Bank/address | SHA-256 | Exact evidence | Confidence |
| --- | --- | --- | --- |
| Bank04 `$AC76-$ACF0` | `AA296CBBF2269130F13C8D6983D8974517710B9A0641A6E9770E50438E07A20A` | code-resume/fixed-link and startup neighborhood | exact bytes; semantic labels bounded |
| Bank04 `$ACD9-$ACE3` | `4761CF44148247C6B96046AE8FA2A9B899BCDD2A3BCB2AD61EFA3BEBBAD9414D` | `$ADD6,X` load/store to `$06CB,X`; startup fixed-link producer | exact |
| Bank04 `$ADD6-$ADDF` | `710E206A0E4A6919A8323E87F40D891B73F8FBC204EA286CE75DE5ED75440155` | bytes `{05,06,07,08,09,00,01,02,03,04}` | exact |
| Bank05 `$96B6-$9708` | `307715F21D95CEEB5033EDD4DD77BE665215E5F2993663D9AB81B17A50D40A48` | two-route selector; table `$9709-$970A` is `{00,80}` | exact mechanics; route names inferred |
| Bank06 `$8374-$84B6` | `0E34FEFAC7DC767B0A0286FD3BD7A849A2495D24F003A18DABFB186F9BB4981F` | CPU shot-request gates, difficulty bytes `{12,1C,28}` | exact predicate; outcome deferred |
| Bank06 `$B081-$B365` | `AAA9670DA5942FA2614F925A266674893A352BB2DB3A8F4158F61C8AE891AE36` | candidate/filter bytes and `$B32C` neighborhood | exact identity only; dynamic selection deferred |

## Exact lifecycle anchors

- Fetch offset is `($0551[x] << 8) | $0547[x]`; Bank04 base is `$9F2E`.
- `$8FD9` advances one five-byte record. `$8FE8` rewinds five and can cancel
  a prior advance. Opcode 1 replaces the offset and is the only immediate
  same-tick record chain; the native step budget bounds its cycles.
- Fixed startup `$06CB` is `{5,6,7,8,9,0,1,2,3,4}`. It is not the dynamic
  candidate/matchup/primary vector. Other exact startup seeds are `$0308=4`,
  `$0309=9`, `$037F=2`, `$0380=7`; the native contract keeps `{2,7}` as a
  separate `matchup_seed` pair.
- Formation theory is 48 starts (4 depth rows x 12 buckets), but only 46
  rows are source-pinned/aligned inside `$938B-$9620`; theoretical indices 46
  and 47 are rejected.
- Bank05 route control is indexed by `$030A` into the two `$030C/$030D`
  entries. A zero control entry performs no route write. Otherwise the exact
  branch compares `($0373 & $80)` with `$9709[$035A]`, then `$0095 != 0`, then
  `$0094 < $28`; it writes `$007D` or `$00D7`, clears the high offset byte, and
  sets state 4.
- The original-reference proof uses a setup/tip/clock gate. It stops the menu
  schedule at the validated MAN VS MAN setup, supplies P1 A at tip ages 30-34,
  A+B at 35-37, and B at 38-55, then starts the post-clock capture delay only
  after the clock leaves the stopped state. A complete frame window with no
  fetch, copied-opcode, indirect-dispatch, handler, advance, aligned stream,
  or exact fixed-link evidence fails closed.
- `$06CB` contains only the ten fixed startup links for actor slots 0 through 9;
  actor slot 10 has no fixed-link entry and is reported as absent in proof
  actor rows.
- The CPU shot-request predicate is ported as a pure transactional request.
  Caller-supplied random input is retained; no RNG or shot outcome is invented.
  Success corresponds to the source jump `$9217`, whose bounded state writes
  are state `$057C=$0D`, action `$0458=$32`, and `$046E=$12` before `$9270`.

## Confidence boundary

The source proves transport, addresses, bounded arithmetic, gates, and selected
RAM writes. It does not by itself prove the basketball intent of handler names,
complete route reachability, dynamic candidate ownership, all pass/steal/shot
choices, or parity of live scene scheduling. The original-reference proof Lua
therefore records raw PC/mapper/register/RAM evidence with separate
`address_confidence=exact_source_pinned` and bounded `label_confidence` columns.
Generated `handler_N` labels mean exact opcode-entry addresses; candidate,
switch, and deferred semantic names remain inferred or deferred and never
inherit exact intent from their address.

## Accepted eleventh draft evidence

Sol accepted the deterministic draft session
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-051716/` with generated UTC
`2026-08-03T10:17:40.6360836Z`. It is `status=draft_pass` at base and head
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`; at generation `final_sha` was
pending, and the resulting worker implementation/evidence commit is now
`db5a043244361b3e9bbab2e154c7f14e4a4a5014`. The session contains 102 files, 100
inventoried artifacts, 36 nonempty runner-metadata logs, zero empty files, no
`.incomplete`, and `33,652,224` bytes. The preserved generated manifest is
`130,114` bytes, SHA-256
`457EB6E50BCCBC113B439104143F5834D55C836C9C47E3CC1E2B4D7F6588165A`; the
summary is `2,936` bytes, SHA-256
`8A8976146A36B0D8431F0CF04AF392B04DDF9461A5B06CB377A3E7D2E9E690CE`.

The exact ROM and FCEUX identities are the revision hashes above. The fresh
pack is SHA-256
`8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`,
`1,397,729` bytes; TGAI-1 remains `7,616` bytes/FNV1a32 `D6C4DB35`. Both
original runs are identical: trace SHA-256
`9EE4DA566665800ECA40E02919AB5323634364620EB71907BCD1901A43A2169D`, actor
SHA-256 `69404667384CC604CE7DD600D33D2F2C232C35FEF6AAE8F3626605E01BF84E6A`,
frame-index equality, and two `768x896` sheets of `76,643` bytes with SHA-256
`2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
Each original run has setup/tip `3929`, live `4101`, capture `4125`, 120
frames, 12 screenshots, 5,243 trace rows, 1,320 actor rows, fetch/opcode/
dispatch `555` each, 1,158 handlers, 551 advances, 367 rewinds, 555 aligned/
fixed-link observations, and zero mismatch/invalid/misaligned counts. Final
progress is sequence `4245`, emu frame `4243`; lifecycle, final progress,
speed mode, and capture completion are true.

Sol personally inspected all 5,243 trace rows across 120 frames: 18-69 events
per frame; fetch `$8B90`, copied-opcode `$8BA2`, dispatch `$8BAE`, and advance
`$8FD9` are the observed addresses. Confidence pairs are
`exact_source_pinned+exact_mechanics=3,258`, `exact_opcode_entry=1,158`,
`deferred_mechanics=707`, and `inferred_label=120`. Actor CSV has 11 rows per
frame for slots 0-10; slot 10 stream/fixed-link fields are `NA`, and traced
fixed-link text is `05:06:07:08:09:00:01:02:03:04`.

Native continuity evidence has 12 primary frames 25-36 and 12 exact repeats at
`640x480`; the native sheet is `1920x1920`, `261,899` bytes, SHA-256
`4F3AF89F575572CE80C976B141207D45930DB952C5788C1588C816A7A8160DC9`. Both
MP4s are `31,777` bytes with SHA-256
`66632EF630E2798D0908E982502BE854A59E4D7DF054288BB6EE84F6DD85988C`; ffprobe
reports H.264, `640x480`, exact `r_frame_rate`/`avg_frame_rate=39375000/655171`,
`time_base=1/39375000`, `duration_ts=7862052`, duration `.199671`, and both
frame counts 12. Tool hashes and versions are retained in the private
manifest.

Personal visual inspection found both original sheets identical and intact:
Bulls-Celtics live movement, clock `3:59 -> 3:57`, readable HUD/court/crowd/
sprites, and no corruption. The inspected original full-resolution frames
were 0001/0006/0012. Native frames 0025/0030/0036 and the native sheet show
intact Hawks-Celtics continuity at `3:00`, readable HUD/court/crowd, moving
sprites, black margins, and no corruption. Team, clock, framing, and scenario
differ; native output remains legacy harness/formation regression evidence, not
one-to-one parity or scene integration.

The warning-clean Win32 production smoke also passed with project-root,
subsystem, icon, startup, lifetime, and clean-shutdown checks. At the time of
the draft snapshot, formal proof had not yet run; that historical state is
superseded by the formal PASS and independent QA closure below.

## Personal raw-ROM reinspection

The canonical ROM SHA was personally reconfirmed. The exact bytes at the
runtime evidence boundaries are Bank06 `$8B90 = BD 47 05`, `$8B9F = 20 06 C0`,
`$8BA2 = A4 C7`, `$8BAE = 6C A4 00`; static handler-table data `$8BB1/$8BC9`
pairs opcode 22 with `$8BE1`. `$8FD9` advances five bytes and `$8FE8` rewinds
five. Bank04 `$ACD9` is the fixed-link producer and `$ADD6-$ADDF` is
`05 06 07 08 09 00 01 02 03 04`. The CPU shot gate is Bank06 `$8431`, with
timing bytes `$84B4-$84B6 = 12 1C 28`. These observations reinforce the
existing exact-source classifications; they do not promote inferred handler
intent, native harness output, or scene integration to exact parity.

## Formal clean proof and independent QA closure

The formal clean proof passed at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-053244/`, generated UTC
`2026-08-03T10:33:08.4555394Z`, with status `pass`, base
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`, and proven final/code-doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd` on the pinned worker branch. The
tracked and nonignored worktree was clean, personal inspection was complete,
and no pending metadata remained. The formal manifest is SHA-256
`E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`; the
summary is SHA-256
`78C91AAF981C075BF9088EE4618EBB73CDB740DF08E12B5AC1D5E125C5419252`.
It contains 102 session files, 100 inventoried artifacts, 36 nonempty logs, no
empty files, and `33,650,575` bytes. The formal identity gates are ROM
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`, FCEUX
`F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`, fresh
pack `8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`
(`1,397,729` bytes), and TGAI-1 `7,616` bytes/FNV1a32 `D6C4DB35`. The two
original runs and native/video facts were reproven deterministically.

Independent final QA was performed by thread
`019fc628-0b32-7e83-b969-b41990b36e9b`, `gpt-5.6-luna/max`, repinned for
read-only final QA. It accepted implementation/code-doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd` with no P0/P1 findings. The only
finding was this bounded P2 documentation-state revision. QA reran the static
lab and focused CPU wrapper with the canonical Rev1 ROM and worker project
root; both passed, including 680 commands, 24 handlers, and 17 ROM mutation
rejections. QA reported a clean tree, zero bad-request faults, no mutation,
and no FCEUX/private proof. Formal proof limitations remain: dynamic policy,
workspace effects, and normal scene integration are deferred; native video is
deterministic legacy-harness regression evidence, not one-to-one original
parity.
