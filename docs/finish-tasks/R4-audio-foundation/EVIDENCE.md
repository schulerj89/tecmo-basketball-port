# R4 Audio Foundation source evidence

This document records the private-source facts used to build and validate the
semantic pack. The canonical Rev1 ROM is research/test evidence only:

- revision: Tecmo NBA Basketball USA Rev 1
- private ROM SHA-256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`
- runtime policy: normal native C runtime consumes validated pack entries and
  never opens or parses the ROM

## Serialized contracts

| Contract | Bytes | FNV-1a32 | Contents | Confidence |
| --- | ---: | --- | --- | --- |
| TMUS-1 music | 36,784 | `05C00ECB` | 2,251 instructions / 37 voices | exact-high; importer postconditions and parser gate |
| TFSX-1 frontend SFX | 1,792 | `985DC7ED` | 87 instructions / 3 voices | exact-high; importer postconditions and source gate |
| TSFX-1 gameplay SFX | 2,824 | `968A5DE6` | 131 instructions / 14 voices | exact-high; isolated gameplay importer gate |
| TDMC-1 gameplay DMC | 2,515 | `AD70E6E8` | five clips / three pools | exact-high for serialized bounds and pool data |

The strict importers require the public compatibility contract of at least
eight declared PRG banks for music, gameplay audio, and frontend audio. They
also check declared-bank ranges before fixed-bank arithmetic, checked public
offset addition/multiplication, exact serialized sizes, instruction/voice
counts, full-ROM identity, and same-pack dependencies where applicable.

TDMC accepted serialization requires `pool_offset==0`. Runtime subtraction-form
relational and queue-time checks remain defense in depth; they do not represent
a reachable mutated-offset exploit in an accepted TDMC pack.

The semantic runtime facts carried by the validated pack are TMUS PCM
`105B1338` / state `1C74513C`, and TSFX/TDMC PCM `83E60072` / state `17208C83`.

## Reviewed bank and address evidence

- TMUS-1 uses the Bank04 `$8AA4` source/directory contract. Track spans are
  `$8CE2-$9F06`: ID 5 `$92F4-$96C3` (`1270498B`), ID 6 `$96C3-$9D8B`
  (`BD91FCF1`), ID 7 `$8CE2-$92F4` (`69F85EC2`), and ID 8 `$9E13-$9F06`
  (`8122C6CF`). The fixed engine is `$F2F2`, pitch table `$F93B`, Bank04
  queue `$826A`, first route `$82CF`, fixed menu queue `$E477`, and Bank06
  pregame-matchup queue `$A145` (5 bytes, `1E564AC0`).
- TSFX-1 uses Bank04 directory `$8AA4`/32 bytes (`6283F255`), core `$8AA4`/556
  (`548EED95`), and extension `$9D8B`/136 (`838408D4`). Mapped effects are:
  ID 3 `$9DF7-$9E13` (`B7138F94`), ID 5 `$8C5D-$8CA3` (`28EE1024`), ID 6
  `$9D8B-$9DF7` (`34460805`), ID 11 `$8AC4-$8AF6` (`93E7AC2C`), ID 12
  `$8AF6-$8B35` (`B172920D`), ID 13 `$8B35-$8B6E` (`DC401221`), and ID 14
  `$8B6E-$8B97` (`E3035B54`).
- TDMC-1 uses fixed-bank DMC pools `$C080`/513 (`33E109D7`), `$C440`/721
  (`6ECC107C`), and `$C740`/945 (`F621FD7C`). Fixed event/trigger evidence
  includes `$E7DB`, `$E863`, `$E86D`, and Bank05 `$A8D6`, `$A9C5`, `$ABF5`,
  `$B5AB`.
- TFSX-1 uses Bank04 ID 8 `$8BF7-$8C2A` (`AC9D4C1F`) and ID 10
  `$8B97-$8BF7` (`963DC35E`), Bank03 title setup `$8056`/32 (`4A97C61D`),
  confirm `$8076`/27 (`0C902C97`), and fingerprinted fixed bridge/dispatch
  ranges.

The Sol source review also recorded these exact semantics:

- Bank07 `$F7D5-$F7DB`: `LDY #$01; LDA ($3E),Y; LSR; LSR; STA $06AC,X`.
- `$91 $00` is the pitch-reset path and zeros `$0697` and `$069F`.
- The native cadence contract is `39375000/655171`; track 7 stops at tick
  2615 and track 8 stops at tick 396.
- Bank05 DMC evidence: `$A8D6` source `$1D`/rate `$0E`/enable `$1F`;
  `$A9C5` source `$1D` length `$3B` rate `$0E` enable `$1F`; `$ABF5` source
  `$11` length `$2D` rate `$0F` enable `$1F`; `$B5AB` source `$02` length `$20`
  rate `$0F` enable `$1F`.
- Fixed `$EC06` clears `$4015` and wait/pointers without writing `$4011`;
  Bank06 `$A145` queues music ID 8.
- TDMC evidence covers held-DAC end, retrigger, and clear behavior. The fixed
  clear path does not write `$4011`.

## Fidelity classification

| Evidence/behavior | Classification and boundary |
| --- | --- |
| Source spans, hashes, bank layout, importer offsets, serialized counts | exact-high for the Rev1 contract; mutations fail closed |
| TMUS IDs 5–8, TFSX 8/10, TSFX 3/5/6/11/12/13/14, TDMC metadata | exact-high semantic pack contract |
| `39375000/655171`, last-write-wins mailboxes, matching-channel override, future-track-5 music gate | exact-high native contract coverage |
| DMC end/retrigger/clear held-DAC levels | exact-high for the declared state boundary |
| Nonlinear/cycle-exact NES APU mixing and DMC reader bit/IRQ phase | approximation/deferred; not claimed |
| DMC IDs 0/1/2 | unresolved/address-bound; not overclaimed |
| Effect 5 | neutral/unresolved |
| Effect 6 | bounded correlation only |
| Cross-domain cue call sites and full ACC-AUDIO | deferred and outside ownership |

The source evidence establishes the semantic boundary; it does not turn the
native renderer into a cycle-exact emulator or authorize edits to excluded
call-site/build/platform files.
