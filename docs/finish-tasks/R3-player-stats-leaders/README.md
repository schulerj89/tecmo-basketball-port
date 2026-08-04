# R3 Player Stats and League Leaders

This directory is the task-local implementation and proof record for
`R3-PLAYER-STATS-LEADERS` on `codex/r3-player-stats-leaders-luna`.  It is
durable evidence only; it contains no extracted proprietary ROM payload.

## Current disposition

The proof capture covered the authorized implementation paths plus this
documentation subtree.  No main, staging, or origin ref was mutated and
nothing was pushed.

The three research-auditor registry records are reconciled from the
parent-provided registered facts in [EVIDENCE.md](EVIDENCE.md), including exact
thread IDs, titles, `created_at` values, pin state, retry fault counts, and the
zero-mutation result.  The signed master correction is applied: imported
leader template IDs 39..44 resolve against screen IDs 38..44, using the
validated `template_id - 38` relationship, and the native three-line row
geometry is retained.  The final visual proof was captured under the uniquely
suffixed build directory recorded in [PROOF.md](PROOF.md).

## Contract implemented

- Shared game ledgers use checked `uint8_t[2][12][9]` counters and separate
  `uint16_t[27][12][9]` season totals.  Counter order is FTA, FGA, 3PA, FTM,
  FGM, 3PM, steals, blocks, rebounds.  Coverage is a counter-index mask and
  implemented coverage is exactly bits 0 through 5.
- Gameplay emits only FGA/3PA at a validated shot launch, FGM/conditional
  3PM at one successful exact-once make settlement, and FTA/conditional FTM
  at one successful free-throw settlement.  All event mutations are
  transactional and counters wrap at `uint8_t`; counters 6 through 8 remain
  zero and have no new gameplay emitter.
- A scene ledger is initialized/cleared at lifecycle boundaries.  A valid
  GAME_COMPLETE result carries one finalized ledger, and the runtime copies it
  once into `TecmoSeasonGameResult`.
- Season commit validates the exact ledger contract, maps away/home sides to
  canonical teams, adds all nine deltas modulo `uint16_t` in the same candidate
  as W/L and schedule updates, intersects coverage (`existing &= ledger`),
  and installs only after one successful persistence operation.
- TSAV-2 is exactly 24 bytes of header plus 5,918 bytes of payload (5,942
  bytes total).  The first 84 payload bytes remain the v1 prefix, followed by
  little-endian coverage and 5,832 bytes of little-endian totals in
  team-major/roster-minor/counter-minor order.  v1 dispatch is explicit:
  schedule zero migrates to coverage `0x003f`, in-progress v1 remains coverage
  zero, and later commits cannot heal incomplete coverage.
- Projection uses the required signed-magnitude behavior for wrapped uint16
  numerators, zero-denominator rules, high-bit exclusion, wrapped composite
  points `(2*FGM + 3PM + FTM) mod 65536`, actual per-team games
  `wins[team]+losses[team]`, thresholds, high-byte/low-byte metric pairs, and
  later canonical-key tie wins.
- Rendering supports only complete FIELD GOALS, TOTAL POINTS, 3 POINT SHOTS,
  and FREE THROWS.  Unsupported STEALS, BLOCKED SHOTS, REBOUNDS and incomplete
  migrated seasons retain the exact `PLAYER RESULTS UNAVAILABLE` boundary.
  Results pages are offsets 0/6/12 and use the existing category template
  contract, imported native row geometry, the fixed dynamic-text palette role,
  and `x.xxx`/`xx.xx` formats.

## Scope and exclusions

Writable implementation scope is limited to the shared stats header, gameplay
scene/result seams and authorized state-flow tests, season menu/session/save/
leader logic, the proof-only CLI fixture, the Season test script, and this
directory.  Team-data ranking implementation and all unrelated source/assets
remain read-only.

Explicit exclusions are team-data implementation, gameplay rules/render/assets
outside the authorized seams, importers/source maps/CMake/build/global docs,
player details, team management, substitutions, All-Star, original SRAM
import, new rebound/steal/block semantics, priority metasprites, production
CLI stat seeding, and any main/staging/origin mutation or push.

See [EVIDENCE.md](EVIDENCE.md) for source locations, classifications, and the
reconciled research metadata.  See [PROOF.md](PROOF.md) for exact commands,
results, the current render hashes, and the save/malformed matrix.
