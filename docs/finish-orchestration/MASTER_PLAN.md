# Tecmo Basketball Finish Orchestration Master Plan

## Purpose

This directory is the committed control plane for finishing the native C port.
It is designed so that a replacement master can reconstruct task, round,
session, ownership, evidence, blocker, acceptance, and Git state without relying
on chat previews or uncommitted notes.

The immutable Round 0 base is
`63b29b04b1ab4745b7b8d5dd0499942d1bf8ba4e`. The master workspace is
`C:\Users\joshs\Projects\tecmo-basketball-port-master-orchestrator` on
`codex/master-finish-orchestration`.

## Definition of Finished

The port is finished only when all of the following are true:

1. Every criterion in `state/acceptance.json` is classified as
   `exact_source_pinned`, `native_faithful`, or
   `native_approximation_with_justification`; none remains `incomplete`.
2. Every accepted approximation names its evidence limit, reason, user-visible
   consequence, and why it is appropriate for a native non-cycle-exact port.
3. All discovered tasks have reached `pushed`, or have a documented accepted
   `replaced` disposition whose replacement reached `pushed`.
4. Each user-visible task has source-pinned original-game evidence, automated
   results, deterministic production-path proof, and personal Sol inspection.
5. Every round has a signed domain acceptance and a separate Sol Max combined
   integration-QA acceptance, except the coordination-only Round 0 gate.
6. The executable and semantic asset pack rebuild from documented commands.
7. Full automated, smoke, end-to-end game, and end-to-end season runs are signed
   by the final integration orchestrator.
8. Video, numbered full-resolution frames/contact sheets, and audio captures
   satisfy the proof contracts for their respective visible or audible work.
9. `main` and `origin/main` resolve to the same final SHA and the push record is
   stored in `state/rounds.json`.
10. No ROM, lifted ASM, proprietary decoded payload, private capture, save
    state, trace, or other prohibited artifact is tracked.
11. Every worker and domain/integration orchestrator is unpinned after accepted
    work is integrated and recorded. The master is unpinned only after the
    final report is published.

Compiling is never sufficient acceptance.

## Architecture and Clean-Port Rules

- Normal runtime is native C operating on game concepts and validated semantic
  asset-pack entries.
- ROM, decompilation, ASM, emulator state, captures, traces, PPU/OAM dumps, and
  reference audio are research/test evidence only.
- Importers may understand NES storage. High-level runtime code must not depend
  on emulator-shaped replay data or private local paths.
- Asset-pack readers and runtime consumers fail closed on revision, size,
  fingerprint, provenance, bounds, dependency, or cross-pack failures.
- Proprietary evidence stays ignored and local. Committed documentation records
  sanitized fingerprints, bank/address/table/routine references, commands, and
  conclusions, never original bytes or prohibited media.
- Existing debug/capture scaffolding may remain only where it is explicitly
  diagnostic and unreachable from normal play.
- Unsupported behavior is not filled with fabricated data or mislabeled as
  original behavior.

`AGENTS.md` and `PORTING.md` remain authoritative and must be read in full by
every orchestrator and worker before work begins.

## Session Hierarchy

- The master is a top-level `gpt-5.6-sol`, thinking `max`, Codex task.
- Only the master creates or replaces top-level Sol Max domain and integration
  orchestrators.
- Domain/integration Sols may create only separate top-level
  `gpt-5.6-luna`, thinking `max`, implementation/research/QA tasks.
- A domain Sol may not create another Sol orchestrator.
- The master communicates only with Sol orchestrators and never reads,
  messages, waits on, or directly evaluates ordinary Luna tasks.
- Sol orchestrators own Luna assignment, retry, revision, review, tests, proof,
  merge into their domain branch, and lineage reporting.
- Internal collaboration/sub-agent tools are prohibited for this program.
- Every active task is pinned and clearly titled with Tecmo, round, domain/task,
  role, and model tier.
- Session identifiers, worktrees, branches, pin state, lineage, failure counts,
  replacement lineage, and last-good SHAs are committed in `state/sessions.json`.

## Full Subsystem Inventory Method

Round 0 inventory is evidence-driven and never trusts titles or previews.

1. Read `README.md`, `PORTING.md`, every applicable `AGENTS.md`, source status
   strings/TODOs, tests, tools, docs, asset-pack entry declarations, menu routes,
   gameplay flows, and save formats.
2. Enumerate all Codex Tecmo tasks with app thread listing/reads. For Sol tasks,
   inspect their structured reports. Never contact their Luna descendants.
3. Enumerate Git worktrees, local/remote branches, tags, logs, merge bases,
   dirty state, and untracked proof directories without deleting anything.
4. Inspect actual commits and task documentation. Treat task titles, previews,
   and informal completion claims as descriptive only.
5. Inventory proof manifests, frame/video/audio locations and fingerprints,
   while leaving ignored private artifacts untracked.
6. Map every supported, approximate, missing, or unreachable behavior into
   `state/acceptance.json` using exactly one fidelity classification.
7. Map every implementation gap to a queue task with dependency, exclusive
   ownership, evidence requirement, and acceptance gate.
8. Adopt useful Sol work, request a structured report where absent, or record a
   superseding task. Never duplicate an already accepted main commit.

The machine-readable inventory is `state/inventory.json`. A snapshot is not
complete until its `inventory_status` is `complete` and validators pass.

## Round Strategy

- **Round 0:** control plane, coordination validation, and current-state
  inventory. The control-plane commit is the first administrative push.
- **Round 1:** critical gameplay foundation and adopted CPU/tip/input/facing
  work.
- **Round 2:** gameplay mechanics, player actions, rules, live/reference
  animation, fouls, free throws, rebounds/blocks/steals, and halftime.
- **Round 3:** season, management, team/player data, mutable statistics,
  leaders, All-Star, save/load, and completed-season progression.
- **Round 4:** opening/title/attract/menu fidelity and complete music/SFX/audio
  behavior.
- **Round 5:** remaining parity gaps, clean rebuild, end-to-end original
  comparison, end-to-end game/season QA, and release readiness.

The seed structure is revised when the evidence/dependency inventory requires
it. Rounds are dependency-aware. Concurrent tasks must have disjoint writable
ownership. Shared boundaries are sequential or owned by a designated boundary
task.

## Task Lifecycle

The canonical state machine is `state/state-machine.json`:

`backlog -> scoped -> assigned -> in_progress -> sol_review -> sol_accepted -> ready_for_round_staging -> combined_qa -> ready_for_main -> merged -> pushed`

`luna_revision` is used when Sol review returns work to the same Luna task.
`blocked`, `replaced`, `failed`, and `reopened` paths are explicit and audited.
Every transition records UTC timestamp, actor session, reason, prior state, and
new state. The transition tool rejects an unpermitted edge.

## Conflict Prevention

1. An exclusive ownership claim must exist before a domain session is created.
2. Concurrent tasks in one round may not have overlapping writable globs.
3. Shared files require a sequential shared-boundary rule or one designated
   boundary/integration task.
4. Every task records base SHA, expected parent, branch, worktree, result
   commits, and declared merge order.
5. Validators reject duplicate task/session/thread/worktree/branch IDs,
   dependency cycles, missing references, glob overlap, and active registry
   collisions. A task cannot enter an active execution state until every
   dependency has reached Sol acceptance or a later state. Reported writable
   Luna contexts participate in the same branch/worktree collision and Git
   lineage checks as Sol/master contexts; read-only Lunas keep both fields null.
6. One Sol may reuse one exact domain branch/worktree for explicitly
   dependency-ordered tasks in the same round only after the earlier task has
   left every writable, revision, review, or blocked state. Ownership claims
   remain disjoint, and concurrent writable reuse is rejected.
7. Lineage validation checks that bases and result commits exist and that each
   branch/result descends from its expected base.
8. The master does not improvise product conflict resolution. Overlap or merge
   conflicts are sent back for rescoping to the responsible Sols, or to a
   bounded Sol boundary orchestrator created by the master.

## Domain Acceptance Gate

Before a task may reach `sol_accepted`, its Sol orchestrator must commit its
task documentation contract, personally review the implementation and source
evidence, own automated QA, inspect production-path visual/audio proof, record
all Luna revision history, and report the exact accepted branch/commit. A Luna
self-certification is insufficient.

Before `ready_for_round_staging`, the master verifies only the documented Sol
sign-off, documentation completeness, ownership, lineage, and queue state. The
master does not rerun product tests.

## Round Merge and Push Gate

1. Freeze the round task set, merge order, expected parents, and staging branch.
2. Verify remote `origin/main` has not advanced unexpectedly.
3. Merge only signed Sol-accepted domain commits in declared order.
4. Create a dedicated top-level Sol Max Round Integration QA orchestrator on
   the combined staging branch. It may use Luna Max QA/proof tasks.
5. Require its signed full-suite/rebuild/video/frame/audio/cross-domain report.
6. Move the round to `ready_for_main` only after that report is committed and
   its accepted staging SHA is exact.
7. Fast-forward or merge the complete round into `main`; never cherry-pick an
   unsigned fragment.
8. Push `origin/main` once for the accepted round, without force.
9. Record pre-main SHA, merge commits, post-main SHA, remote SHA, push result,
   and acceptance timestamp.

If remote main changed unexpectedly, no push occurs. A bounded Sol integration
orchestrator reconciles the histories.

Round 0 is coordination-only: the master may run JSON/schema/ownership/lineage
validators and commit their results. It performs no product QA.

## Original-Game and ASM Evidence Policy

Every fidelity claim names what is proven and what remains inferred:

- ROM revision/fingerprint or other reference identity;
- bank and CPU address range, routine, table, selector, or queue site;
- sanitized source-map or asset-pack fingerprint;
- original frame/audio reference and timestamp where relevant;
- confidence (`exact`, `high`, `medium`, or `low`);
- runtime, test, proof-manifest, and documentation locations;
- explicit limitations and approximation boundary.

Original material is never copied into committed docs. An evidence record may
identify an ignored local reference path but must mark it `local_private` and
must not contain original bytes.

## Visual and Audio Proof Requirements

- User-visible work uses deterministic before/after production-path evidence
  whenever feasible.
- Video proof includes numbered full-resolution frames and a contact sheet.
- Manifests record base/final SHA, asset-pack fingerprint, ROM revision,
  reference source, input script, resolution, frames/timestamps, commands, and
  acceptance observations.
- Audio proof records capture command, sample rate/channels, cue/event trace or
  waveform evidence, original reference, and Sol listening notes.
- The responsible Sol personally inspects the proof and records that inspection.
- Private reference captures remain ignored. Safe generated proof may be
  committed only when repository policy permits it.

## Per-Task Documentation Contract

Every domain task commits a folder under `docs/finish-tasks/<task-id>/` before
Sol acceptance. It contains the fields and files specified in
`TASK_DOCUMENTATION_CONTRACT.md`. The queue's `task_docs_path` points to it.

## Bad-Request Recovery

The literal `{detail: bad request}` and equivalent confirmed bad-request faults
are session failures, never completion.

- The master records a Sol fault, failure count, raw signature, last-good SHA,
  retry/replacement IDs, and takeover confirmation in `state/sessions.json`.
- The master creates and pins a replacement top-level Sol Max task with the
  exact branch/worktree/task/evidence/queue state. The failed Sol is unpinned
  only after takeover is confirmed.
- Domain Sols retry the same Luna once. A second Luna bad-request creates a new
  pinned Luna Max replacement from the last committed state; failed Luna is
  unpinned only after takeover.
- Committed work is never discarded.

## True External Emergency Blockers

Routine code decisions, test failures, merge conflicts, ambiguity, and missing
proof are not user blockers. They are investigated or delegated.

Only a genuine external requirement—legally supplied reference material,
credentials, materially divergent product decision, external service, or
hardware after safe alternatives are exhausted—may create one pinned top-level
task titled exactly `Blockers we need help with`, using `gpt-5.6-luna`, thinking
`high`. Its prompt contains only concise blocker records and it performs no
implementation. `state/blockers.json` remains synchronized. The task is
unpinned when all blockers resolve.

## Recovery and Handoff

A replacement master reads, in order:

1. this plan and repository policy files;
2. all state JSON and schemas;
3. the generated dashboard;
4. current Git refs/worktrees and remote main;
5. only the active Sol orchestrator threads listed in `sessions.json`.

It then runs the coordination validator and lineage verifier. The committed
state—not chat memory—is the source of orchestration truth.
