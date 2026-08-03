# Evidence and confidence

## Canonical local-private source

The source gates require the exact Rev1 iNES layout: 16-byte header, 8 PRG
banks of 0x4000 bytes, and 32 CHR banks of 0x2000 bytes. The canonical local
private ROM SHA-256 is
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The ROM is test evidence only and is never loaded by normal runtime code or
committed to this branch.

## Serialized semantic contracts

| Contract | Serialized size | Fingerprint | Semantic count | Confidence |
| --- | ---: | --- | --- | --- |
| TMUS-1 music | 36784 | `05C00ECB` | 2251 instructions / 37 voices | high; importer postcondition and parser gate |
| TSFX-1 gameplay SFX | 2824 | `968A5DE6` | 131 instructions / 14 voices | high; isolated gameplay importer gate |
| TDMC-1 gameplay DMC | 2515 | `AD70E6E8` | 5 clips / 3 pools | high for serialized bounds and pool data |
| TFSX-1 frontend SFX | 1792 | `985DC7ED` | 87 instructions / 3 voices | high; importer postcondition and parser gate |

The music cadence is exactly 44,100 Hz with
`39375000 / 655171` tick accumulation. The semantic pack also records the
known native PCM/state anchors used by the domain tests:
TMUS PCM `105B1338`, state `1C74513C`, TSFX/TDMC PCM `83E60072`, state
`17208C83`, title PCM `09718C9D`, and menu PCM `100B5218`.

## ROM source ranges

Addresses below are CPU addresses in the specified bank/window. The importer
checks the declared PRG end as well as total ROM size before using them.

| Asset | Bank/window and exact evidence |
| --- | --- |
| TMUS-1 | Bank04 `$8AA4` source/directory contract; track spans `$8CE2-$9F06` with IDs 5 `$92F4-$96C3` (`1270498B`), 6 `$96C3-$9D8B` (`BD91FCF1`), 7 `$8CE2-$92F4` (`69F85EC2`), and 8 `$9E13-$9F06` (`8122C6CF`). Fixed-bank engine `$F2F2`, pitch table `$F93B`; Bank04 queue `$826A`, first route `$82CF`; fixed menu queue `$E477`; Bank06 pregame matchup queue `$A145` (5 bytes, `1E564AC0`). |
| TSFX-1 | Bank04 directory `$8AA4`/32 bytes (`6283F255`), core `$8AA4`/556 (`548EED95`), extension `$9D8B`/136 (`838408D4`); effects: ID 3 `$9DF7-$9E13` (`B7138F94`), 5 `$8C5D-$8CA3` (`28EE1024`), 6 `$9D8B-$9DF7` (`34460805`), 11 `$8AC4-$8AF6` (`93E7AC2C`), 12 `$8AF6-$8B35` (`B172920D`), 13 `$8B35-$8B6E` (`DC401221`), 14 `$8B6E-$8B97` (`E3035B54`). |
| TDMC-1 | Fixed-bank DMC pools `$C080`/513 (`33E109D7`), `$C440`/721 (`6ECC107C`), `$C740`/945 (`F621FD7C`); fixed event/trigger evidence includes `$E7DB`, `$E863`, `$E86D`, and Bank05 `$A8D6`, `$A9C5`, `$ABF5`, `$B5AB`. Accepted serialized TDMC clips require pool offset zero; runtime subtraction-form checks remain defense in depth, not a currently reachable mutated-offset exploit. |
| TFSX-1 | Bank04 effects ID 8 `$8BF7-$8C2A` (`AC9D4C1F`) and ID 10 `$8B97-$8BF7` (`963DC35E`); Bank03 title setup `$8056`/32 (`4A97C61D`) and confirm `$8076`/27 (`0C902C97`); fixed bridge/dispatch ranges are fingerprinted by the frontend importer. |

The gameplay importer additionally fingerprints the fixed directory/engine and
Bank05 cue ranges used by the isolated source gate. A mutation in a bounded
TSFX/TDMC-owned span therefore reports a gameplay revision/source failure
before the final full-ROM SHA check. The Bank06 `$A145` music-owned mutation is
kept in broad asset-pack integration coverage and intentionally excluded from
the gameplay-only importer gate.

## Evidence limits

The evidence establishes deterministic semantic extraction and native-player
state behavior. It does not establish a nonlinear, cycle-exact NES APU mixer,
exact DMC reader bit/cycle phase, or original game-wide cue routing. Those
limits are part of the contract.

## Fidelity classification

These classifications describe the evidence and implementation boundary; they
do not replace the pending Sol acceptance decision.

| Criterion | Classification | Boundary |
| --- | --- | --- |
| TMUS-1, TSFX-1, TDMC-1, and TFSX-1 semantic extraction | R4 isolated foundation: exact/high-confidence | Exact serialized sizes, FNV fingerprints, counts, parser postconditions, and Rev1 source gates are enforced. |
| 44.1 kHz native-player cadence/state transitions | R4 isolated foundation: exact/high-confidence within native contract | The checked `39375000/655171` accumulator, queues, termination, mailbox, override, held-DAC, and retry semantics are deterministic and tested. |
| Output transaction/fallback behavior | R4 isolated foundation: exact/high-confidence | The portable seam covers accepted/rejected initial/refill transactions, valid borrowed aliases, invalidation detachment, and frozen fallback; real device failures are not simulated by a hardware driver. |
| Same-pack music/gameplay/frontend selection | R4 isolated foundation: exact/high-confidence | Canonical aliases accept; byte-identical distinct containers reject and preserve prior selection. |
| Bounded malformed-source and arithmetic rejection | R4 isolated foundation: exact/high-confidence | Checked public offsets, minimum bank counts, declared-PRG bank ranges, isolated gameplay mutation diagnostics, and direct-builder tests are covered. |
| NES APU nonlinear/cycle-exact mixing | Approximation/deferred | Native C synthesis is deterministic but is not claimed to reproduce nonlinear or cycle-exact NES APU mixing. |
| DMC reader bit/cycle phase | Approximation/deferred | Rate, byte, DAC, retrigger, end, and stop continuity are modeled; original reader phase/IRQ/cycle behavior is not claimed. |
| DMC clip identity | Bounded/partial | IDs 0/1/2 remain address-bound and unresolved; ABF5 has sequence-level correlation only, with no impact/rim/exclusivity claim. |
| Gameplay effect 5 | Neutral/unresolved | No stronger audible semantic claim is made. |
| Gameplay effect 6 | Bounded-correlation only | The observed bounded correlation is retained without overclaiming cue identity. |
| Cross-domain cue routing and full game integration | Incomplete/deferred | Excluded call sites and shared integration boundaries remain outside R4 ownership; broader ACC-AUDIO is not complete. |

The complete audible approximation/deferred-difference list, source/waveform
notes, and listening disposition slots are kept in [PROOF.md](PROOF.md).
