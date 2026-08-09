# Gameplay receiver and defender candidate selection

The production live scene converts the bounded Bank06 candidate pass at
`$B081-$B365`. The fixed per-frame order is pinned by canonical bytes
`20 6A D3 20 39 B1 20 04 B1`: map Bank06, call offensive `$B139`, then call
defensive `$B104`, before an A-button pass/switch consumes either result.

| Original | Native field |
|---|---|
| `$0308/$0309` | `primary_actor` / `defender_actor` |
| `$030A/$030B` | `offense_side` / `defense_side` |
| `$0E/$0F` | `selected_actor_by_side[2]` |
| `$037F/$0380` | `candidate_actor_by_side[2]` |
| `$04B0 & $10` | `actor_selector_flags[10]` |
| `$0463` | scene actor `movement_direction` |
| `$06DA-$06DD` | candidate actor/score result |
| `$06CB-$06D4` | existing `dynamic_link[10]` handoff input |

`tecmo_gameplay_candidate_directional_select()` reproduces `$B183-$B326`:
descending scan, current-actor/polarity/viewport/sign filters, the five exact
tables, byte-ordered shifts, wrapped arithmetic, strict minimum comparison,
and highest-slot tie winner. CPU offense maps `$0463` through exact `$9E48`
bytes `01 02 04 05 06 08 09 0A`. Human neutral input retains the prior
side-indexed candidate because `$B183` returns without a write.

Automatic defense reuses the exact
`tecmo_gameplay_defense_contact_b06_candidate_scan_b081()` weighted ball
metric. Human defense uses the common directional evaluator with defensive
polarity and the selected defender excluded. Pass completion still runs the
existing `$B317` handoff.

Source contract:

- Rev1 ROM SHA256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
- `$B081-$B326` FNV1a32 `11B1E26E`.
- `$B32F-$B365` FNV1a32 `6488E745`.
- `$9E48-$9E4F` FNV1a32 `008DEAE4`.
- Bank05 `$B074-$B0FD` consumer FNV1a32 `D444B867`.
- Fixed-loop nine-byte FNV1a32 `7DDC3A8D`.

Runtime has no ROM/decomp dependency. The optional ROM argument to
`--gameplay-candidate-selection-test` is source-test-only. CPU pass triggering
and the broader opcode-15 `$918A/$91CB` lifecycle remain deferred.
