# Evidence and source reconciliation

## Research-auditor registry metadata

The parent-provided registry facts for all three completed auditors are
recorded below.  All three remain pinned.  Each auditor created zero
replacement/retry/pin-fault events, and each performed zero repository
mutation.  Findings are concise
reconciliations against the authorized implementation and source references;
they are not copied proprietary payloads.

| Auditor | exact thread ID | exact title | `created_at` | pinned | creation/pin/retry/replacement faults | classification | reconciled finding and source reference |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Original evidence | `019fcbd0-963e-7022-aea4-fb450b4801c2` | `Tecmo R3 Player Stats Original Evidence — Luna Max` | `2026-08-04T08:07:52.000Z` | yes | `0/0/0/0` | `exact-source-pinned` | Rev1 evidence is source-pinned at fixed C042→CC12/CC1E/CC27 nine byte-wide 2x12 planes with modulo-256 increments; Bank05 counter call sites plus Bank00 counter tables/order/labels; Bank02 B41E-B483 and fixed E618-E667 16-bit rollup/wrap; Bank00 B17F-B2E8 and Bank06 formulas/thresholds; B0D1-B131 27x12/top18/later-scan ties; and the page/format/template-ID tables plus B430-B487 row boundary. No repository mutation. |
| Architecture ownership | `019fcbd0-fcd1-7d51-b93c-9e8688440b20` | `Tecmo R3 Player Stats Architecture Ownership — Luna Max` | `2026-08-04T08:08:18.000Z` | yes | `0/0/0/0` | `native-faithful` | The authorized gameplay ledger/result seams and season commit/save boundary are isolated in the granted files; prohibited team-data/rules/assets ownership remains unchanged, as reflected by `include/tecmo_player_stats.h:85-211` and `src/tecmo_game.c:1030-1087`; no repository mutation. |
| Proof save schema | `019fcbd1-62ae-7891-a16e-b20c6b3b4342` | `Tecmo R3 Player Stats Proof Save Schema — Luna Max` | `2026-08-04T08:08:44.000Z` | yes | `0/0/0/0` | `native-faithful` | The port-native TSAV-2 layout is the exact accepted persistence contract/native-faithful extension preserving the TSAV-1 84-byte prefix; explicit v1/v2 dispatch, checksum/reserved validation, migration, and rollback are covered by `src/tecmo_season_menu.c:993-1080` and `tools/Run-SeasonTests.ps1:649-688`. It is not original SRAM compatibility or an original-source-pinned byte layout. No repository mutation. |

The technical reconciliation below is limited to the accepted task contract,
the authorized source files, and the passing local proof commands; it is not
represented as an auditor quote or proprietary payload.

## Reconciled classifications

| Area | Classification | Reconciled finding and boundary |
| --- | --- | --- |
| Rev1 counter planes/order, rollup/wrap, formulas/thresholds, 27x12/top18/later-key ranking, page/format/template-ID tables, and B430-B487 row boundary | `exact-source-pinned` | These are the reconciled original evidence locations listed by the first auditor; the implementation follows the accepted source-pinned values and tests them directly. |
| TSAV-2 sizes/offsets, explicit v1/v2 dispatch, and native persistence migration | `native-faithful` | This is the exact accepted port-native persistence contract preserving the TSAV-1 prefix. It is explicitly not original SRAM compatibility. |
| Identity-bearing gameplay observation/copy/merge seams, canonical away/home mapping, and atomic candidate installation | `native-faithful` | Existing validated gameplay/season state boundaries are retained; the stats seam observes only validated state and rolls back on failure. |
| Shot/free-throw outcome policies and their current validated scoring timing | `native-approximate` | The port observes the existing validated shot/free-throw outcomes transactionally; it does not claim the original unexposed outcome implementation. |
| Supported leader templates, page offsets, unavailable boundary, metric formatting, fixed dynamic-text role, and deterministic populated render fixtures | `native-faithful` | Imported season assets and native row/page boundaries are retained; the CLI fixture is proof-only. |
| Four supported categories with complete coverage | `native-faithful` | FIELD GOALS, TOTAL POINTS, 3 POINT SHOTS, and FREE THROWS populate through the production leader path. |
| STEALS, BLOCKED SHOTS, REBOUNDS, incomplete migrated seasons, original SRAM import, and counters 6-8 gameplay emission | `incomplete` | Pure formulas exist for all seven categories, but the accepted product boundary keeps these render/event paths unavailable or zero; original SRAM import is not implemented. |
| CLI populated-results fixture | `native-approximate` | It seeds deterministic proof data only; it is not production stat seeding or a claim about original season data. |

## Exact authorized source locations

The references below are the routines, tables, and constants relied on by the
implementation.  Line numbers identify the proof-capture source locations;
routine names are the durable anchors.

### Shared contract and gameplay

- `include/tecmo_player_stats.h:16-66` defines the checked dimensions, exact
  `TecmoPlayerStatCounter`, `TecmoPlayerGameStats`,
  `TecmoPlayerSeasonStats`, and separate `TecmoPlayerStatsGameLedger`.
- `include/tecmo_player_stats.h:85-166` contains clear/initialize, checked
  counter-add, shot/free-throw emitters, and exact-coverage ledger validation.
- `include/tecmo_player_stats.h:179-211` contains canonical away/home merge,
  uint8-to-uint16 accumulation, and coverage intersection.
- `include/tecmo_gameplay_scene.h:96-105` defines the result ledger carried by
  `TecmoGameplaySceneResult`; `:190-239` defines the live scene ledger and
  result state.
- `src/tecmo_gameplay_scene.c:955-1103` is the launch lifecycle.  It clears
  the placeholder result and initializes both the live and placeholder ledgers
  to valid empty coverage at successful launch.
- `src/tecmo_gameplay_scene.c:1487-1501` validates GAME_COMPLETE and copies the
  finalized live ledger into the result exactly once.
- `src/tecmo_gameplay_scene.c:1589-1670` is the transactional free-throw
  result/settlement seam.  The function-entry scene snapshot restores CPU
  frame/action/stat/state changes on every post-mutation failure.
- `src/tecmo_gameplay_scene.c:2152-2227` is the outer update seam.  It
  snapshots/restores `TecmoGameplayState` and `TecmoGameplayEventBuffer` when
  the pre-action free-throw phase fails after the phase-frame increment.
- `src/tecmo_gameplay_scene_shots.c:26-57` normalizes validated non-three
  field goals to the two-point stats path and classifies only validated value
  3 as a three-pointer.
- `src/tecmo_gameplay_scene_shots.c:2483-2507` is the copy-on-candidate shot
  update boundary, preserving full rollback for failed launch/settlement.
- `src/tecmo_gameplay_scene_test_state_flow.c` contains the actual 2/3-point
  launch/make/miss, away/home/CPU free-throw, invalid-coverage rollback,
  wrap, result-copy, and unsupported-counter regression seams.  The real
  steal-policy test explicitly checks counters 6-8 remain zero.

### Season, persistence, projection, and rendering

- `src/tecmo_season_menu.c:51-69` pins TSAV-2 header/payload/total sizes and
  compile-time asserts 5,942 bytes total.
- `src/tecmo_season_menu.c:140-166` validates the imported leader screen IDs
  `[38,39,40,41,42,43,44]` against category template IDs
  `[39,40,41,42,43,39,44]`; the resolver is the validated `template_id - 38`
  relationship, not the selection-screen ID.
- `src/tecmo_season_menu.c:448-490` validates the imported three-line leader
  row geometry: player x=32/team x=184 at y=32+32*row, labels at
  y=40+32*row, and projected numeric values at y=48+32*row.
- `src/tecmo_season_menu.c:194-325` contains the source-pinned signed-
  magnitude ratio, seven-category pure projection switch, thresholds, and
  composite point wrap.
- `src/tecmo_season_menu.c:993-1080` contains the explicit `parse_save_v1`
  and `parse_save_v2` payload parsers and the strict exact-size, header,
  checksum, reserved-byte, and version dispatcher.
- `src/tecmo_season_menu.c:1187-1274` is the v2-only atomic saver; the
  forward declaration at `:967` precedes the first
  `season_session_fields_valid` call.
- `src/tecmo_season_menu.c:1394-1445` checks the first projected pair for
  page navigation using each candidate team's actual `wins+losses` games.
- `src/tecmo_season_menu.c:1592-1614` validates season coverage/totals without
  healing incomplete coverage.
- `src/tecmo_season_menu.c:1803-1866` is the candidate-based game commit,
  including result contract validation, canonical team mapping, W/L/schedule
  update, stats merge, persistence, and install-on-success.
- `src/tecmo_season_menu.c:2471-2512` builds all 324 candidates with per-team
  games and delegates ranking to the unchanged `tecmo_team_data_rank_leaders`
  implementation.  `src/tecmo_season_menu.c:2533-2543` formats 1000/100
  scales.
- `src/tecmo_season_menu.c:2545-2642` retains the category template and
  unavailable boundary, draws the native row geometry, and uses the fixed
  dynamic-text palette role 0 rather than blank template-cell attributes.
- `src/tecmo_game.c:1030-1087` copies one validated gameplay result into one
  season result and invokes the existing commit boundary; no fallback or
  coverage healing is performed there.
- `src/tecmo_cli_render_menu_modes.c:15` seeds the proof-only populated
  leader fixture; `:772-820` configures its selectable categories/pages.
  The CLI fixture does not seed production state.
- `tools/Run-SeasonTests.ps1:60-79` records the 19 render checkpoints;
  `:649-688` records the strict v1/v2 rejection and cross-version physical
  size matrix.

## No-payload note

This report records source locations, contract constants, classifications, and
test outcomes only.  It intentionally omits raw ROM/CHR/asset payload bytes,
proprietary extracted tables, and unrelated team-data implementation details.
