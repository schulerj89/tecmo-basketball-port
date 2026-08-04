# R3 Player Stats Leaders — Implementation Handoff

This document records the implementation delivered by the signed R3 source
candidate and the bounded proof-fixture correction made during current-main
integration QA. It is a source and behavior handoff, not a claim that the
native port is an emulator-perfect reimplementation of every original ROM
detail.

## Scope

R3 adds the native player-statistics ledger, season save/load support, and
League Leaders presentation needed by the season flow. The supported leader
categories are FIELD GOALS, TOTAL POINTS, 3 POINT SHOTS, and FREE THROWS.
The implementation keeps the port's existing native C and asset-pack
boundaries: the runtime does not read a ROM, execute 6502/ASM, capture an
emulator, or depend on decompilation output. The private Rev1 ROM and the
decompilation tree are QA-only inputs to the local test scripts and are not
tracked or shipped.

## Source modules

| Area | Native implementation |
| --- | --- |
| Player-stat data model | `include/tecmo_player_stats.h` |
| Gameplay stat/result ownership | `include/tecmo_gameplay_scene.h`, `src/tecmo_gameplay_scene.c` |
| Shot and scoring events | `src/tecmo_gameplay_scene_shots.c` |
| Gameplay state-flow fixture | `src/tecmo_gameplay_scene_test_state_flow.c` |
| Season persistence and leaders | `include/tecmo_season_menu.h`, `src/tecmo_season_menu.c` |
| Deterministic menu fixture | `src/tecmo_cli_render_menu_modes.c` |
| Season QA orchestration | `tools/Run-SeasonTests.ps1` |

The source candidate also updated the R3 README, evidence, and proof records.
The complete source and QA path ledger is in [LINEAGE.md](LINEAGE.md); the
integration test inventory is in [TESTS.md](TESTS.md).

## Persistence and presentation behavior

- TSAV-2 is the native current save representation. The strict parser accepts
  the supported TSAV-1 migration path and TSAV-2 records, while rejecting
  wrong versions, wrong sizes, malformed headers, reserved-field violations,
  checksum failures, truncation, and trailing data.
- Save replacement is transactional. A malformed replacement cannot partially
  overwrite valid in-memory season state; rollback and new-save precedence are
  covered by the Season suite.
- Leader pages use deterministic native data and rendering. Category 0 uses
  page offsets for continuation pages 0, 6, and 12; the other inspected pages
  are categories 3, 5, and 6. Generated frames are native QA evidence, not
  promoted original-reference parity proof.
- The CLI menu fixture is proof-only and does not become a runtime ROM or SRAM
  dependency.

## Proof-fixture correction

The independent visual/source review found that the former proof-only seed used
FGA 400, 3PA 200, and FTA 120 while makes increased with the canonical key.
For keys through 323 this could display impossible percentages above 1.000.
The bounded correction changed only the three proof attempts in
`seed_populated_leader_results` to FGA 800, 3PA 500, and FTA 500. The make
vectors, formulas, sorting/ranking behavior, runtime code paths, and gameplay
semantics were unchanged. The resulting maximum seeded makes (623, 373, and
413) are below their corresponding attempts.

Five of the six populated render hashes changed; category 3 TOTAL POINTS
remained byte-identical because it is derived from makes rather than attempts.
The corrected expected hashes and current/repeat capture records are in
`PROOF.md`, [TESTS.md](TESTS.md), and the R3B evidence report.

## Explicit behavior boundary

The delivered implementation intentionally distinguishes supported data from
unavailable data. The four supported categories have complete native coverage
in the tested path. STEALS, BLOCKS, REBOUNDS, and incomplete categories remain
unavailable/zero rather than being populated with invented values. TSAV-2 is a
native port format and is not represented as original NES SRAM.

The source and QA records classify behavior as exact-source-pinned where the
repository has an explicit source/data anchor, native-faithful where the
native implementation preserves the observed state and presentation contract,
native-approximate where the port supplies a documented native equivalent,
and incomplete where source coverage is not claimed. This makes remaining
user-visible differences explicit instead of treating a native fixture as
emulator parity.

## Handoff status

The implementation was reconciled into current main at merge `91f1584`. The
proof-fixture correction is signed in checkpoint `20dcf9a`, and the final
documentation closure is its signed descendant. The correction and QA closure
do not rewrite the merge or alter main.
