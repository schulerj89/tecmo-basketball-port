# R1 TIP Fidelity — review and merge handoff

Status: first TIP implementation commit
`a37e10207455933be3930e90c55b10b669cb0ef3` and its clean-commit formal proof
are accepted by Sol. Independent QA and Sol-branch integration remain pending;
do not merge or push this branch.

## Review gate

Sol reviewed and accepted the source/ABI/pack/test surface and clean-commit
formal proof. The post-rescope build, explicit console-flow and GUI/console
Win32 smoke, focused pre-tip, and broad scene gates passed. Independent QA and
Sol-branch integration remain pending.

The durable clearances permit only two fixture edits beyond the original
boundary: `src/tecmo_gameplay_live_proof.c::live_proof_advance_pretip` supplies
P1/Away held-B during `JUMP_CONTEST` and clears it otherwise, while
`src/tecmo_flow_test.c::flow_finish_gameplay_pretip` recomputes P1 cancel from
that phase inside its existing 721-iteration loop, keeps P2 neutral, and
preserves both callers and post-handoff assertions. No other function or line
in either file may be included. The broad wrapper, Win32 launch smoke, and
formal clean-commit proof pass; independent QA and Sol-branch integration
remain separate pending gates.

## Historical pre-commit handoff

1. Sol recorded approval in the source task.
2. The worker created the bounded first commit on `codex/r1-tip-fidelity-luna`.
3. Sol reviews the committed proof and performs integration/merge in the
   parent workflow. This worker does not push or merge.
4. Proof artifacts remain ignored and are not committed.

## Final identity verified by formal proof

- TPTI-2 version `2`, header `512`, source count `29`, payload `7680` bytes.
- Ordinary stored-payload FNV32 `28910BC1`.
- Ordinary stored-payload FNV64 `7EA1596E8DFAC0C1`.
- TPM2 mechanics FNV32 `3572752A`.
- TPM2 mechanics FNV64 `A52B415F53DA85CA`.
- Exact-source base `7008`; validated zero padding range `6656..7007`.
- Same-pack TGJS-2 dependency required and validated.

Formal `New-TipoffVisualProof.ps1` passed on the first attempt at
`build\proof\tipoff-visual-orientation-a37e10207455`; see `PROOF.md` for the
manifest, inventory, artifact hashes, runtime observations, and visual review.
Independent QA and Sol-branch integration remain pending.
