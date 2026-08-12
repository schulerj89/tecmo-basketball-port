# R1 TIP fidelity

> **Archival lane record.** This file records the R1 worker branch as it stood
> during its original review and is not a current-main status page. Current
> main has integrated later TPTI-2 scheduling: the cards and close-up reach
> center setup after `61/121/61/208 = 451` updates; the primary human capture
> reaches the cinematic, settlement, and LIVE boundaries at frames
> `516/576/606`; and the independent CPU-only threshold route reaches
> `508/568/598`. Historical branch, proof, and frame-721 statements below are
> retained only as lineage for the superseded worker tip.

Historical status: Sol accepted the first TIP implementation commit
`a37e10207455933be3930e90c55b10b669cb0ef3`, including its clean-commit formal
proof. Independent terminal QA passed the frozen product/proof at
`b678beffeacd745fe438e78d323357dc6f86af95` with `P0=0`, `P1=0`, and one
grouped docs-only `P2`; the same QA task must verify this revised doc tip before
terminal acceptance. At that historical lane tip, Sol-branch integration
remained pending.

Historical worker branch: `codex/r1-tip-fidelity-luna`
Historical worker worktree:
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna`
Expected parent/base: `222d75cfafa9153db1eb44492bf557f11b1a9091`
Authoritative review task: `019fc61e-0f2a-7fb0-a76e-e4676808c959`

At that historical tip, this slice upgraded the strict native TPTI payload to
TPTI-2, recorded the validated Bank05/Bank04/fixed-bank evidence seams, added
the transactional capture/automatic/jump/claim state bridge, and preserved the
accepted scene
presentation and successful frame-721 LIVE handoff. The implementation never
executes ROM/ASM at runtime and never commits ROM bytes, generated proof, or
build output.

Read the companion documents in this order:

- [SCOPE.md](SCOPE.md): writable boundary, compatibility, exclusions.
- [EVIDENCE.md](EVIDENCE.md): Rev1 identity, source spans, hashes, and labels.
- [IMPLEMENTATION.md](IMPLEMENTATION.md): payload/state/scene seams and files.
- [TESTS.md](TESTS.md): focused build/test results and remaining closure gates.
- [LINEAGE.md](LINEAGE.md): Luna revisions, review corrections, diagnostics,
  exact failures, and recoveries.
- [PROOF.md](PROOF.md): accepted formal proof manifest and Sol visual review.
- [MERGE.md](MERGE.md): review, commit, and fast-forward handoff procedure.

The focused TPTI-2 harness, warning-clean console+Win32 rebuild, explicit
console-flow and GUI/console production Win32 launch smoke, and broad
`Run-GameplaySceneTests.ps1` suite pass. The first formal proof passed on its
first attempt at
`build\proof\tipoff-visual-orientation-a37e10207455`; its manifest and full
visual/runtime evidence are recorded in `PROOF.md`. The frozen product/proof
then passed independent terminal QA with only the docs-only P2 above.
`src/tecmo_gameplay_live_proof.c` contains only the durably authorized
fixture change in `live_proof_advance_pretip`: P1/Away held-B is applied during
`JUMP_CONTEST` and cleared in every other phase. The separate
`src/tecmo_flow_test.c` change was likewise limited to
`flow_finish_gameplay_pretip`: within its historical 721-iteration loop, P1
cancel is recomputed from the current phase, P2 remains neutral, and both
callers/post-handoff assertions are preserved.
