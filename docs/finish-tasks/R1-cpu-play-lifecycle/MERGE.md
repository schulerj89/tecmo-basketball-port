# R1 CPU lifecycle merge handoff

This file is the CPU-only handoff record. Sol accepted the eleventh
`DRAFT_PASS` evidence snapshot, and the formal clean proof plus independent QA
passed at code/doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd`. This turn is a bounded docs-only
closure revision. No merge, rebase, or push is authorized from this worker.

## Expected integration

- Expected parent/base: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.
- Sol-recorded control-plane checkpoint (not a product commit): `dea1fd7c2c2761fe08a6a27ab13a5e661e2b7094`.
- Worker branch: `codex/r1-cpu-play-lifecycle-luna`.
- Final worker implementation/evidence commit:
  `db5a043244361b3e9bbab2e154c7f14e4a4a5014`.
- Proven code/doc HEAD used by the formal proof and QA:
  `8be7a9f9a11d43e68b090a98af122758885931fd`.
- Terminal docs-closure commit SHA: reported in the final handoff after commit;
  it is intentionally not self-embedded in this commit.
- Master merge commit: `PENDING_SOL_MASTER_MERGE_SHA`.

The worker remains based on the expected parent. Sol personally inspected the
accepted draft source/trace/visual evidence; the formal clean `-RequirePass`
proof and independent QA are recorded as passed at the proven HEAD above.
Source-map compatibility and master integration remain Sol responsibilities.
The worker must not resolve history by merge/rebase or by changing an unowned
path. Dynamic policy/workspace effects and normal scene integration remain
deferred to `R1-LIVE`.

## Ordered acceptance checklist

1. Confirm allowed changed paths and source-map/TGAI identity.
2. Run the focused CPU wrapper, gameplay-lab static suite, and final audits.
3. Preserve the accepted eleventh draft evidence as chronology and distinguish
   it from the later formal clean proof.
4. Record the proven code/doc HEAD and the docs-only closure commit SHA.
5. Sol integrates the ordered commit(s) onto the expected parent; no worker
   branch is independently merged or pushed by Luna.

## Sol fast-forward-only integration from the expected base

Use the terminal docs-closure SHA from the final Luna handoff. From Sol's
worker branch at the expected base, the authorized integration shape is:

```powershell
git status --short --untracked-files=all
git rev-parse HEAD
git merge --ff-only <TERMINAL_DOCS_CLOSURE_SHA_FROM_FINAL_HANDOFF>
git status --short --untracked-files=all
```

The expected starting `HEAD` is
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. These are instructions for Sol;
this worker does not merge, rebase, push, or claim a master merge SHA.

## Legal/provenance confirmation

No ROM, ASM/decompilation, emulator binary, capture, generated asset pack,
PNG/video, or private absolute path is a merge input. Runtime remains native-C
plus validated semantic assets. Proof outputs remain ignored/private.
