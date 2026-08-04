# R3 Player Stats Leaders — Merge Handoff

## Accepted merge

The branch began QA at the already reconciled clean merge:

- Tip: `91f158456d72537f0a8b6ae032cf0b0ade053493`
- Tree: `94c9c2802fe5a5f40ead49a6b591d13e41b4e30d`
- First parent: `bdc2fbb8f5b8497f4855b80e8834696220036aba`
- Second parent: `7897871aa1e392f7650203e23d106b9ad9d7fbbf`
- Merge signature: `Good`, by `jaystar524@gmail.com`

`git merge-tree --write-tree bdc2fbb8... 7897871...` reproduced the accepted
tree. The source candidate's signature was `Good`, its sole parent was
`ed060720...`, and its exact source path ledger is recorded in [LINEAGE.md](LINEAGE.md).
The R3 and R2E changed-path ledgers had zero intersection, so the structural
collision gate remained clear.

## Proof correction checkpoint

The independent review found that the proof-only League Leaders seed could
display impossible percentages at high canonical keys. The bounded correction
was limited to the three proof attempt constants and the six populated render
expectation entries (five hashes changed; TOTAL POINTS category 3 remained
unchanged), with no formula, runtime, ranking, gameplay, or merge changes.

The signed correction checkpoint is:

- `20dcf9a4d30f8d4e557ab61df5af8fc34458c82c`
- sole parent: `91f158456d72537f0a8b6ae032cf0b0ade053493`
- signature: `Good`, SSH key `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`

That checkpoint is frozen and is not amended, reset, rebased, or otherwise
rewritten. The historical pre-correction frame table remains preserved and
explicitly superseded in `PROOF.md`; the distinct current-QA corrected and
repeat tables use the actual ignored build paths and timestamps.

## Terminal closure commit

This QA task adds the four missing R3 handoff files and the five-file R3B
current-main integration-QA report, plus the factual current/historical proof
label repair in `PROOF.md`, as one signed documentation closure commit. That
commit has sole parent `20dcf9a...`, is a descendant of both original merge
parents, and contains no implementation, ROM, save-state, video, or
control-plane artifact. The exact final tip, tree, parents, signature, and
changed-path ledger are emitted by the terminal audit and returned with this
handoff.

No push, main mutation, rebase, cherry-pick, force update, or manual conflict
resolution was performed. A downstream integrator should consume the signed
closure commit as a descendant of `20dcf9a...`; it should not reconstruct the
merge from fragments.

## Verification required at handoff

The closing audit verifies:

```text
git diff --check
git verify-commit <final-tip>
git rev-list --parents -n 1 <final-tip>
git merge-base --is-ancestor bdc2fbb8... <final-tip>
git merge-base --is-ancestor 7897871... <final-tip>
git merge-base --is-ancestor 20dcf9a... <final-tip>
git status --short --branch
```

The expected result is a clean branch, a `Good` signed final commit, all three
ancestry checks succeeding, and only the bounded Markdown report paths changed
after the frozen correction checkpoint.
