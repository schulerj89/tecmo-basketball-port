# Lineage and control record

## Signed control

- Original bounded authority: `647512886d29b9d53b81aae6f5cc6d3cba8f7da9`.
- Assignment checkpoint: `486b9ae15fe3e27286d7259248685fa3d82da404`, tree `1a3bc281ec2fbb39987635adc0a9f3ffa56ea0c8`, parent `647512886d29b9d53b81aae6f5cc6d3cba8f7da9`.
- Durable master checkpoint: `24e71d731b14a4d9560f995f11dc35c002c69365`, tree `91dc49939d800960996743d2ec0dfa4962455731`, parent `486b9ae15fe3e27286d7259248685fa3d82da404`, subject `control: record R2D combined QA`.
- `git verify-commit --raw` returned exit 0 and a Good SSH signature for all personally checked control commits, using `jaystar524@gmail.com` and RSA key fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Product ancestry

- reserved base, initial last-good, local main, `origin/main`, and live remote main: `7fe2dd772af1d88f704a9272005b4ba557434cac`
- candidate product parent: `1dde1ef748658a11403ff4bc450af858d05f08c2`, tree `a46d403f1537583b55e7607cdde541bf1bf98dc4`, sole parent reserved base
- immutable accepted candidate and immutable staging ref: `1f23235cd60582379da5e8f1713cd25de4ced62f`, tree `b668b45fa38807de6249b1a814d3e6baf96329f5`, sole parent product parent
- allocated integration branch: `codex/r2d-rules-restarts-integration-qa-sol`
- allocated worktree: `C:/Users/joshs/Projects/tecmo-basketball-port-r2d-rules-restarts-integration-qa-sol`

The product parent and candidate both verified as Good SSH signatures by the same authorized identity and key. Direct ancestry checks passed for base to product parent, product parent to candidate, and base to candidate.

The branch-only mutation was exactly `git merge --ff-only 1f23235cd60582379da5e8f1713cd25de4ced62f` from a clean branch at the reserved base. It produced no merge commit. Main, staging, tracking refs, the live remote, and all other worktrees were not switched or mutated. No push occurred.

## Candidate ledger

The base-to-candidate ledger is exactly 17 text paths, 1,275 insertions, and 35 deletions. The product parent changes seven accepted product/test/build paths. The candidate adds the ten accepted documentation paths. `git diff --check` passed; no binary numstat entry, private path, proprietary artifact, ownership escape, or integration collision was found.

The report layer is restricted to:

- `docs/finish-tasks/R2D-rules-restarts-integration-qa/README.md`
- `docs/finish-tasks/R2D-rules-restarts-integration-qa/LINEAGE.md`
- `docs/finish-tasks/R2D-rules-restarts-integration-qa/COMMANDS.md`
- `docs/finish-tasks/R2D-rules-restarts-integration-qa/EVIDENCE.md`
- `docs/finish-tasks/R2D-rules-restarts-integration-qa/INDEPENDENT-QA.md`

The report commit is the Good-SSH-signed descendant containing this directory. Its exact SHA/tree are intentionally supplied by the terminal master handoff because a commit cannot embed its own object ID.

## Independent task lineage

Exactly one top-level projectless independent task was created after the post-fast-forward registry/collision gate:

- task ID: `019fcb82-4090-7b33-8293-7ceda46a43ba`
- exact title: `Tecmo R2D Rules Restarts Integration Independent QA — Luna Max`
- model: `gpt-5.6-luna`
- thinking: `max`
- created_at: `2026-08-04T06:42:18Z`
- host: `local`
- initial turn duration: `1,796,805 ms`
- creation fault: none
- setup fault: none
- retry/replacement fault: none
- pin: `true`, successful immediately after creation; retained for signed-tip reuse

No second Luna task or writable worker was created.
