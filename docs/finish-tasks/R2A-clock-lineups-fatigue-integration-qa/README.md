# R2A clocks, lineups, and fatigue integration QA

Status: **ACCEPT** at signed pre-documentation tip
`73e87dcccbfe1ddc6a78d9b313e8dd75252fb857`.

This directory is the terminal acceptance record for
`OWN-R2A-CLOCK-LINEUPS-INTEGRATION-QA`. The immutable R2 candidate was merged
once into accepted current main on the assigned branch. After a collision
checkpoint, the master granted one additional test-runner path to correct a
stale current-main R4 finale oracle. No runtime source was edited by this QA
lane. The exact signed docs-only terminal commit cannot record its own SHA; the
Sol handoff supplies that SHA and its `git verify-commit` result.

## Decision

- Integration disposition: **ACCEPT**.
- Sol terminal findings: P0 `0`, P1 `0`, P2 `0`.
- Independent Luna terminal findings: P0 `0`, P1 `0`, P2 `0`, P3 `0`.
- One Sol P2, the stale broad TFIN/TFM1 asset-pack oracle, was resolved in the
  signed runner-only commit `73e87dcc...`.
- One Luna P3, stale historical closure wording in the immutable R2 reports,
  is resolved by the seven-commit and integration lineage recorded here.
- One later Luna P3, the temporary result marker in the first static re-audit
  draft, was replaced with the concrete ledger and timing. The same worker's
  corrected-draft confirmation returned terminal PASS with all severities at
  zero.
- Warning-clean MSVC C11 `/W4` build: pass, zero warnings.
- Broad asset-pack suite: `55/55` pass after remediation.
- Focused gameplay-state, TGFT, TGFL, TGCP, TGOR, TPNL, TGVR, TGCT, TGBC,
  TGAI, and TGMO gates: pass.
- NativeFlow and complete GameplayScene integration gates: pass.
- Current-main intro/finale, season, team-data, team-management, music,
  frontend-audio, gameplay-audio, and Win32 launch smokes: pass.
- Accepted R2 proof inventory: `97/97` artifacts valid, with no missing,
  mismatched, or extra artifact.
- Merged-tip deterministic comparison: all `81` shot-clock frames and both
  free-throw orientation frames match the accepted proof byte for byte.
- Ownership/proprietary-artifact scan: pass.
- Dynamic substitution selection, eligibility, timing, and active-lineup
  replacement remain explicitly `incomplete`. They are not upgraded here.

## Acceptance anchors

1. exact current-main input
   `edf16ca9059158452798dbe5667f5e64ef444e39`;
2. immutable signed candidate
   `ed4e56fc595894c692ffca84ae3b35f129317049`, tree
   `84c58b6f3e3dbdeac6acfe50826f4173bb653d4e`;
3. seven linear candidate commits, all Good SSH signed;
4. signed non-fast-forward integration merge
   `8233cb4b7c86612cd290615927439caf83947b1e`, ordered parents current main
   first and candidate second, tree
   `59e81bee9f8e94057a584dbfd7e45053a6d4f8c2`;
5. external signed master control authorizing the collision-cleared one-file
   runner correction
   `360c7806bc9c1b052f9bb249cb62d08348fb1916`;
6. signed runner-only remediation
   `73e87dcccbfe1ddc6a78d9b313e8dd75252fb857`;
7. the signed docs-only terminal commit reported by the Sol handoff.

Every signature above uses identity `jaystar524@gmail.com` and RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Evidence map

- `LINEAGE.md` records the exact Git graph, seven candidate commits, collision
  accounting, master rescope, worker registry, and guarded handoff.
- `COMMANDS.md` records the merge, test, proof, remediation, retry, and final
  verification command shapes.
- `EVIDENCE.md` records classifications, test results, hashes, negative
  coverage, proof/media integrity, visual observations, and ownership scans.
- `INDEPENDENT-QA.md` records the sole Luna identity, first verdict,
  correction/re-audit lineage, workspace effects, and final severity ledger.

All generated packs, executables, reports, PNGs, MP4s, logs, and manifests
remain ignored below `build/`. No ROM, decompilation source, capture, save
state, raw trace, decoded proprietary payload, or private input path is
committed.

## Historical-report P3

The candidate's ten R2 reports are immutable accepted inputs. Three retain
historical wording from before the seventh closure commit: `MERGE.md` describes
closure integration as a next action, and `README.md`/`LINEAGE.md` retain a
six-commit snapshot. Those statements are not used as terminal integration
state. This directory is the superseding R2A record: the candidate has seven
Good-signed commits ending at `ed4e56fc...`, followed by signed merge
`8233cb4b...` and signed runner correction `73e87dcc...`.

## Honest boundary

- Clock/period behavior is a bounded `native_faithful` semantic port, not a
  cycle-exact claim.
- TGFT evolution and TGFL data/projection are source-pinned as recorded in the
  accepted R2 evidence.
- The production fixed five-slot bridge remains
  `native_approximation_with_justification`.
- Dynamic substitutions and production active-lineup replacement remain
  `incomplete` and require a separate scene/game-flow rescope.
- The shot-clock/referee and free-throw frames prove only the documented
  bounded presentation surfaces. They do not claim new scene ownership.
- Audio is not part of the R2 semantic delta. Audio suites are cross-domain
  regression evidence only.
- The regenerated GameplayScene manifest truthfully remains `DRAFT` because
  this is not the dedicated R1 proof branch and `-RequirePass` was not used.

## Non-actions and handoff

This task performed no rebase, cherry-pick, force operation, destructive clean,
main switch, main merge, main push, remote push, or candidate mutation. Only
the master may fast-forward and push main. If local main, `origin/main`, or
live `refs/heads/main` differs from the guarded SHA in `LINEAGE.md`, the
handoff is invalid until the master reconciles that movement.
