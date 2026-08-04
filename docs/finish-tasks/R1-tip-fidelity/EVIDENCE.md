# Evidence and classification

## Canonical Rev1 identity

The read-only original is:

`C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`

| Property | Value |
|---|---|
| Length | `393232` bytes |
| SHA-256 | `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` |
| FNV-1a32 | `0650F5B0` |
| Revision confidence | Exact accepted Rev1 identity; the builder rejects every other ROM identity. |

The builder validates every source record against both FNV-1a32 and FNV-1a64
and stores only semantic/native assets. The exact source records below use
Bank05 CPU addresses or fixed-bank addresses as shown; hashes are of the
original source spans.

## TPTI source records and labels

| Record/role | Revision source span | Size | FNV32 | FNV64 | Classification/confidence |
|---|---|---:|---|---|---|
| 17 later/general collision settlement | Bank05 `$98E1-$9A5F` | 383 | `0A2F945A` | `6CB90FF6825E2A5A` | Exact bytes; explicitly not TIP claim. High. |
| 20 orientation ordering | fixed `$E537-$E542` | 12 | `23EB540B` | `F47061919680B38B` | Exact narrowed ordering span; `$E537` high-bit ordering into `$0758`, `$E542` post-store. High. |
| 21 B04 capture/error | Bank04 `$86E1-$8817` | 311 | `F62D7C02` | `3940397C1774CFE2` | Exact clocked capture/error; intentionally overlaps the stored close-up entry. High. |
| 22 shared actor dispatcher | Bank05 `$8351-$839E` | 78 | `49B11D9A` | `A90A6C174BE0F15A` | Exact actor-state `$22->$839F` and shared-state `$1A->$8642` dispatcher evidence. High. |
| 23 automatic actor path | Bank05 `$839F-$8402` | 100 | `39E37AD9` | `7591953DB1443C99` | Exact selected-actor automatic/human branch; strict ball-high threshold. High. |
| 24 opposing dispatcher | Bank05 `$9824-$984F` | 44 | `40A15F8A` | `115114FC51CF97EA` | Exact opposing selected-actor dispatcher. High. |
| 25 opposing actor path | Bank05 `$985E-$988E` | 49 | `1A6CF134` | `3EE3400C4DD54BB4` | Exact 49-byte opposing path, separate from the old 13-byte input subspan. High. |
| 26 actor jump commit | Bank05 `$9C79-$9CC8` | 80 | `76854C2B` | `12DAA5698409484B` | Exact actor commit seam at `$9C7F` and source-pinned `$8D92` helper. High; mapping incomplete. |
| 27 dedicated slot-10 claim | Bank05 `$A214-$A2DE` | 203 | `ED30348D` | `0E50CF92E469E26D` | Exact equality deferral, ball-minus-jumper bound, `$0478=$17` at `$A2D2`, `$A2D5` post-store. High; receiver mapping incomplete. |
| 28 E56E hook anchor | fixed `$E56E` | 1 | `AC0BD104` | `AF64244C860266E4` | One-byte executable hook anchor only. Recurring-loop/handoff labels require mapper-gated dynamic evidence. High for byte, incomplete for semantics. |
| 29 RNG/mix seam | fixed `$CD96-$CDAB` | 22 | `299318D0` | `FB37A5940B9FFA10` | Exact `$6A` mix seam; native downstream bridge is deterministic approximation. High for seam, incomplete for trajectory. |

The original records 1–20 are otherwise preserved, but record 17 is now
labelled later/general settlement and record 20 is narrowed from `$E537-$E56D`
to the exact `$E537-$E542` ordering span. The builder, source map, payload,
and tests use those same spans and hashes.

## Intentional overlap and false friends

- B04 `$86E1-$8817` is duplicated as a new semantic record inside the existing
  close-up entry because the validated capture/error bytes are consumed as
  mechanics metadata while the close-up asset remains its original stored
  family. This is intentional overlap, not an alias accident.
- The existing TIP_INPUT record remains exactly the 13-byte `$985E-$986A`
  subspan. It describes opposing selected-actor current-B sampling/wait/
  countdown bytes. The new record separately stores/validates the full
  `$985E-$988E` 49-byte opposing path. The overlapping `$985B` setup and
  `$985E` role are therefore distinct source roles.
- B05 `$8642` is a shared-state false friend and is rejected as slot-10 TIP
  logic. `$A2D1` is a non-hook operand and is rejected where `$A2D5` is the
  validated post-store seam.
- The static raw selector seeds `$0380=7` and `$037F=2` are opaque diagnostics.
  They do not encode team, orientation, claimant, receiver, or holder.

## Dynamic evidence and confidence labels

| Seam | Exact/native-faithful/native-approximate/incomplete boundary |
|---|---|
| B04 clocked B-level capture/error | Exact source mechanics; native state stores sampled frame and bounded error `0..11`, unsampled sentinel error `12`. |
| `$839F` automatic trigger | Exact control relation: automatic only when `0499 > 3D + (($6A & 1F) >> 2)`; equality is rejected. Native bridge calibrates bounded observed ages 20/21/22. |
| Per-jumper actor commit | Exact commit seam/state distinction (`$0B` actor jump commit, `$17` slot-10/global claim commit); native raw-height/velocity bridge is approximate. |
| `$A274` claim | Exact equality deferral and underflow-safe `< $3A` comparator; complete original tie settlement and raw selector-to-team/receiver mapping are incomplete. |
| No-input | Runtime-observed original no-B stall plus native fail-closed policy; it is not a tie winner. |
| Simultaneous human | Same-frame equal samples remain equal/deferred; a complete original tie policy is incomplete. |
| Both automatic | Accepted bounded dynamic observations at ages 20/22; native approximate raw heights preserve Away compatibility (`0x34` vs `0x33`) without asserting selector ownership. |
| Human-vs-CPU one-down | Accepted bounded observation at age 21; native automatic sample field remains raw frame 21 while its bounded error is 11. |
| Frame-721 LIVE handoff | Accepted native presentation contract, not an exact original timing fact. Possession derives from resolved jumper actor/team, then holder slot 0/5 is preserved. |
| Scene arc and ball X | Scene-owned accepted visual approximation. The exact claim gate is retained; ball X stays centered through capture completion and then moves one pixel/update to a cap of eight. Timing is native-approximate. |

## Latest review gate

Sol accepted the source/ABI/pack/test review for the first TIP implementation
commit `a37e10207455933be3930e90c55b10b669cb0ef3`, and accepted the clean-commit
formal proof. The personal post-rescope gates all exited `0`: `build.ps1` built the
console and Win32 targets with no warning lines;
`Run-Win32LaunchSmokeTest.ps1` passed explicit console flow plus GUI/console
production smoke; `Run-GameplayPreTipTests.ps1` passed; and
`Run-GameplaySceneTests.ps1 -Build` passed with LIVE PROOF DRAFT at
`build/live-proof-20260803T205847090Z`. Independent terminal QA passed the
frozen product/proof at `b678beffeacd745fe438e78d323357dc6f86af95` with
`P0=0`, `P1=0`, and one grouped docs-only `P2`; the same QA task must verify
this revised doc tip before terminal acceptance. Sol-branch integration remains
pending.
