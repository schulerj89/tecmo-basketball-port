# R2A lineage, ownership, and guarded handoff

## Control and reservation provenance

The master reserved `OWN-R2A-CLOCK-LINEUPS-INTEGRATION-QA` in Good-signed
control commit `bc0c6804ba9e852db6e60e74216c389d412f8a56`, tree
`faf572d80cbb14f9a9681cf0a38e41eddc205e02`. After the broad asset-pack
oracle exposed its stale pre-R4 finale assumptions, the master collision
checked and durably authorized the sole additional writable file in
Good-signed control commit
`360c7806bc9c1b052f9bb249cb62d08348fb1916`, tree
`44d7f2190169904d5e516ac7fe27528f423fd23f`, parent
`ea23a3daff47625a7868f906d33dc8b3b2881465`.

The control commits are provenance on the separate master-orchestration
lineage, not parents of this integration branch. Both verify as Good SSH
signatures for `jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Immutable inputs

| Role | Commit | Tree | Parent | Signature |
| --- | --- | --- | --- | --- |
| accepted main at allocation | `edf16ca9059158452798dbe5667f5e64ef444e39` | `b5d8435a6664e72513fbb7e0212c4f5c8554174c` | `6b5d43546128408de8ab246d22f1b48322714183` | Good |
| immutable R2 candidate | `ed4e56fc595894c692ffca84ae3b35f129317049` | `84c58b6f3e3dbdeac6acfe50826f4173bb653d4e` | `1567f284ff48a2334fb6a9bd82d00aadf0cdb373` | Good |

The common base of those inputs is
`222d75cfafa9153db1eb44492bf557f11b1a9091`. Assignment-time local `main`,
`origin/main`, and live `refs/heads/main` were all exactly `edf16ca...`. The
immutable staging ref
`codex/round-2a-clock-lineups-fatigue-staging` resolved exactly to
`ed4e56fc...`.

## Seven-commit candidate

The candidate is a linear seven-commit chain. Every row was individually
verified Good with the identity and fingerprint above.

| # | Commit | Tree | Subject |
| ---: | --- | --- | --- |
| 1 | `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a` | `2903fa09e19afb29316b0baae935edeb26fd9fed` | Implement R2 clock fatigue and free-throw lineups |
| 2 | `540ae0ba47ef44d6096781ffd0c276012e683221` | `6637548240b3fc92f65c941d400c28c9ff00414f` | Document R2 clock lineup fatigue handoff |
| 3 | `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8` | `1165566cd5e69ed1caf8a9b7bb9dfe02d5dc2b16` | Record R2 handoff documentation commit |
| 4 | `1536ae31e7016f6e9adbddb7868e2d40e51c1085` | `27c585c2e46c5fe8509249192ea45f93bf34ef80` | Record Sol R2 QA and production proof |
| 5 | `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` | `8255af1499d1efe271e26e5e3bf3d5af5e335322` | Remediate R2 transactional boundaries |
| 6 | `1567f284ff48a2334fb6a9bd82d00aadf0cdb373` | `c658e12c9dcb9469b34543b1c1b9299132de21e1` | Record v2 Sol QA and proof |
| 7 | `ed4e56fc595894c692ffca84ae3b35f129317049` | `84c58b6f3e3dbdeac6acfe50826f4173bb653d4e` | Close R2 clock lineups fatigue documentation |

This seven-row truth supersedes the immutable candidate reports' historical
six-commit snapshot without rewriting those accepted inputs.

## Merge and remediation graph

The clean precomputed merge tree was
`59e81bee9f8e94057a584dbfd7e45053a6d4f8c2`. The sole integration merge is
Good-signed commit `8233cb4b7c86612cd290615927439caf83947b1e`
with exactly that tree and ordered parents:

1. `edf16ca9059158452798dbe5667f5e64ef444e39` - current-main lineage;
2. `ed4e56fc595894c692ffca84ae3b35f129317049` - immutable R2 candidate.

The subsequent Good-signed remediation is
`73e87dcccbfe1ddc6a78d9b313e8dd75252fb857`, tree
`bbe21f76899a47f9ef3977538d51be604b9c83dd`, parent `8233cb4b...`. It changes
only `tools/Run-AssetPackTests.ps1` with 32 insertions and 7 deletions. It
aligns a stale broad finale test oracle to the already accepted TFM1 runtime
contract; it does not change importer, runtime, assets, rendering, build
registration, or candidate content.

The terminal docs-only commit is a direct child of `73e87dcc...`. A commit
cannot truthfully embed its own object name, so its exact SHA, tree, Good
signature result, and clean-status observation are supplied in the signed Sol
handoff.

## Collision and path accounting

From common base `222d75cf...`:

- accepted current main changes `56` paths;
- immutable candidate changes exactly `18` paths;
- their path intersection is `0`;
- the merge first-parent delta is exactly the same `18` candidate paths;
- the predicted and actual merge trees are identical;
- remediation changes exactly the one master-authorized runner;
- the pre-documentation branch delta from current main is `19` paths;
- this directory contributes five terminal text reports;
- terminal delta after the docs commit is therefore `24` paths;
- ownership escapes, binary tracked rows, and proprietary-artifact paths are
  all `0`.

The candidate's exact 18 paths are:

```text
docs/finish-tasks/R2-clock-lineups-fatigue/APPROXIMATIONS.md
docs/finish-tasks/R2-clock-lineups-fatigue/EVIDENCE.md
docs/finish-tasks/R2-clock-lineups-fatigue/IMPLEMENTATION.md
docs/finish-tasks/R2-clock-lineups-fatigue/LINEAGE.md
docs/finish-tasks/R2-clock-lineups-fatigue/MERGE.md
docs/finish-tasks/R2-clock-lineups-fatigue/OBSERVATIONS.md
docs/finish-tasks/R2-clock-lineups-fatigue/PROOF.md
docs/finish-tasks/R2-clock-lineups-fatigue/README.md
docs/finish-tasks/R2-clock-lineups-fatigue/SCOPE.md
docs/finish-tasks/R2-clock-lineups-fatigue/TESTS.md
include/tecmo_gameplay_fatigue.h
include/tecmo_gameplay_free_throw_lineup.h
src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c
src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.c
src/tecmo_gameplay_fatigue.c
src/tecmo_gameplay_free_throw_lineup.c
src/tecmo_gameplay_state.c
tools/Run-GameplayFreeThrowLineupTests.ps1
```

The Sol's writable tracked scope was this integration-report directory plus
the single collision-cleared runner. Candidate and current-main product paths
were read-only except for the one deliberate signed branch-only merge.

## Independent-worker registry

Exactly one top-level, projectless worker was created and pinned:

- task `019fca0a-a7ff-7d92-b2ad-a6b5ab51aece`;
- `gpt-5.6-luna`, thinking `max`, host `local`;
- created `2026-08-03T23:52:03Z`;
- initial audit started `2026-08-03T23:52:04Z` and completed
  `2026-08-04T00:08:10Z` in `966,391` ms;
- no creation/pin bad request, retry, replacement, predecessor, subagent, or
  second independent worker.

Its initial P0/P1/P2 ledger was empty and its only finding was a P3 about the
candidate reports' historical closure wording. The same task was retained for
the runner/docs re-audit. Its first strict static re-audit ran from
`2026-08-04T00:28:12.7865994Z` through
`2026-08-04T00:38:15.8057868Z`, duration `00:10:03.0191874`, and found only
the temporary result marker then present in `INDEPENDENT-QA.md`. That P3 is
resolved by the concrete ledger and timing now recorded there.
`INDEPENDENT-QA.md` records the prompt-boundary correction, two diagnostic
invocation faults, complete workspace-effect disclosure, both P3 resolutions,
and the corrected severity ledger. The corrected-draft confirmation ran from
`2026-08-04T00:41:36.8905692Z` through
`2026-08-04T00:42:41.5224603Z`, duration `00:01:04.6318911`, and returned
terminal PASS with P0/P1/P2/P3 all `0`. Its sole auxiliary fault was a
read-only filename-list comparison against an unsorted expected array; the
corrected comparison confirmed exactly five reports and changed no state.

## Guarded master-only fast-forward

The following is an instruction for the master after substituting the exact
Good-signed terminal docs commit reported by Sol. It was not executed by this
task.

```powershell
$ExpectedOld = 'edf16ca9059158452798dbe5667f5e64ef444e39'
$Accepted = '<SIGNED_TERMINAL_DOCS_SHA>'
$MainWorktree = 'C:\Users\joshs\Projects\tecmo-basketball-port-main-integration'
Set-Location -LiteralPath $MainWorktree

git fetch origin
if ((git rev-parse main).Trim() -ne $ExpectedOld) { throw 'local main moved' }
if ((git rev-parse origin/main).Trim() -ne $ExpectedOld) { throw 'origin/main moved' }
$LiveMain = ((git ls-remote origin refs/heads/main) -split '\s+')[0]
if ($LiveMain -ne $ExpectedOld) { throw 'live main moved' }
if (git status --porcelain=v2 --untracked-files=all) { throw 'main worktree dirty' }

git verify-commit $Accepted
if ($LASTEXITCODE -ne 0) { throw 'terminal signature invalid' }
git merge-base --is-ancestor $ExpectedOld $Accepted
if ($LASTEXITCODE -ne 0) { throw 'terminal is not a descendant of guarded main' }

git merge --ff-only $Accepted
if ($LASTEXITCODE -ne 0) { throw 'fast-forward failed' }
if ((git rev-parse main).Trim() -ne $Accepted) { throw 'unexpected main result' }
git push origin main:main
```

If any main observation differs from `$ExpectedOld`, this instruction is
invalid. The master must first authorize reconciliation of the newly accepted
main into the assigned integration branch, rerun the combined gate, and issue
a new terminal descendant. No force, rebase, cherry-pick, or non-fast-forward
main operation is permitted.
