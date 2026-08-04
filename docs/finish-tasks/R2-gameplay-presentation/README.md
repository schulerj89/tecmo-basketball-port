# Tecmo R2 Gameplay Presentation

Status: complete. The read-only audit was reconciled at immutable base
`ed060720a98b790f98591af363a490a0e0816018`, the exact signed implementation
grant was exercised through the sole persistent Luna worker, and accepted
product candidate `4cb0c43bcd4c7ca111c996b3788e1bd00a734424` was integrated
into `codex/r2-gameplay-presentation-sol` by strict fast-forward.

The audit found a coherent but deliberately bounded native presentation:

- court, camera, movement, pose, HUD, referee, close-shot, jump-shot, and dunk
  primitives are source-addressed and validated at their existing contracts;
- the live composition of those primitives is native-faithful only within the
  named routes and checkpoints;
- shot selection, ball arcs, outcome/caller policy, CPU action policy,
  contact/foul behavior, several HUD adapters, and framebuffer clipping versus
  NES PPU/OAM behavior remain native approximations;
- pass and defense-action animation, ordinary two-point presentation, numeric-1
  full semantics, layup visual proof beyond the named fixture, general
  edge/action proof, complete free-throw presentation, original live OAM
  priority, and broad original frame parity remain incomplete.

No evidence supports an emulator-perfect, cycle-perfect, or generalized visual
parity claim. Existing native hashes are deterministic native checkpoints only.

## Completed bounded result

The implementation adds only:

- `gameplay-layup-frameN` parsing and execution inside the three authorized
  symbols in `src/tecmo_cli_render_gameplay_checkpoint.c`; and
- the new focused runner `tools/Run-GameplayPresentationTests.ps1`.

Frames 1 through 16 expose the exact-source-pinned TGCS numeric-variant-2 pose
steps. Frame 17 is the first deterministic terminal/tail boundary. The fixture
sets only the granted actor/anchor, holder/ball, and camera-settle inputs, then
uses normal production cancel input. It never authors shot kind, variant, pose,
schedule, outcome, score, claimant, settlement, or layup facing. Runtime checks
require production selection of layup and numeric variant 2 while active, and
`shot=none` at frame 17.

The parser transactionally rejects malformed forms, frame 0, leading-zero
spellings, and frames above 17. No gameplay scene, renderer, camera, HUD,
shot-asset, rule, restart, audio, build-orchestrator, or existing-runner source
was changed.

## Reconciled and implemented seam

The completed seam is an input-driven, away-left
layup presentation checkpoint and focused proof runner. It reuses the current
production scene, TGCS variant-2 poses, renderer, camera, HUD, and asset pack; it
does not change production scene/state logic, shot schedules, trajectories,
renderer behavior, assets, source maps, or build registries.

The exact grant, paths, symbols, exclusions, proof results, and worker lineage
are recorded in `SCOPE.md`, `EVIDENCE.md`, and `LINEAGE.md`.

## Acceptance

- Warning-clean canonical and fresh CMake builds passed for both native targets.
- The focused proof produced two equal 17-frame passes, six strict negative
  cases, 640x480 images, 17 noncollapsed unique frame hashes, and a clean
  branch/HEAD manifest.
- The complete gameplay-scene proof and nine neighboring focused suites passed.
- Sol reviewed all 17 frames at original 640x480 resolution.
- The sole independent terminal Luna returned PASS with P0, P1, P2, and P3 all
  `none` and no actionable issue.

## Evidence classes

- **Exact-source-pinned:** revision-locked bytes/tables or exact pure primitives
  with a bounded caller contract.
- **Native-faithful:** native composition that consumes those primitives without
  inventing a stronger source claim.
- **Native-approximate:** explicit substitutions, host rendering behavior, or
  policies whose original caller inputs are unavailable.
- **Incomplete:** behavior or proof not established by the current sources,
  tests, or accepted artifacts.

See `SCOPE.md`, `EVIDENCE.md`, and `LINEAGE.md`.
