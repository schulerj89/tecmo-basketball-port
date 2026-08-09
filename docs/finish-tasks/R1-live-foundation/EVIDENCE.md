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
- Fixed `$F024-$F058`: 53 bytes, SHA-256 `3C0FE0337190EF2A7A57082BDB3E054CCCB806248DBE2567DB5C307DF8A0AE42`.
  The recurring loop maps Bank05, calls `$81F2/$97AD` and the slot-10 `$A214`
  dispatcher, then maps Bank06 and calls `$B139/$B104`. It does not remap
  Bank04 or replay `$AC8C` after the opening tip.
- Bank05 `$A274-$A2DE`: 107 bytes, SHA-256 `71A6BCE1DD326193B354F7E4D721D6D3DBEC9854C362B2F9BD61C8EBCE910D4`.
  The claim reads the selected receiver's current `$73/$E8/$F3`, launches
  ball state `$17`, and sets `$0588` bit `$20`; it does not rewrite player
  coordinates.
- Bank05 `$86BB-$879A`: 224 bytes, SHA-256 `B36772055E1210601A38891441996A6F8D733DBE928B981DDFC543DB88E578FE`.
  States `$0B-$0E` update the existing jumper object through landing/recovery.
- Bank06 `$827E-$82B2`: 53 bytes, SHA-256 `B68FF871D994DB4E846DAD435FC1D79D2E4374458992C29D62AD8AC32BF2DCCD`.
  The ordinary loop scans slots 9 through 0, skips the selected pair, and
  dispatches each remaining actor through its existing command state.
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
| Tip-to-LIVE actor continuity | Source-backed ownership, native scheduling policy | The handoff preserves every current actor/CPU object and synchronizes live-foundation coordinates in place. Exact first hidden ordinary-actor eligibility remains unproven; native ordinary movement begins on the first subsequent LIVE update, never from a recreated Bank04 layout. |

## Tip-to-LIVE continuity boundary

`scene_initialize_actors()` is now launch-only. The tip handoff transaction
lands the already committed jumpers in place, changes possession and receiver
ownership without applying a generic team-facing rewrite, synchronizes the
live foundation from all ten current coordinates, and settles the camera from
the attached receiver ball. Failure rolls back actors, selection, possession,
orientation, ball, foundation, backcourt, and camera together.

This is exact about the absence of a Bank04 replay and about the bounded
Bank05/Bank06 object ownership above. The precise first under-cinematic update
of every ordinary actor is not established by these spans. The port therefore
keeps the existing presentation freeze as an explicit bounded scheduling
policy, preserves all object history at the boundary, and resumes ordinary
dispatch on the first LIVE update.
