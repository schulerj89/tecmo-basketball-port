# R2B lineage, ownership, and guarded handoff

## Assignment and control provenance

| Field | Value |
| --- | --- |
| Master task | `019fc5d4-f360-78b3-b2a6-c8bae92df690` |
| Sol session | `S-SOL-R2B-SHOTS-INTEGRATION-QA-001` |
| Task | `R2B-SHOTS-OUTCOMES-INTEGRATION-QA` |
| Claim | `OWN-R2B-SHOTS-OUTCOMES-INTEGRATION-QA` |
| Round/lane | `R2B` / `LANE-R2B-SHOTS-OUTCOMES-INTEGRATION-QA` |
| Worktree | `C:/Users/joshs/Projects/tecmo-basketball-port-r2b-shots-outcomes-integration-qa-sol` |
| Branch | `codex/r2b-shots-outcomes-integration-qa-sol` |

Good-signed control lineage includes:

- `6c94652d95462037f4f05d75a5ef3beaabd436dd`, durable assignment transfer;
- `aaac0fef9f3527c550df1e37a63542e4405e4140`, clean takeover, exact signed
  branch merge, and sole independent reviewer allocation;
- `5a625bdf62b9d23db57d3808ff1242ebf0b574cf`, complete personal combined PASS
  ledger, source/ASM/exclusion classifications, deterministic proof, harmless
  diagnostics, and Luna no-defect state.

Each was verified with a Good SSH signature for `jaystar524@gmail.com`, RSA key
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Immutable inputs

| Input | SHA/tree |
| --- | --- |
| Initial last-good/current main | `8a5b9928544a430efa34cbf98a248d6a8cbe7b14` |
| Immutable staging ref | `codex/round-2b-shots-outcomes-staging` |
| Immutable accepted candidate | `7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a` |
| Candidate tree | `4c24e464b4a2c0bb275e307afb99e4827a7e795a` |
| Candidate base | `222d75cfafa9153db1eb44492bf557f11b1a9091` |
| Predicted merge tree | `2d918e8d672f991c87c293096e315a8bde5685da` |

Local `main`, `origin/main`, and live remote main were all exact at
`8a5b992...` during clean takeover, after merge, after personal gates, and
before documentation. The merged branch is a true descendant of that main.

## Three-commit accepted candidate

The immutable candidate is exactly:

1. `24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`, parent `222d75cf...`, tree
   `367c14eb390f53a7b7a45c08d9ad1a02ab44d415`, subject
   `feat: complete R2 shot outcomes`;
2. `8be0258e83369bce58d3a9eabedb4ef575127b25`, parent `24bdde9c...`, tree
   `5863c301ed00e8dedbc9e2af12a3c8b97ea876f3`, subject
   `docs: record R2 shot outcomes`;
3. `7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a`, parent `8be0258e...`, tree
   `4c24e464b4a2c0bb275e307afb99e4827a7e795a`, subject
   `docs: finalize R2 shots handoff`.

All three commits are individually Good SSH-signed with the accepted key.

## Exact authorized merge

The only authorized branch-only reconciliation merge is:

```text
commit  26e6aaf19b639972cb9043f29fc55daa1efce835
parent1 8a5b9928544a430efa34cbf98a248d6a8cbe7b14
parent2 7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
tree    2d918e8d672f991c87c293096e315a8bde5685da
subject merge: reconcile R2 shots outcomes with current main
```

It is Good SSH-signed. Its tree exactly equals the precomputed and freshly
reproduced conflict-free `git merge-tree --write-tree` result.

No conflict resolution, rebase, cherry-pick, force operation, main mutation,
staging mutation, or push occurred.

## Exact candidate ownership

The candidate delta from `222d75cf...` is exactly these 17 paths:

1. `docs/finish-tasks/R2-shots-outcomes/FAULT-LEDGER.md`
2. `docs/finish-tasks/R2-shots-outcomes/PROOF-MANIFEST.md`
3. `docs/finish-tasks/R2-shots-outcomes/README.md`
4. `include/tecmo_gameplay_close_shots.h`
5. `include/tecmo_gameplay_scene.h`
6. `include/tecmo_gameplay_scene_internal.h`
7. `include/tecmo_gameplay_shot_resolution.h`
8. `src/tecmo_cli_gameplay_shot_resolution.c`
9. `src/tecmo_cli_gameplay_shots.c`
10. `src/tecmo_gameplay_close_shots.c`
11. `src/tecmo_gameplay_dunk_cutaway.c`
12. `src/tecmo_gameplay_jump_shots.c`
13. `src/tecmo_gameplay_scene_shots.c`
14. `src/tecmo_gameplay_scene_test_state_flow.c`
15. `src/tecmo_gameplay_scene_validation.c`
16. `src/tecmo_gameplay_shot_resolution.c`
17. `tools/Run-GameplayCloseShotTests.ps1`

All 17 paths matched the accepted released claim `OWN-R2-SHOTS-OUTCOMES`.
There are no candidate binaries or proprietary payloads.

## Collision accounting

From common candidate base `222d75cf...`:

- candidate normalized path count: 17;
- current-main normalized path count: 80;
- exact normalized intersection: 0;
- merge conflicts: 0;
- candidate `diff --check` failures: 0;
- predicted-tree `diff --check` failures: 0.

R2B's writable tracked set is only
`docs/finish-tasks/R2B-shots-outcomes-integration-qa/**`. All R2B tracked
post-merge additions are confined to that directory.

## Exclusion proof

The following were checked at candidate base, all candidate commits, current
main, predicted tree, merge, and pre-doc branch tip:

| Path | Exact blob |
| --- | --- |
| `src/tecmo_gameplay_scene.c` | `58ad821d31a5559225855fbb30a1566d374063e7` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f` |

These exclusions explain why the public numeric-1 name and stale source-map
sentence remain deferred. No source-map or scene name test was weakened.

## Independent-review registry

The post-merge registry/collision gate found no prior R2B reviewer allocation.
The corrected creation request produced exactly one top-level projectless
reviewer:

```text
019fca5b-3b84-7a82-b2ab-588c50a4b7fd
```

The same pinned reviewer performed source/merge QA, fresh artifact QA, and the
signed documentation review. No replacement task, subagent, fork, or contact
with another Sol/Luna task occurred.

The rejected pre-creation schema request created no task and is recorded in
`FAULT-LEDGER.md`.

## Documentation lineage

The first signed docs candidate descends from merge `26e6aaf...` and changes
only this R2B documentation folder. It is independently reviewed at the exact
signed tip. Any correction is a docs-only Good-signed descendant and is sent
back to the same reviewer.

A commit cannot contain its own object ID or a later independent result. The
exact terminal SHA, its Good signature, final P0/P1/P2 ledger, current-main
observation, and reviewer-unpin result are therefore supplied in the durable
Sol-to-master handoff and then reflected in control.

## Guarded master-only fast-forward

As documented, then-current main remains `8a5b992...`. Before integration,
master should independently confirm:

```powershell
$ExpectedMain = '8a5b9928544a430efa34cbf98a248d6a8cbe7b14'
$FinalR2B = '<EXACT_GOOD_SIGNED_TERMINAL_SHA_FROM_SOL_HANDOFF>'

if ((git rev-parse main) -ne $ExpectedMain) { throw 'local main moved' }
if ((git rev-parse origin/main) -ne $ExpectedMain) { throw 'origin/main moved' }
if (((git ls-remote origin refs/heads/main) -split '\s+')[0] -ne
    $ExpectedMain) { throw 'live main moved' }
git verify-commit $FinalR2B
git merge-base --is-ancestor $ExpectedMain $FinalR2B
if ($LASTEXITCODE -ne 0) { throw 'R2B is not a descendant of expected main' }
git merge-base --is-ancestor `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a $FinalR2B
if ($LASTEXITCODE -ne 0) { throw 'accepted R2 candidate is not ancestral' }
git diff --check $ExpectedMain..$FinalR2B
git merge --ff-only $FinalR2B
```

If main advances before handoff, this stale guard must stop. The lane's
separate authorization then requires a read-only overlap/regression audit and,
only if clean, another Good-signed branch-only no-ff reconciliation with R2B
lineage first and new main second, followed by affected reruns and a new exact
handoff. Master remains the sole owner of main integration and ordinary push.
