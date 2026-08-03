# Evidence and classification

## Canonical evidence

The read-only Rev1 ROM is:

`C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`

- Length: `393232` bytes
- SHA-256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

Relevant source evidence:

- Bank03 `$8FC2-$9102`: 321 bytes, SHA-256 `FA3B396D01581451717CEB44A0F5628560FC664191E8F15F5843B0EAB316A9F5`; five unique selected values `0..11` staged into `$6023 + 5*team`.
- Bank04 `$AC76-$AC8B`: calls `$ADE0` for team IDs `$0765/$0766`, destination offsets `0/5`.
- Bank04 `$ADE0-$ADF3`: 20 bytes, SHA-256 `B4CC98CF95216620E6DAAB21C71BC1D9A679AFE9BB8BE5DC455A239E07640A3B`; stages `$6023+5*team` into `$7B2E-$7B37`.
- Bank04 `$AC76-$ADDF`: 362 bytes, SHA-256 `E123614333986D9D5084678C9AE32DD3A1A28ABF52F6D6265FE749FC0070C6E0`.
- Bank06 `$938B-$9620`: 46 source-pinned starts out of 48 theoretical entries; indices `depth_row * 12 + x_bucket`; `46/47` reject.
- Accepted payloads remain unchanged: TGAI `7616` bytes/FNV-1a32 `D6C4DB35`; TGMO `1664` bytes/FNV-1a32 `6C82A137`.

The exact Bank04 actor-slot values used by bound LIVE are:

| Slot | Position | Direction | Fixed link |
|---:|---|---:|---:|
| 0 | `(528,144)` | 1 | 5 |
| 1 | `(448,144)` | 1 | 6 |
| 2 | `(362,112)` | 2 | 7 |
| 3 | `(364,192)` | 5 | 8 |
| 4 | `(392,144)` | 1 | 9 |
| 5 | `(176,144)` | 0 | 0 |
| 6 | `(320,144)` | 0 | 1 |
| 7 | `(408,112)` | 2 | 2 |
| 8 | `(400,192)` | 5 | 3 |
| 9 | `(372,144)` | 0 | 4 |

Static seeds are primary actor `4`, defender actor `9`, matchup seed family `{2,7}`. They remain separate from the first post-handoff holder-driven synchronization and from inferred native matchup metadata.

## Classification table

| Area | Classification | Boundary |
|---|---|---|
| `$6023 -> $7B2E` starter staging | Exact | Source-backed byte staging. |
| Session-to-launch roster propagation | Native-faithful policy | By-value selected TTDT starters; one-for-one staged-entry-to-stable-slot mapping is inferred. |
| Bound actor positions/directions/fixed links | Exact static table data; native-faithful/inferred LIVE reuse | Stable native scene topology policy; the Bank04 values are exact, but post-tip reuse is not a proven first-running-clock snapshot and fixed links are not dynamic matchup assignment. |
| Static seeds `{4,9}`, `{2,7}` | Exact source-backed seeds | Dynamic role synchronization is separate. |
| Formation selector | Source-pinned integration | Actual selector is called; 46/47 reject. |
| Formation/play-state/one-step lifecycle | Accepted API integration | One immutable ten-actor snapshot, source actor order, `step_budget=1`, transactional commit. |
| Source actor target | Native-faithful adapter | A valid stored coordinate is required; movement follows the current referenced actor on every immutable post-human snapshot/tick. Original Bank05 dynamic retarget/matchup semantics remain incomplete and unproven. `(0,0)` is a valid TGCT coordinate. |
| Source direction to TGMO | Native-faithful adapter | Synthesized targets stay inside the playable court polygon. Direct direction is composed when exposed; otherwise tested target-to-direction equivalence is used. An outward edge/corner direction with no legal target is preserved as metadata and applied inert/deferred, without an out-of-court target or scene failure. |
| Controller/matchup routing | Native-faithful/inferred | Controller observations are validated; fixed links remain exact and native matchup is explicitly inferred. |
| Shot request workspace/random | Native approximation | Deterministic caller input; not a claim of original RNG or complete caller workspaces. |
| Unsupported `shots.c` playback | Incomplete/deferred | Request and exact actor are recorded with explicit non-launch classification; no shot outcome code was changed. |
| Original first running-clock RAM snapshot | Unproven | Not labelled as the static Bank04 setup. |
