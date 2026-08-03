# R1 TIP Fidelity — review and merge handoff

Status: the first TIP implementation commit is authorized after Sol's accepted
source/ABI/pack/test review. Clean-commit formal proof and independent QA
remain pending; do not merge or push this branch.

## Review gate

Sol has reviewed and accepted the source/ABI/pack/test surface for the first
implementation commit. The post-rescope build, explicit console-flow and
GUI/console Win32 smoke, focused pre-tip, and broad scene gates passed. The
clean-commit formal proof and independent QA remain pending.

The durable clearances permit only two fixture edits beyond the original
boundary: `src/tecmo_gameplay_live_proof.c::live_proof_advance_pretip` supplies
P1/Away held-B during `JUMP_CONTEST` and clears it otherwise, while
`src/tecmo_flow_test.c::flow_finish_gameplay_pretip` recomputes P1 cancel from
that phase inside its existing 721-iteration loop, keeps P2 neutral, and
preserves both callers and post-handoff assertions. No other function or line
in either file may be included. The broad wrapper and Win32 launch smoke now
pass; the formal clean-tree proof remains separate.

## Expected handoff after authorization

1. Sol records approval in the source task.
2. The worker creates one bounded commit on `codex/r1-tip-fidelity-luna` containing only the authorized changed paths listed in `README.md` and this task's diff.
3. Sol reviews the commit and performs the integration/merge in the parent workflow. This worker does not push or merge.
4. Any later proof artifacts remain ignored and are not committed.

## Final identity to verify before commit

- TPTI-2 version `2`, header `512`, source count `29`, payload `7680` bytes.
- Ordinary stored-payload FNV32 `28910BC1`.
- Ordinary stored-payload FNV64 `7EA1596E8DFAC0C1`.
- TPM2 mechanics FNV32 `3572752A`.
- TPM2 mechanics FNV64 `A52B415F53DA85CA`.
- Exact-source base `7008`; validated zero padding range `6656..7007`.
- Same-pack TGJS-2 dependency required and validated.

Formal `New-TipoffVisualProof.ps1` has not been run. The first implementation
commit is authorized, but Sol must complete clean-commit formal proof,
independent QA, and merge authorization afterward.
