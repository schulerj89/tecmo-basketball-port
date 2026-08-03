# R3 Season Data Foundation

## Scope and non-goals

This Luna lineage implements the strongest bounded native-C foundation supported
by the accepted Rev1 evidence. It adds canonical TTDT player identity, a
caller-owned metric/ranking seam, immutable TSNS schedule/progression queries,
an explicit native standings policy, transactional TSAV-1 session mutations
with proven per-mode record bounds, runtime state coherence checks, and owned
CHR boundary hardening.

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
| TSNS schedule | Bank03 `$B52F-$BDD4` contains 1107 two-byte records. Lower six bits are team IDs; proven filtered counts are `1107/567/351/1107`, with per-team appearances `82/42/26/82`. | High for the bounded schedule contract. Reserved-bit, range, self-match, count, per-team coherence, and mode-specific per-team record bounds are closed. |
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
behind are stored in unsigned half-game units; if an arbitrary programmed
record would produce a negative distance, the exported value floors at zero.
The self-test explicitly exercises a `1-0` leader ahead of a `10-1` row: the
exact-ratio order is preserved and the negative raw distance exports as zero.
The original tie policy is not claimed.

### Transaction and state hardening

Team-control changes, season-type reset, programmed-editor changes, and result
commit now persist a candidate TSAV-1 session and install it only on success.
On failure, live season gameplay fields and pending state remain unchanged;
the live session retains an I/O diagnostic. Runtime validation occurs before
frame increment and requires a coherent schedule/session/division contract.
The native TSAV/runtime contract shares the proven per-team W+L targets
`82/42/26/82` for regular/reduced/short/programmed modes. Parse, save,
session validation, result commit, and editor mutation all enforce the active
mode target. A successful type reset also clears stale completion, result
history, pending identity, and game-boundary flags.

When a result is pending, its index and away/home teams must resolve to the
current `TecmoSeasonScheduleRecord`. `game_launch_blocked` is an in-flight,
pending-result, and relaunch lock—not a block on the initial launch:
preparation emits one `LAUNCH_GAME` action while the flag is true, no session
progression occurs until a validated result is persisted, and commit then
clears the flag. The existing 13-pixel game-start image is retained while the
diagnostic state correctly reports `launch-blocked=1`.

TSNS CHR availability now revalidates every imported cell/cursor range after
asset mutation using overflow-safe arithmetic. The owned importer rejects CHR
range arithmetic overflow and has exact boundary tests.

## Luna lineage and revisions

- Worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r3-season-data-foundation-luna`
- Branch: `codex/r3-season-data-foundation-luna`
- Expected parent/base: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- ROM/ASM research Luna: `019fc824-fea9-70b2-a0d0-c0621b217fa5`,
  `gpt-5.6-luna/max`, read-only/projectless, completed
  `2026-08-03T15:18:41Z`, Sol accepted, unpinned
  `2026-08-03T15:22:38.348Z`.
- Native audit Luna: `019fc825-586c-7250-9e8f-805f2bf8860a`,
  `gpt-5.6-luna/max`, read-only/projectless, completed
  `2026-08-03T15:17:07Z`, Sol accepted, unpinned
  `2026-08-03T15:22:38.348Z`.
- Writable implementation Luna: `019fc836-d254-78a0-b3a6-c4003278c2a5`,
  `gpt-5.6-luna/max`, created `2026-08-03T15:21:03Z`; the branch, worktree,
  and base are recorded above and this Luna remains pinned until final
  integration.
- Independent QA Luna: `019fc869-9412-7d72-8534-f81c1d63275b`,
  `gpt-5.6-luna/max`, projectless/read-only, created `2026-08-03T16:16:30Z`,
  completed `2026-08-03T16:37:54Z`, ACCEPT with no candidate-relevant
  findings; it remains pinned until the final report/unpin.
- Implementation commit: `75119657dda1db7d97083dafddc4498548ac7ab3` (`Implement R3 season data foundation contracts`)
- Review corrections included: removed the unsafe null ranking comparison;
  bounded full-buffer insertion now skips a non-winning candidate; canonical
  All-Star tables are shared by parser and runtime validation; schedule
  pending-record validation is exact; persistence failures roll back; and the
  prepared result remains launch-blocked until commit.
- Revision commit: `0c2bf410f8d86d3a5bbb3af75d699be86de8a780` (`Harden season targets and reset lifecycle`): mode-specific TSAV/runtime W+L targets, completed-session reset coherence, boundary regressions, and the unsigned games-behind floor contract.
- Final accepted code candidate: `15946f584e7a69836a3767059123c7b13593fc2a`;
  a focused synthetic `1-0` versus `10-1` standings regression proves the
  negative games-behind expression is floored before export.
- Exact candidate lineage before this documentation commit, in order:
  `75119657dda1db7d97083dafddc4498548ac7ab3`,
  `3431112e1ddcc66cf771106818f31bd1b5a5e4e6`,
  `0c2bf410f8d86d3a5bbb3af75d699be86de8a780`,
  `d282bc21e8fd1e766ede0e677c05a8721ae7a47d`,
  `15946f584e7a69836a3767059123c7b13593fc2a`.
- The exact SHA of this final documentation-only commit is supplied in the
  Sol handoff because a commit cannot contain its own SHA.

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

.\tools\Run-TeamDataTests.ps1 -ProjectRoot .
  TEAM DATA TEST PASS ... 15 pixel checkpoints

.\tools\Run-SeasonTests.ps1 -ProjectRoot .
  SEASON TEST PASS ... 13 pixel checkpoints
```

The self-tests additionally prove all four schedule counts and completion
sentinels, zero-game standings, exact-ratio/wins ordering, odd half-game
distance, the explicit negative `1-0` versus `10-1` zero-floor case, strict
TSAV parsing, exact and over-limit `82/42/26/82` mode boundaries,
completed-session reset recovery, invalid pending no-mutation,
team-control/reset/editor/result rollback, and importer CHR boundaries.
`git diff --check` was clean before the evidence-correction commit.

## Sol personal QA and independent acceptance

Sol personally reverified the canonical ROM SHA-256 as
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The warning-clean `.\build.ps1`, `.\build\tecmo_port.exe --assetpack-test`,
and `.\build\tecmo_port.exe --season-test` passed. The exact wrapper runs
were:

```text
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -SkipBuild
  PASS: 15/15 pixel checkpoints

.\tools\Run-SeasonTests.ps1 -ProjectRoot . -SkipBuild
  PASS: 13/13 pixel checkpoints, including native gameplay handoff/result
```

Sol also passed `git diff --check`, exact base ancestry, allowed-path scope,
and clean status. The reproducible ignored proof directory is
`build/sol-proof-15946f5`; private packs and captures were test-only and were
not committed.

Independent QA repeated diff/build/self-tests/wrappers, the exact five-commit
candidate lineage, source claims, completed reset, `82/42/26/82` bounds,
negative games-behind floor, pending coherence, identity/ranking/CHR, and all
four rollback paths. It accepted the candidate with no candidate-relevant
findings; intentional downstream gaps remain as documented above.

## Visual observations

The existing 15 TTDT and 13 TSNS pixel checkpoint hashes remain accepted by
their scripts. No ROM capture, save-state, trace dump, or proprietary decoded
payload was added. The leaders results view continues to show
`PLAYER RESULTS UNAVAILABLE`; the new ranking seam is not wired to that UI.

Sol's visual proof observations:

| Checkpoint | Frame SHA-256 | Observation |
| --- | --- | --- |
| `team-data-profile` | `E8BA35AC6C2FF05F882CC6D374BC3D4578992A1304D7018A2FE2D21F25F8D575` | Atlanta header/logo/coach/division roster/menu cursor aligned and legible. |
| `team-data-player-detail` | `BC717CC2C62A1BAD485BA6307F8F250198476AFBD816162D6311D4A960635174` | Portrait, identity, attributes, zero-value profile statistics, and six ability meters aligned without clipping or artifacts; static profile fields are not claimed as season accumulators. |
| `season-standings-east` | `3CC04A3C668C9EA7265D7758AA08CADB33BA5E416C1D717B37B9D595050229AB` | Atlantic/Central zero-state rows preserve source order, columns, and readable games-behind markers. |
| `season-leaders-results` | `540D6EA78E8CB646E1D4D960E97EE5A464D04ABD32A6634BCCBD6E75F8CE7764` | Seven-category screen is stable and explicitly shows `PLAYER RESULTS UNAVAILABLE`; no fabricated rows. |
| `season-game-start` | `66458313C7243A8EB3C464495B5B31D1EEAD31BA5D5B8669AD1F9009B0D65649` | TSNS boundary is stable with diagnostic `game-pending=1/launch-blocked=1` and no pre-result save. |

No audio path changed; audio proof is not applicable to this domain.

## Honest approximations and deferred gaps

- The metric API deliberately consumes ephemeral caller-provided pairs. It
  does not infer accumulators, event updates, category byte meanings, or
  standings from static TTDT ratings.
- Native standings are an explicit documented policy, not a reconstruction of
  unproven original tie parity.
- TSAV-1 remains a native port save and is not original SRAM compatibility.
- Game launch emits once at the prepared boundary while
  `game_launch_blocked=1` holds the in-flight/pending-result/relaunch lock;
  gameplay, management, progression presentation, and All-Star integration
  remain downstream work.
- The importer work is limited to owned CHR/range hardening and tests; the
  excluded pack-reader/source-map/game integration surfaces were not changed.

## Merge instruction

The primary implementation unit is commit
`75119657dda1db7d97083dafddc4498548ac7ab3`; the first documentation follow-up
is `3431112e1ddcc66cf771106818f31bd1b5a5e4e6`; and this revision is
`0c2bf410f8d86d3a5bbb3af75d699be86de8a780`. Sol can cherry-pick those commits
in order onto the expected parent/lineage, followed by the final evidence
correction commit `15946f584e7a69836a3767059123c7b13593fc2a`. After this
documentation-only commit is reviewed, Sol should fast-forward the domain
branch from the exact base lineage with:

```text
git switch codex/r3-season-data-foundation-sol
git merge --ff-only codex/r3-season-data-foundation-luna
```

The exact SHA of this documentation-only commit is supplied in the Sol
handoff because a commit cannot contain its own SHA. Master later integrates
the domain branch; no main/origin/main push or merge occurs here.
