# R3 Season Data Foundation

## Scope and non-goals

This Luna lineage implements the strongest bounded native-C foundation supported
by the accepted Rev1 evidence. It adds canonical TTDT player identity, a
caller-owned metric/ranking seam, immutable TSNS schedule/progression queries,
an explicit native standings policy, transactional TSAV-1 session mutations,
runtime state coherence checks, and owned CHR boundary hardening.

It does not implement downstream gameplay statistics, persistent season
accumulators, leader-result population, progression UI, team management,
All-Star behavior, frontend/audio work, gameplay/scene/LIVE/TIP work, original
SRAM compatibility, or an original save checksum/slot/schema claim. TSAV-1 is
the port's native save contract.

## Source evidence and confidence

The evidence basis is the canonical Rev1 ROM, SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

| Area | Exact evidence | Confidence and boundary |
| --- | --- | --- |
| TTDT identity | Bank02 contains 27 real teams x 12 roster slots and 29 selector/profile entries, including West/East. Existing `source_team/source_player` mappings are authoritative for selector resolution. | High for identity/key resolution. The runtime also validates the canonical West/East tables and rejects mutated valid-range mappings. |
| TSNS schedule | Bank03 `$B52F-$BDD4` contains 1107 two-byte records. Lower six bits are team IDs; proven filtered counts are `1107/567/351/1107`, with per-team appearances `82/42/26/82`. | High for the bounded schedule contract. Reserved-bit, range, self-match, count, and per-team coherence failures are closed. |
| Leader catalog/ranking | Bank00 labels are exactly `FIELD GOALS`, `BLOCKED SHOTS`, `REBOUNDS`, `TOTAL POINTS`, `STEALS`, `3 POINT SHOTS`, `FREE THROWS`. `$B0CC-$B17F` proves 27x12 scanning, 18 unique results, primary descending then secondary, and later-scan replacement for exact equal pairs. `$B430-$B4AF` renders rows. | High for the ranking seam. Accumulator storage/lifecycle, gameplay updates, exact per-byte meanings, and original result-commit behavior remain unproven. |

## Implemented contracts

### TTDT

`tecmo_team_data_player_key` defines a team-major canonical key for all 324
real roster identities. `tecmo_team_data_identity_contract_valid` proves all
27x12 keys are unique, requires real selectors to resolve to their own source
keys, and requires both selector-only All-Star tables to match the accepted
canonical mapping. `tecmo_team_data_resolve_player_identity` fails closed on
invalid, unavailable, or mutated assets while retaining selector identity.

`tecmo_team_data_stat_category_name` exposes the seven source-backed labels.
`tecmo_team_data_rank_leaders` accepts exactly one caller-owned candidate for
each canonical key, honors explicit `available` and `eligible` flags, skips
unavailable candidates without mutating them, and returns up to 18 entries.
Ranking is primary descending, secondary descending, then canonical key
descending as the later-scan exact-tie rule. No persistent metric storage was
added and the existing leader display remains explicitly unavailable.

`tecmo_team_data_self_test` is reached by the owned asset-pack self-test and
covers the null-comparison regression, full-buffer non-overwrite behavior,
exact ties, 18-result uniqueness, unavailable metrics, candidate
nonmutation, invalid inputs, and mutated All-Star mappings.

### TSNS schedule, progression, and standings

`tecmo_season_game_count`, `tecmo_season_schedule_record`, and
`tecmo_season_next_progress` expose logical ordinal, raw index, teams,
total/completed/remaining counts, and an explicit complete state. Queries and
prepare are side-effect free; `tecmo_season_commit_game_result` advances one
validated pending record only after persistence succeeds.

`tecmo_season_build_standings_rows` provides renderer-ready rows. The native
policy is exact winning-ratio comparison by 64-bit cross multiplication (no
display truncation), then wins, then source division/display order. Games
behind are stored in half-game units. The original tie policy is not claimed.

### Transaction and state hardening

Team-control changes, season-type reset, programmed-editor changes, and result
commit now persist a candidate TSAV-1 session and install it only on success.
On failure, live season gameplay fields and pending state remain unchanged;
the live session retains an I/O diagnostic. Runtime validation occurs before
frame increment and requires a coherent schedule/session/division contract.
When a result is pending, its index and away/home teams must resolve to the
current `TecmoSeasonScheduleRecord`, and the launch gate remains closed until
the commit succeeds. The existing 13-pixel game-start image is retained while
the diagnostic state correctly reports `launch-blocked=1`.

TSNS CHR availability now revalidates every imported cell/cursor range after
asset mutation using overflow-safe arithmetic. The owned importer rejects CHR
range arithmetic overflow and has exact boundary tests.

## Luna lineage and revisions

- Worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r3-season-data-foundation-luna`
- Branch: `codex/r3-season-data-foundation-luna`
- Expected parent/base: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- Implementation commit: `75119657dda1db7d97083dafddc4498548ac7ab3` (`Implement R3 season data foundation contracts`)
- Review corrections included: removed the unsafe null ranking comparison;
  bounded full-buffer insertion now skips a non-winning candidate; canonical
  All-Star tables are shared by parser and runtime validation; schedule
  pending-record validation is exact; persistence failures roll back; and the
  prepared result remains launch-blocked until commit.

## Reproducible proof manifest

All commands below were run from the exact worktree above. The build is the
repository's existing warning-clean native build; no build/CMake translation
unit was added.

```text
.\build.ps1
  exit 0; Built build\tecmo_port.exe and build\tecmo_port_game.exe; no warnings

.\build\tecmo_port.exe --assetpack-test
  Asset pack self-test passed.

.\build\tecmo_port.exe --season-test
  Season management self-test passed.

.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -SkipBuild
  TEAM DATA TEST PASS ... 15 pixel checkpoints

.\tools\Run-SeasonTests.ps1 -ProjectRoot . -SkipBuild
  SEASON TEST PASS ... 13 pixel checkpoints
```

The self-tests additionally prove all four schedule counts and completion
sentinels, zero-game standings, exact-ratio/wins ordering, odd half-game
distance, strict TSAV parsing, invalid pending no-mutation, team-control/reset/
editor/result rollback, and importer CHR boundaries. `git diff --check` was
clean before the implementation commit.

## Visual observations

The existing 15 TTDT and 13 TSNS pixel checkpoint hashes remain accepted by
their scripts. No ROM capture, save-state, trace dump, or proprietary decoded
payload was added. The leaders results view continues to show
`PLAYER RESULTS UNAVAILABLE`; the new ranking seam is not wired to that UI.

## Honest approximations and deferred gaps

- The metric API deliberately consumes ephemeral caller-provided pairs. It
  does not infer accumulators, event updates, category byte meanings, or
  standings from static TTDT ratings.
- Native standings are an explicit documented policy, not a reconstruction of
  unproven original tie parity.
- TSAV-1 remains a native port save and is not original SRAM compatibility.
- Game launch remains a guarded boundary pending a validated native result;
  gameplay, management, progression presentation, and All-Star integration
  remain downstream work.
- The importer work is limited to owned CHR/range hardening and tests; the
  excluded pack-reader/source-map/game integration surfaces were not changed.

## Merge instruction

The primary implementation unit is commit
`75119657dda1db7d97083dafddc4498548ac7ab3`; Sol can cherry-pick that
commit onto the expected parent/lineage. The documentation follow-up commit is
reported separately in the final handoff so the exact repository tip can be
verified without claiming that a commit can contain its own SHA-256 identity.
