# Exact lineage and QA status

## Authority

- Sol orchestrator logical/session boundary: `S-SOL-R2-DEFENSE-CONTACT-001`.
- Current Sol task: `019fca06-752d-7863-a2ae-862747a457b7`.
- Grant checkpoint: `99739ff563b5128b355e54cea25eaeb33305b5a8`, Good-signed as
  reported/verified.
- Worker-registration checkpoint:
  `5e45166583d940055dd6eefe0499f42cb013df98`, Good-signed as
  reported/verified.
- Pre-QA accepted candidate control:
  `aa74b03b90db7b17933d800b8a9ddf6a5fad8b17`, Good-signed as
  reported/verified.
- Exact QA FAIL/control acknowledgment:
  `5a625bdf62b9d23db57d3808ff1242ebf0b574cf`, Good-signed as
  reported/verified.

## Sessions

| Role | Exact session | Model/status | Scope/result status |
| --- | --- | --- | --- |
| Read-only evidence auditor | `019fca19-9ff5-7790-8721-5cf13de90aad` | gpt-5.6-luna/max; completed, unpinned | Zero faults, retries, or replacements. |
| Read-only collision auditor | `019fca19-a8b5-7981-839c-7cce0353a3f3` | gpt-5.6-luna/max; completed, unpinned | Zero faults, retries, or replacements. |
| Persistent implementation/revision worker | `019fca31-519c-7533-b710-7fc8fb09f424` | `Tecmo R2 Defense Contact Raw Foundation Implementation — Luna Max`; gpt-5.6-luna/max; pinned/active until Sol acceptance | Worktree `C:\Users\joshs\Projects\tecmo-basketball-port-r2-defense-contact-luna`; branch `codex/r2-defense-contact-luna`; base/first-candidate sole parent `edf16ca9059158452798dbe5667f5e64ef444e39`; corrective revision expected parent/pre-QA last-good `d5c5fa9b84cdce404751eda1a86e5507fc014656`; zero faults, retries, or replacements. |

## Review state

The first Good-signed branch-only candidate is
`d5c5fa9b84cdce404751eda1a86e5507fc014656`, with tree
`fcfd0c31b14187c7368e83bf1fc235c37b339820` and sole parent
`edf16ca9059158452798dbe5667f5e64ef444e39`. It remains the pre-QA
last-good and is not amended, rebased, or integrated here.

Independent terminal QA identity is recorded distinctly: QA thread
`019fca60-e2c5-7903-8b52-6df76809dd91`; completed QA turn
`019fca60-e497-7052-a705-e42c83fe6e31`; exact title `Tecmo R2 Defense Contact
Raw Foundation Independent Terminal QA — Luna Max`; model/reasoning
gpt-5.6-luna/max; status/pin completed/idle, pinned; topology independent
top-level projectless; zero faults, retries, or replacements. Its initial
verdict was `FAIL`. The QA findings were: compiler/linker object outputs could
appear in the repository root; the runner documented but did not execute the
enclosing B06 `$B081-$B365` provenance hash/length check; and candidate
lineage text still described the Good-signed candidate as awaiting
authorization. These
are QA findings, not faults, retries, or replacements of this worker task;
the worker remains zero-fault/zero-retry/zero-replacement. The corrective
signed descendant's exact object identity is recorded externally after commit.
No runtime, scene, video, audio, or player-facing proof is claimed.

Revision history: the corrected hash, raw `0x17U` value, B081
one-pass/precedence behavior, `$9968` coordinate-pair borrow contract, iNES
layout, truthful boundary output, and mirrored raw-vector coverage were all
revised in this persistent worker session before authorization.
