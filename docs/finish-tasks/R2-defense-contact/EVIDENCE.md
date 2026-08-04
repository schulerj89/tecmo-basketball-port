# Raw evidence and API mapping

## Canonical test-only ROM

The runner requires this local research/test input and never makes it a
runtime or committed dependency:

`C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`

| Property | Value |
| --- | --- |
| Size | `393232` bytes |
| SHA-256 | `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` |
| iNES header | `4E 45 53 1A 08 20 42 00 00 00 00 00 00 00 00 00` |
| PRG | 8 banks × `0x4000` = `131072` bytes |
| CHR | 32 banks × `0x2000` = `262144` bytes |
| trainer | none (`flags6 & 0x04 == 0`) |
| total | `0x10 + 0x20000 + 0x40000 = 393232` bytes |

For this supported legacy iNES image, the mapping is derived rather than
using an opaque file offset:

```text
PrgStart = 0x10 + (trainer_flag ? 0x200 : 0)
file_offset = PrgStart + bank * 0x4000 + (cpu_address - 0x8000)
CPU window = $8000-$BFFF
```

The runner rejects a NES 2.0 header, inconsistent header/trainer/PRG/CHR
lengths, trailing bytes, and any span outside the PRG range. The canonical
file's CHR bytes therefore remain part of the validated image without being
mistaken for PRG.

## Direct raw spans

| Evidence | Bank | CPU range | File offset | Bytes | FNV-1a 32 | SHA-256 |
| --- | ---: | --- | ---: | ---: | --- | --- |
| B06 functional span plus wrapper boundary | 6 | `$B081-$B108` | `0x1B091` | 136 | `87A88720` | `6BD687EABED16010B1DB4A0D81F532680DD3503F4757F3B5DCEA584A910CAF6D` |
| B05 geometry span plus following opcode | 5 | `$9968-$999E` | `0x15978` | 55 | `FF699FE9` | `5F3742E3D833700C25811333B3B4B9FE737FFC2FB62414CDA9B1FCC206CEEBF3` |
| B05 raw `$17` tail | 5 | `$9A24-$9A5F` | `0x15A34` | 60 | `953B37A4` | `1751F2A4AAC9A23A385BF172BC419260D7EFB20650B360FDB604DF67A7A5A66B` |

The last SHA-256 is the corrected authoritative value, exactly 64 hex
characters and with no appended character.

## Enclosing B06 source span (provenance only)

The enclosing B06 source span is checked separately by the runner using the
same derived iNES bank mapping. It is not a fourth direct implementation span
and has no additional API claim:

| Evidence | Bank | CPU range | File offset | Bytes | SHA-256 |
| --- | ---: | --- | ---: | ---: | --- |
| B06 enclosing source span (provenance only) | 6 | `$B081-$B365` | `0x1B091` | 741 | `AAA9670DA5942FA2614F925A266674893A352BB2DB3A8F4158F61C8AE891AE36` |

The three direct spans above retain their independent FNV-1a 32 and SHA-256
checks. The enclosing row is a source-span length/SHA verification only.

## B06 `$B081-$B108`

The functional scan closes at RTS `$B103` (`$60`). `$B104` begins a separate
wrapper. The direct `$B081-$B108` span contains its first five bytes
`AC 0B 03 B9 0C`; `$B109=03` completes the `LDA $030C,Y` operand and is checked
as an adjacent boundary byte, not included in the direct span fingerprint. The
public B081 API models only one descending cursor pass `9,8,...,0`; it does
not read a wrapper predicate and does not synthesize a second pass.

The raw scan contract preserves these observations:

- high threshold byte `$06D8` is initialized to `0x07U`; stale low `$06D7`
  remains the low threshold input;
- candidate `$0309` is skipped;
- `$04B0[candidate] & 0x10` is the gate;
- wrapped absolute X and absolute depth feed
  `max(delta_x, delta_depth) + floor(min(delta_x, delta_depth)/2)`;
- equality is rejected (`metric < threshold` only), so earlier accepted
  higher indices remain on equal ties;
- improvement writes the raw metric pair, `$06D5`, and
  `$037F[$030B]`; no improvement preserves stale values.

The threshold is formed as a complete `uint16_t` before comparison. The test
oracle includes metric `0x0800` against threshold `0x07FF`, which would be
incorrectly accepted by the former relational/bitwise precedence defect.

## B05 `$9968-$999E`

The proper geometry path ends at `$999D`; `$999E` is the following `LDA
$0499` opcode and is included only to anchor the span boundary. The native
path subtracts candidate X `$73/$E8` minus reference X `$7D/$F2`, retains the
SBC borrow branch, then checks the delta high byte and low-byte window. The C
API therefore accepts the raw coordinate pairs and reports the derived
wrapped deltas plus borrow flags.

The same raw delta is not sufficient by itself: candidate `$0000` minus
reference `$0001` produces `$FFFF` with borrow and passes the negative window,
whereas candidate `$FFFF` minus reference `$0000` produces `$FFFF` without
borrow and fails the high-byte/branch condition. Depth `$00-$01` versus
`$FF-$00` provides the corresponding 8-bit pair. These are direct regression
vectors, along with `0x0100`, `0x0107`, `0xFEFF`, mirrored `0xFFF8/0xFFF7`,
and exhaustive near-boundary coordinate matrices.

## B05 `$9A24-$9A5F`

The plan reports the local raw effects only:

```text
$0754[$030B] = old + 1       (8-bit wrap)
request external $C042 with X = $07
$0588 = (old & $EF) | $60
$BA   = old | $80
$0478 = $17
$0528 = $17
$0743 = 0
if raw $030C[$BE] != 0:
    $0458[$BF] = (old & $F0) | $05
```

The required route-context byte is exactly `1U`; it is a raw precondition, not
a semantic outcome claim. The result stores the raw state byte as `0x17U`.
The helper request is never invoked, and preceding vector helpers,
`$0742=7`, and the helper's own effects are outside this primitive.

## Cross-contract boundary evidence

The focused runner reads and hashes the accepted CPU, LIVE, and TIP/pretip
header/source boundaries before and after the run. It also checks, without
editing those files, that:

- TPNL says its API does not infer contact, collision, possession, or shooting
  state;
- TGSR claimant settlement says it does not name rebound, steal, block, or
  recovery;
- LIVE says `native_matchup_actor` and caller workspaces are not claims about
  the incomplete ROM dynamic candidate vector;
- CPU/state documentation keeps the `$B081-$B32E` candidate scan outside the
  existing movement/state evidence boundary.
