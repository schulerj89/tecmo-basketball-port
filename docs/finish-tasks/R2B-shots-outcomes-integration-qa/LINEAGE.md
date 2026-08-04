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

Verified Good-signed control lineage includes:

- `6c94652d95462037f4f05d75a5ef3beaabd436dd`, durable assignment transfer;
- `aaac0fef9f3527c550df1e37a63542e4405e4140`, clean takeover, original merge,
  and sole reviewer allocation;
- `5a625bdf62b9d23db57d3808ff1242ebf0b574cf`, original personal combined PASS;
- `981aa4e3b0aece8569b0be247d0e27ef88fa02c7`, accepted new-main advance and
  reconciliation authority.

Applicable commits verify Good for `jaystar524@gmail.com`, RSA key
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Immutable candidate inputs

| Input | SHA/tree |
| --- | --- |
| Candidate base | `222d75cfafa9153db1eb44492bf557f11b1a9091` |
| Immutable staging ref | `codex/round-2b-shots-outcomes-staging` |
| Immutable accepted candidate | `7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a` |
| Candidate tree | `4c24e464b4a2c0bb275e307afb99e4827a7e795a` |
| Initial last-good/main | `8a5b9928544a430efa34cbf98a248d6a8cbe7b14` |
| Initial predicted merge tree | `2d918e8d672f991c87c293096e315a8bde5685da` |

The accepted candidate is exactly three Good-signed commits:

1. `24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`, parent `222d75cf...`, tree
   `367c14eb390f53a7b7a45c08d9ad1a02ab44d415`, subject
   `feat: complete R2 shot outcomes`;
2. `8be0258e83369bce58d3a9eabedb4ef575127b25`, parent `24bdde9c...`, tree
   `5863c301ed00e8dedbc9e2af12a3c8b97ea876f3`, subject
   `docs: record R2 shot outcomes`;
3. `7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a`, parent `8be0258e...`, tree
   `4c24e464b4a2c0bb275e307afb99e4827a7e795a`, subject
   `docs: finalize R2 shots handoff`.

## Original authorized integration

Clean takeover observed local main, `origin/main`, and live remote main all at
`8a5b992...`. Candidate/main normalized overlap was zero, both diff checks
passed, and `git merge-tree --write-tree 8a5b992... 7b9287a...` reproduced
`2d918e8d...`.

After the durable takeover checkpoint, the first authorized branch-only merge
was:

```text
commit  26e6aaf19b639972cb9043f29fc55daa1efce835
parent1 8a5b9928544a430efa34cbf98a248d6a8cbe7b14
parent2 7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
tree    2d918e8d672f991c87c293096e315a8bde5685da
subject merge: reconcile R2 shots outcomes with current main
```

It is Good-signed and exactly matches the predicted tree. Original personal
combined gates and independent source/evidence QA passed at this lineage.

The first docs checkpoint is Good-signed
`6eaaa535fa69a12e0f63012470dcc052583351b5`, parent `26e6aaf...`, tree
`6b9ccf94b4158bbeecd3c24a2be0a9f707297654`, subject
`docs: record R2B shots outcomes integration QA`. It later became non-terminal
because main advanced.

## Accepted new-main reconciliation

Control `981aa4...` authorized reconciliation after stable local main,
`origin/main`, and live remote main advanced to accepted TIP terminal
`0ef11cf247e3110b6064e79a4c496be6346f3e13`. The R2B/new-main merge base is
`8a5b992...`.

Read-only preflight at R2B tip `6eaaa535...` found:

- R2B lineage changed paths: 23;
- new-main changed paths: 31;
- exact normalized overlap: 0;
- both side diff checks: PASS;
- merge-tree result: conflict-free;
- predicted ordered merge tree:
  `37bbb3868ee1b2b35fbaec1f7801213d648d7fb0`.

Accepted new main alone advances the two original exclusion paths. Personal
diff/seam audit classified them as TIP/TPTI-2 changes and confirmed numeric 1
remains unsupported/`"invalid"` and the raw-group wording remains
`"intentionally unexposed"`.

The second authorized branch-only merge is:

```text
commit  d3f1980d1d9147c47bd6a3bd555708ad6bfcb0f9
parent1 6eaaa535fa69a12e0f63012470dcc052583351b5
parent2 0ef11cf247e3110b6064e79a4c496be6346f3e13
tree    37bbb3868ee1b2b35fbaec1f7801213d648d7fb0
subject merge: reconcile R2B shots outcomes with accepted TIP main
```

It is Good-signed, both required ancestors verify, and its tree exactly matches
the precomputed reconciliation prediction. Reconciled combined gates and fresh
proofs passed at this commit.

No conflict resolution, rebase, cherry-pick, force operation, main mutation,
staging mutation, origin mutation, or push occurred.

## Exact candidate ownership

The immutable candidate delta from `222d75cf...` is exactly these 17 paths:

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

All match accepted released claim `OWN-R2-SHOTS-OUTCOMES`; no binary or
proprietary payload is present.

R2B tracked writable scope is only
`docs/finish-tasks/R2B-shots-outcomes-integration-qa/**`, plus the two expressly
authorized signed branch-only reconciliation merges. All docs commits change
only this directory.

## Shared-file blob history

| Path | Original exclusion through `26e6aaf` | Accepted new-main/reconciled blob |
| --- | --- | --- |
| `src/tecmo_gameplay_scene.c` | `58ad821d31a5559225855fbb30a1566d374063e7` | `504d3d0459780779f47a533ce8bb548208a4195d` |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f` | `40417d8544ce9ffaca7b7110f341fa82bd4b486f` |

The second-column contract proves immutable candidate exclusion. The third
column records accepted-main-only evolution; it is not R2B ownership or a path
collision.

## Independent-review registry

Exactly one top-level projectless reviewer exists for this lane:

```text
019fca5b-3b84-7a82-b2ab-588c50a4b7fd
```

The same pinned Luna performs source/merge QA, original and reconciled artifact
QA, and signed-document review. No replacement, subagent, fork, or contact with
another Sol/Luna task occurred. The rejected pre-creation schema request made
no task and is recorded in `FAULT-LEDGER.md`.

## Documentation lineage

Good-signed revised docs candidate
`fcc520998c206c2f244fab8d75b69fe8ac96bf64`, tree
`445b1615dea553b328c2dbf79bb88ee342943862`, descends directly from
reconciliation `d3f1980d...` and changes exactly the six files in this folder.
The same pinned Luna accepted that exact tip with P0=0/P1=0/P2=0 and requested
only that its completed result be appended to the terminal docs. The resulting
final docs-only Good-signed descendant is returned to that same reviewer for a
narrow no-overreach verification before handoff.

A commit cannot contain its own object ID or a later review result. The exact
terminal SHA, Good signature, final severity ledger, stable-current-main
observation, and eventual reviewer-unpin are therefore supplied in the durable
Sol-to-master handoff and recorded by control.

## Guarded master-only fast-forward

The expected main for this handoff is `0ef11cf...`. Master should independently
confirm:

```powershell
$ExpectedMain = '0ef11cf247e3110b6064e79a4c496be6346f3e13'
$AcceptedCandidate = '7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a'
$Reconciliation = 'd3f1980d1d9147c47bd6a3bd555708ad6bfcb0f9'
$FinalR2B = '<EXACT_GOOD_SIGNED_TERMINAL_SHA_FROM_SOL_HANDOFF>'

if ((git rev-parse main) -ne $ExpectedMain) { throw 'local main moved' }
if ((git rev-parse origin/main) -ne $ExpectedMain) { throw 'origin/main moved' }
if (((git ls-remote origin refs/heads/main) -split '\s+')[0] -ne
    $ExpectedMain) { throw 'live main moved' }
git verify-commit $FinalR2B
git merge-base --is-ancestor $ExpectedMain $FinalR2B
if ($LASTEXITCODE -ne 0) { throw 'terminal R2B is not a descendant of main' }
git merge-base --is-ancestor $AcceptedCandidate $FinalR2B
if ($LASTEXITCODE -ne 0) { throw 'accepted R2 candidate is not ancestral' }
git merge-base --is-ancestor $Reconciliation $FinalR2B
if ($LASTEXITCODE -ne 0) { throw 'reconciliation is not ancestral' }
git diff --check $ExpectedMain..$FinalR2B
git merge --ff-only $FinalR2B
```

If main advances, this stale guard must stop. Standing authority requires a new
read-only overlap/regression audit and, only if clean, another Good-signed
branch-only no-ff reconciliation with R2B lineage first and new main second,
followed by affected reruns and a new exact handoff. Master remains sole owner
of main integration and ordinary push.
