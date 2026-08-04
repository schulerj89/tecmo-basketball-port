# Authority, ownership, and lineage

## Authority

- Master task: `019fc5d4-f360-78b3-b2a6-c8bae92df690`.
- Sol task: `019fcae1-d939-7dd0-a839-6599e2efc362`.
- Sol session: `S-SOL-R2-RULES-RESTARTS-001`.
- Claim/lane: `OWN-R2-RULES-RESTARTS` / `LANE-R2-RULES-RESTARTS`.
- Good-signed master reservation:
  `99e7183fb9816b33db3bd3dea205e1db77b3671b`.
- Sol branch/worktree: `codex/r2-rules-restarts-sol` at
  `C:\Users\joshs\Projects\tecmo-basketball-port-r2-rules-restarts-sol`.

## Signed control checkpoints

| Commit | Durable decision |
| --- | --- |
| `dc666278bf99109a91f95050de75202f5d71f505` | Assign R2 rules/restarts. |
| `ccea216b3c920469562d1946cb8871d3dbd3bced` | Accept clean takeover and three-auditor registry. |
| `b46b6347bd880d5e8f75b781a9b44a595f86d449` | Grant bounded production/audio/test seam. |
| `2cf7df3f57072219cd379fa318d7393261635ff1` | Register sole persistent implementation worker. |
| `c1eecd14a5f8ecf6fd65ab20f6ed39de88884343` | Grant first stale state-flow timing correction. |
| `bb461df957e798b5785fc6fc86fe16d7289d5024` | Grant two further timing corrections and build.ps1 registration. |
| `27f8a7731c6e2d4d37dd6f616ee635c93bad29fa` | Accept worker candidate and authorize Sol ff-only integration. |
| `513fd064480017b4750c68aa3ffe347158e73f0a` | Record exact Sol integration and start sole terminal QA. |
| `6028f997ee890fbfe5c329a2331d0bacf776e827` | Grant three exact wording-only corrections for the active cue-status contract. |

Each cited control commit was reported Good-SSH-signed. Sol personally
reverified the two integration/QA controls before acting.

## Task registry

| Role | Task | Boundary/result |
| --- | --- | --- |
| Original ASM/native evidence auditor | `019fcae5-dd40-73d3-9b9e-de84fd76f39d` | Projectless gpt-5.6-luna/max, read-only, completed and pinned. |
| Architecture/ownership auditor | `019fcae5-e369-7802-919f-5e166ce2a760` | Projectless gpt-5.6-luna/max, read-only, completed and pinned. |
| Test/proof-gap auditor | `019fcae5-eac8-7661-86e4-92f6e91e9c0b` | Projectless gpt-5.6-luna/max, research-only; later partial stop explicitly made no terminal-acceptance claim; pinned. |
| Persistent implementation/revision worker | `019fcb05-7013-7453-87ff-2d92d3ad2b32` | gpt-5.6-luna/max; branch `codex/r2-rules-restarts-luna`; exact signed path grants only; pinned. |
| Sole independent terminal QA | `019fcb44-0f91-7632-9b25-88e51b505ce3` | Top-level projectless gpt-5.6-luna/max; created once, pinned, `ACCEPT`, no P0-P3 findings. |

The three research auditors were never promoted into the terminal-QA lineage.
The terminal task was collision-gated, created exactly once, and had zero
creation, pin, retry, replacement, or duplicate faults.

## Candidate and Sol integration

Worker branch/worktree:
`codex/r2-rules-restarts-luna` at
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-rules-restarts-luna`.

The worker produced Good-SSH-signed candidate
`1dde1ef748658a11403ff4bc450af858d05f08c2`, tree
`a46d403f1537583b55e7607cdde541bf1bf98dc4`, sole parent
`7fe2dd772af1d88f704a9272005b4ba557434cac`. The signing identity was
`jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

After signed master acceptance, Sol ran only:

```text
git merge --ff-only 1dde1ef748658a11403ff4bc450af858d05f08c2
```

The Sol tip/tree then equaled the worker candidate exactly and remained clean.
No main/staging checkout, merge, commit, rebase, or push occurred.

All listed tasks remain pinned pending durable master acceptance.
