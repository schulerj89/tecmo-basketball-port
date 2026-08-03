# R1 CPU lifecycle implementation

## Public contracts

`include/tecmo_gameplay_cpu_steering.h` retains the old direction/inspection/
harness APIs and adds tagged transactional contracts:

- `TecmoGameplayCpuSteeringEffectKind` and
  `TecmoGameplayCpuSteeringAdvancePolicy` describe all 24 dispatch entries.
  Opcodes 17, 18, and 19 intentionally share
  `EFFECT_AGGREGATION_BARRIER`.
- `TecmoGameplayCpuSteeringAssets` stores validated opcode histogram/metadata,
  fixed links, and decoded formation offsets. It reports 48 theoretical starts
  and exactly 46 source-pinned rows.
- `TecmoGameplayCpuSteeringFormationResult` selects only a pinned formation.
- `TecmoGameplayCpuSteeringRouteInput/Result` models the exact two controller
  slots (`TEAM_COUNT == 2`) and the Bank05 route branch.
- `TecmoGameplayCpuSteeringPlayState/Input/Result` model per-actor 16-bit
  stream offsets, state/wait/target fields, fixed links, separate startup
  seeds, native candidate storage, exact global scratch/aggregation fields,
  bounded opcode chaining, and transactional failure.
- `TecmoGameplayCpuSteeringShotInput/Result` exposes the exact CPU request
  predicate and semantic state transition without a raw `$9217` handoff field.

`play_step` rejects invalid tags, actors, budgets, offsets, positions, fixed
links, seeds, and all cross-object aliases before writing either output. The
caller may not alias state/result with state/input or each other. Existing wait
state decrements to state 4 at zero and does not fetch another record that tick.
Only opcode 1 chains in the same tick; non-goto handlers stop after one bounded
effect/transport decision.

## Native functions and importer functions

### `src/tecmo_gameplay_cpu_steering.c`

- `validate_lifecycle_command_corpus` verifies all 680 aligned opcodes and goto
  destinations.
- `decode_formation_native_fields` decodes semantic offsets during parse; no
  executable handler bytes are interpreted by the runtime.
- `tecmo_gameplay_cpu_steering_assets_parse` fills metadata, links, and
  formations after strict payload/dependency validation.
- `tecmo_gameplay_cpu_steering_decode_command` returns raw record fields plus
  semantic metadata.
- `tecmo_gameplay_cpu_steering_formation_select` and
  `tecmo_gameplay_cpu_steering_route_select` implement the exact bounded
  selectors transactionally.
- `tecmo_gameplay_cpu_steering_play_state_initialize` seeds formation offsets,
  fixed links, state 4, primary 4, defender 9, and separate `{2,7}` matchup
  seeds without pretending they populate `$037F`.
- `tecmo_gameplay_cpu_steering_play_step` performs the bounded native semantic
  executor. Deferred effects retain any independently source-pinned transport;
  they do not become command-argument guesses.
- `tecmo_gameplay_cpu_steering_shot_request` implements only the exact CPU-side
  request predicate and semantic success transition.
- `tecmo_gameplay_cpu_steering_self_test` contains corpus, selector, lifecycle,
  transaction, boundary, deterministic, and shot goldens.

### `src/asset_pack/tecmo_asset_pack_gameplay_cpu_steering.c`

- `validate_lifecycle_anchor` and `validate_lifecycle_anchors` verify the six
  out-of-span SHA-256 ranges and route table bytes directly in the exact ROM.
- The importer keeps the canonical TGAI-1 payload and its FNV identity unchanged;
  anchor bytes are never copied into runtime assets.

## Handler classification and later boundary

| Opcode | Handler/effect category | Advance policy | Native status |
| ---: | --- | --- | --- |
| 0 | relative target | BA-gated none/+5 | exact target arithmetic when `$035A` and `$BA` are supplied |
| 1 | goto | jump | exact offset replacement and bounded same-tick chain |
| 2 | absolute target | BA-gated none/+5 | exact orientation mirror and target fields with gates |
| 3 | wait/countdown | wait/+5 | exact bounded state/wait transition; full caller cadence deferred |
| 4 | actor/object target | +5 | player-slot target supported; canonical C8=`$0A` lookup deferred |
| 5 | direction/pose | +5 | effect deferred; transport exact |
| 6 | transition/reset | none | effect deferred; no advance exact |
| 7 | actor-state compare/branch | branch +5 or current +5 | exact `$046E` eleven-entry probe, CA/CB branch, state 4 |
| 8 | boundary branch | conditional none/+5 | effect deferred; transport gate retained |
| 9 | state/animation | +5 | bounded state/timer/action writes |
| 10 | fixed-link proximity/retry | BA/retry complexity | effect deferred; current contract retains same record |
| 11 | fixed-link relative pose | +5 | effect deferred; transport exact |
| 12 | fixed-link follow-up gate | conditional none/+5 | helper inputs deferred |
| 13 | global scratch target | BA-gated none/+5 | global workspace deferred; transport exact |
| 14 | group reseed | +5 | exact `$04B0 & $10`, `$0023`, state 4, current-actor exit |
| 15 | primary/defender switch | conditional none/+5 | all switch gates/side effects deferred; no invented switch |
| 16 | pointer-actor target | BA-gated none/+5 | RAM-pointer lookup deferred; C8 is not treated as actor argument |
| 17 | aggregation/barrier | +5 | exact shared handler fields/state 11 |
| 18 | aggregation/barrier | +5 | same exact effect metadata/handler; zero corpus |
| 19 | aggregation/barrier | +5 | same exact effect metadata/handler; zero corpus |
| 20 | global target scratch | +5 | effect deferred; source-pinned transport exact |
| 21 | conditional advance | +5/+10 | exact `$058A/$0357/$0358/$7E` gate |
| 22 | global timers/mask | +5 | exact C8/C9 replacement and CA OR retention |
| 23 | alternate direction/pose | +5 | effect deferred; transport exact |

The native `native_matchup_actor[10]` field is explicitly integration/candidate
storage, not a representation of `$037F`. The accepted slice removes the
caller-decided primary-switch seam; opcode 15 remains deferred. Dynamic
candidate scan/filter behavior at `$B081-$B365` is a later CPU policy slice.

## Scene boundary

The production scene still uses the legacy native harness/formation
approximation and existing TGAI-to-TGMO movement surface. It does not fetch the
isolated 680-record stream, consume native formation offsets, or execute this
play state. The lifecycle engine is ready for, but deliberately not wired into,
`R1-LIVE`.
