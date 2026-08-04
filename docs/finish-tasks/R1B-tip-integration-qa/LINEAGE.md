# Git, authority, diagnostics, and acceptance lineage

## Candidate ancestry

```text
222d75cfafa9153db1eb44492bf557f11b1a9091  common R1 base
a37e10207455933be3930e90c55b10b669cb0ef3  accepted TIP implementation/proof
b678beffeacd745fe438e78d323357dc6f86af95  accepted TIP evidence closure
1b1bf23b3c48947d988c9231870f9827f88cc5a6  accepted TIP follow-up
e21f9a6621df5527544be1de4d0dc60382539c60  immutable accepted TIP terminal
```

All four candidate commits verify Good SSH for
`jaystar524@gmail.com`, key
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

At allocation, local `main`, `origin/main`, and live remote main were all:

```text
edf16ca9059158452798dbe5667f5e64ef444e39
```

The immutable staging ref
`codex/round-1b-tip-fidelity-staging` resolved to `e21f9a...`.
Common base between current main and the candidate was `222d75cf...`.
The current-main side changed 56 paths, the candidate changed 24, normalized
overlap was `0`, and Git-native merge-tree predicted:

```text
a08a66bb9edc858f7b87e72bff160c5cd8310186
```

## Signed product merge

After the clean takeover/collision/candidate checkpoint, this lane made its
single authorized branch-only no-fast-forward product merge:

```text
564d83835258ab48b9ea2ebcc867ba41e185822f
  parent 1: edf16ca9059158452798dbe5667f5e64ef444e39
  parent 2: e21f9a6621df5527544be1de4d0dc60382539c60
  tree:     a08a66bb9edc858f7b87e72bff160c5cd8310186
```

The merge verifies Good SSH. Both parents are ancestors in the required order,
the tree equals the precompute, and no conflict resolution or manual product
edit occurred.

## Traceability finding and authorized correction

The additional user-requested ASM/native gate found current-main guidance
contradicting the accepted runtime and R1 records on equal-error claims. The
Sol reported the mismatch to master before terminal acceptance and made no
out-of-scope edit.

Master then signed control checkpoint
`9a3b4623022e3e4dc46142f5370f26c705bd9fe3`, collision-cleared exactly
`AGENTS.md` and `PORTING.md`, and added those two paths to
`OWN-R1B-TIP-INTEGRATION-QA` for guidance-only tie-policy correction.

The resulting commit is:

```text
7ba0066ca1084e971a268d0b1b0176d065fdbd01
  parent: 564d83835258ab48b9ea2ebcc867ba41e185822f
  tree:   c8b1f48ef771ca566b5fcadec227b2d2daec6e2b
  paths:  AGENTS.md
          PORTING.md
```

It verifies Good SSH with the same signer/key. It replaces both stale
`AGENTS.md` equal-Away statements and the one stale `PORTING.md` statement
with the authorized equality-deferral/incomplete-original-tie boundary. No
runtime, source-map, test, build, or tool path changed.

## Signed live-main reconciliation

Before the report commit, local `main`, `origin/main`, and live remote main
advanced together to accepted R2A terminal
`8a5b9928544a430efa34cbf98a248d6a8cbe7b14`. The Sol stopped, audited its
Good SSH signature and ten-commit accepted lineage, compared both path sets,
and reported the collision/ref checkpoint to master. The new-main delta
changed 24 paths; the R1B lineage changed 26 paths from the shared prior main;
normalized overlap was `0`. Git-native precompute returned:

```text
fb2e4cd08e5c20dfb5f4167853bd49ade6095780
```

After master's explicit authorization, the assigned branch made this signed
non-fast-forward reconciliation, with the required R1B-first parent order:

```text
3aa7dfb523d6fee51785f845d023e7ea8a990074
  parent 1: 7ba0066ca1084e971a268d0b1b0176d065fdbd01
  parent 2: 8a5b9928544a430efa34cbf98a248d6a8cbe7b14
  tree:     fb2e4cd08e5c20dfb5f4167853bd49ade6095780
```

The merge verifies Good SSH, its tree equals the precompute, and there was no
conflict resolution or manual product edit. Warning-clean build, focused TIP,
R2A/free-throw/fatigue/asset-pack, broad gameplay, native-flow, Win32, season,
and fresh deterministic proof gates all passed at this signed tip.

## Terminal report commit

After the same Luna's post-reconciliation terminal review, one
Good-SSH-signed report commit has `3aa7dfb...` as its sole parent and changes
exactly these five paths:

```text
docs/finish-tasks/R1B-tip-integration-qa/README.md
docs/finish-tasks/R1B-tip-integration-qa/COMMANDS.md
docs/finish-tasks/R1B-tip-integration-qa/EVIDENCE.md
docs/finish-tasks/R1B-tip-integration-qa/INDEPENDENT-QA.md
docs/finish-tasks/R1B-tip-integration-qa/LINEAGE.md
```

The exact report SHA is supplied in the terminal master handoff rather than
through a self-referential edit.

## Temporal ref observations

| Observation | `main` | `origin/main` | R1B HEAD |
|---|---|---|---|
| clean takeover | `edf16ca...` | `edf16ca...` | `edf16ca...` |
| signed product merge | `edf16ca...` | `edf16ca...` | `564d838...` |
| personal combined gate at `2026-08-04T00:06:02Z` | `edf16ca...` | `edf16ca...` | `564d838...` |
| guidance correction | unchanged at last gate | unchanged at last gate | `7ba0066...` |
| live-main advance checkpoint | `8a5b992...` | `8a5b992...` | `7ba0066...` |
| signed reconciliation | `8a5b992...` | `8a5b992...` | `3aa7dfb...` |
| reconciled proof complete `2026-08-04T01:13:08Z` | `8a5b992...` | `8a5b992...` | `3aa7dfb...` |
| terminal docs handoff | verified again at handoff | verified again at handoff | exact report SHA in handoff |

If main advances before terminal handoff, this table is not permission to use a
stale fast-forward instruction. This Sol must reconcile the newly accepted main
on its assigned branch, rerun the combined gate, reuse the same Luna, and
produce a true descendant before reporting acceptance.

## Master control checkpoints

- Reservation checkpoint:
  `bc0c6804ba9e852db6e60e74216c389d412f8a56`.
  It verifies Good SSH and is intentionally a master-control commit, not an
  ancestor required in the product/candidate lineage.
- Clean takeover transferred `OWN-R1B-TIP-INTEGRATION-QA` after exact
  branch/base/ref/worktree/collision verification.
- The Sol reported the candidate path audit, signature chain, precomputed tree,
  exact merge SHA/parents/tree, personal combined gates, proof hash, and sole
  Luna allocation.
- Master added the original ASM/native traceability gate at the user's request.
- The Sol reported the equal-error mismatch and source-map subdispatch boundary
  before terminal acceptance.
- Master signed the narrow two-file guidance rescope at `9a3b462...`.
- The Sol reported signed correction `7ba0066...` before terminal docs.
- The Sol reported the live-main advance, zero-overlap audit, and predicted
  tree; master explicitly authorized signed R1B-first reconciliation with
  `8a5b992...` second and the affected regression gates.
- The Sol reported signed reconciliation `3aa7dfb...`, exact parents/tree, and
  fresh proof before terminal acceptance.
- Main integration and every push remain master-owned.

## Independent task lineage

Exactly one projectless read-only `gpt-5.6-luna` / `max` task was created:
`019fca13-3e2c-7e52-9e7a-198c66bdfef8`. It was pinned immediately and reused
for the traceability/correction/report review. It received no tracked writable
scope and did not contact another task. Literal/equivalent bad-request count
was `0`; no retry/replacement branch was required. Its initial guidance audit
was BLOCKED `0/0/1`; its first terminal-report review was BLOCKED `0/0/2`
on a stale external source filename and unfinished closure wording. Both exact
report-only fixes, the new-turn retry lineage, and the current `0/0/0` Sol
disposition are in [INDEPENDENT-QA.md](INDEPENDENT-QA.md). The same pinned task
then returned **PASS `0/0/0`**, with bad-request count `0`; it was unpinned
after that review. Following live-main reconciliation, the same completed task
was re-pinned and reused for one final read-only review of `3aa7dfb...`, the
fresh gates/proof, and the revised terminal reports. It found one report-only
historical-proof variable mismatch, verified the exact correction, and
returned terminal **PASS `0/0/0`**. Complete post-reconciliation diagnostics
are recorded in [INDEPENDENT-QA.md](INDEPENDENT-QA.md). It was unpinned again
after that verdict and was not archived.

## Diagnostics and recovery

All diagnostics were recovered and are preserved as process lineage rather
than hidden failures:

| Diagnostic | Count | Recovery/result |
|---|---:|---|
| `AGENTS.md` and thread/tool output truncation | several bounded reads | Re-read complete instructions and required records in bounded chunks |
| Unquoted PowerShell Git range parsed as an expression | 1 | Reissued as a quoted revision range |
| Good Git signature text on stderr surfaced as `NativeCommandError` despite exit 0 | several verifies | Captured `$LASTEXITCODE=0`, signer, and key explicitly |
| First Win32 fresh-worktree launch lacked ignored root pack | 1 | Built exact canonical pack; identical launch rerun PASS |
| Broad PNG inventory was noisy/truncated | 1 | Narrowed to exact proof roots and representative full-resolution frames |
| Initial contact-sheet hash pipeline had an empty/invalid projection | 1 | Reissued exact `Get-FileHash`; no artifact mutation |
| Windows `rg` wildcard path error | 1 | Searched exact directories/files |
| First direct FNV formatter produced signed/leading-zero display issues | 2 probes | Corrected unsigned formatting and rehashed all spans |
| Initial raw pointer helper returned low bytes only | 1 | Reimplemented full little-endian pointer decode |
| Thread-read calls used unsupported limits | 2 | Reissued with supported limits |
| First two-file guidance patch used over-specific context | 1 | Patch applied nothing; reissued exact minimal hunks |
| First COMMANDS report patch contained an unescaped PowerShell continuation character in the tool wrapper | 1 | No file was created; reissued without continuation characters |
| Draft source lookup initially assumed `$DecompRoot/lifted` | 2 Sol/Luna probes | Resolved the actual read-only `$DecompRoot/decomp/lifted` location and corrected the reproducible command |
| Read-only probes piped directly from a `foreach` statement | 2 | Assigned loop output first, then piped it; named tools exist and all code-fence counts are even |
| Draft Win32 command initially omitted the exact script suffix and `-DecompRoot` argument | 1 self-review | Corrected to the command that exercises the recorded explicit developer flow |
| Live main advanced before report commit | 1 required checkpoint | Stopped, audited zero overlap and signatures, obtained master authorization, made signed R1B-first reconciliation, and reran affected gates |
| Fatigue/native-flow wrappers inherited nonzero exit from intentional negative child vectors | 2 observations | Used script success state and exact PASS output; reruns confirmed the runners completed successfully |
| Fresh proof rejected the five authorized untracked report drafts | 1 clean-worktree prerequisite | Temporarily held the exact five drafts outside the worktree, generated proof, restored five-of-five hash-identically, and removed the empty hold |
| Unquoted `HEAD^{tree}` in one post-proof Git probe | 1 | Quoted the revision and verified exact tree `fb2e4cd...` |
| Post-reconciliation report used `$ProofRoot` for the historical `564d838...` proof command | 1 Luna P2 | Changed only that invocation to `$InitialProofRoot`; the same active Luna verified the exact fix |
| Luna bounded EOF read appeared to omit five unresolved bullets | 1 provisional alert | True-EOF reread proved all six bullets were already present; no edit required |

The independent worker's own recovered diagnostics and retry lineage are
recorded in [INDEPENDENT-QA.md](INDEPENDENT-QA.md). None changed product code or
invalidated evidence.

## Scope, ownership, and proprietary audit

- The accepted candidate arrived only through signed merge `564d838...`;
  accepted R2A main arrived only through signed reconciliation `3aa7dfb...`.
- Manual guidance writes are exactly `AGENTS.md` and `PORTING.md` under
  signed rescope `9a3b462...`.
- Manual terminal writes are exactly the five R1B report paths.
- Product/runtime, asset builder, source map, tests, build scripts, and proof
  tools were read-only throughout this lane.
- Ignored packs, executables, logs, reports, frames, sheets, videos, and
  manifests remain under `build/`.
- No ROM, decompilation, ASM, emulator trace, capture, screenshot from the
  original game, save state, or other proprietary artifact is committed.
- The preserved historical dirty tip-input-e2e worktree was never touched.
- No other active claim/worktree overlapped the guidance or report paths.

## Signing and guarded integration policy

The report commit is created with Good SSH signing and a signoff trailer.
Master may fast-forward only if all of the following are still exact:

1. live remote `refs/heads/main`, local `main`, and `origin/main` equal
   `8a5b9928544a430efa34cbf98a248d6a8cbe7b14`;
2. the reported terminal commit verifies Good SSH;
3. `8a5b992...`, `e21f9a...`, `564d838...`, `7ba0066...`, and `3aa7dfb...`
   are ancestors of that terminal commit;
4. the branch/worktree is clean and the terminal delta is only the authorized
   candidate merge, two guidance files, and five R1B reports; and
5. integration is non-force and performed by master, not this lane.

If any guard differs, stop and re-evaluate. Do not apply the instruction
blindly, force, rebase, or cherry-pick.
