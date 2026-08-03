# R1A CPU + LIVE integration QA

## Decision

**ACCEPTED — TERMINAL P0/P1/P2 = 0/0/0.**

The accepted integration candidate is merge commit
`351f446dddc96c34c838c5a9642a0be9d7f1411e`. Its exact parents are:

1. intermediate R1A/R3A merge
   `f98fea320bf2340e0c6c9b226cfe6caa63196dd7`; and
2. accepted current-main R4A tip
   `bcacd5b6963f4db1a92c8db9b9770413505a0e98`.

The R1A/R3A sides changed 43 and 9 paths from their common base
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`; their path intersection was
empty. After accepted R4A advanced current main, the R1A side and R4A side
changed 43 and 18 paths from common base `dd096cb...`; that intersection was
also empty, and Git's merge-tree completed with exit 0. Both merges were made
only on `codex/r1a-cpu-live-integration-qa-sol`. Neither `main` nor
`origin/main` was mutated or pushed by this lane; both remain `bcacd5b...`.

CPU and LIVE compose at the reconciled tip without an observed product defect
or cross-slice regression. The decision covers the accepted CPU ordered slice
through `ad0f005673692b04772bce3c3b4d3ac4b2624731`, the accepted LIVE slice
through `222d75cfafa9153db1eb44492bf557f11b1a9091`, and their clean composition
with accepted R3A at `dd096cb...` and accepted R4A at `bcacd5b...`. TIP remains
outside R1A.

## Gate result

| Gate | Personal result |
|---|---|
| Exact branch, parents, ancestry, ownership, and index/tracked state | PASS |
| R1A/R3A overlap and conflict audit | PASS: 43 vs 9 paths, overlap 0 |
| R1A/R4A overlap and conflict audit | PASS: 43 vs 18 paths, overlap 0, merge-tree exit 0 |
| No prohibited tracked ROM/decomp/private-capture artifact | PASS |
| Warning-clean full repository build at merged tip | PASS |
| CPU focused Rev1 gate | PASS: 680 aligned commands, 24 handlers, 17 ROM mutation rejections |
| Gameplay movement | PASS: exact TGMO spans, transactional rejection, live integration, 7 ROM mutations |
| Production preseason/season flow and CLI boundaries | PASS |
| Production season suite | PASS: native handoff/result flow and 13 pixel checkpoints |
| Accepted R4A music/frontend/gameplay audio gates | PASS |
| Full gameplay scene suites | PASS |
| Fresh merged-tip ignored `-RequirePass` proof | PASS |
| Win32 production launch smoke | PASS |
| Manifest/log/inventory/media hash audit | PASS |
| Numbered-frame/contact-sheet inspection | PASS |
| CPU-to-LIVE source and invariant audit | PASS |
| Same Luna exact-control disposition | PASS: evidence-invalidating P0/P1/P2 = 0/0/0 |
| Same Luna intermediate `f98fea3...` review | PASS as intermediate; correctly found later-main reconciliation required |
| Same Luna terminal `351f446...` review | PASS: P0/P1/P2 = 0/0/0 |

## Personal integration conclusion

- Production preseason and season entry points bind selected Team Management
  starters by value before scene launch. Duplicate or out-of-range starters
  reject before runtime mode changes.
- Stable scene slots preserve selected roster and fatigue identity. Controller,
  holder, possession, orientation, and opposing links remain synchronized by
  the fail-closed ownership invariant.
- LIVE consumes the accepted CPU formation/play-state/one-step/shot contract
  from one immutable post-human ten-actor snapshot. Actor, CPU, foundation,
  ball, and supported-shot mutations commit only after the full candidate
  validates.
- Possession/pass/switch transitions invalidate stale command metadata and
  synchronize the new holder. A supported close-shot request launches once;
  unsupported playback is explicitly deferred/non-launch.
- The sustained bound running-clock regression executes 120 outer updates,
  traverses more than two game-clock seconds at the native 45-frame divider,
  preserves roster/condition/ownership identity, and permits at most one action
  launch per outer update.
- Same-pack TGAI/TGMO dependencies, ROM provenance, source spans, formation
  bounds, parser/input mutations, and proof inputs fail closed. Both build
  source lists retain the LIVE units after the R3A merge.
- The accepted R4A audio side has no writable-path overlap with R1A. Its music,
  frontend-audio, and gameplay-audio regression gates pass at the terminal
  combined tip, and the scene proof's gameplay-audio coverage remains green.

## Exactness boundary

Exact/source-pinned facts remain limited to the documented Rev1 identity,
CPU/source spans, 680 five-byte commands, 24 handlers, fixed-link bytes,
Bank03/Bank04 staging bytes, 46 source-pinned formation starts, and accepted
TGAI/TGMO payload identities.

The following remain approximations, incomplete work, or out of scope:

- reuse of exact static Bank04 actor data as the stable post-tip LIVE layout;
- dynamic Bank05 candidate, matchup, reset, and retarget policy;
- the original first-running-clock RAM snapshot;
- caller workspaces, CPU shot RNG, make/miss policy, and unsupported playback;
- unfinished exact TIP behavior; and
- pixel, team, view, schedule, or timing parity between the immutable original
  reference and native event proof.

## Records

- [COMMANDS.md](COMMANDS.md) records personal commands and results.
- [EVIDENCE.md](EVIDENCE.md) records proof hashes, artifact/visual audits,
  source evidence, and integration invariants.
- [INDEPENDENT-QA.md](INDEPENDENT-QA.md) preserves the single Luna lineage,
  findings, controls, and terminal disposition.
- [LINEAGE.md](LINEAGE.md) records temporal refs, merge/control state,
  diagnostics, ownership, and signing.

## Personal signature

- Decision owner: `S-SOL-R1A-INTEGRATION-QA-001`
- Task/claim: `R1A-INTEGRATION-QA` / `OWN-R1A-INTEGRATION-QA`
- Decision: `ACCEPT`
- Date: `2026-08-03`
- Merged candidate signed: `351f446dddc96c34c838c5a9642a0be9d7f1411e`
