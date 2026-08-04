# Scope and completed rescope

## Granted authority

Good-SSH-signed control
`de316a73b1f1814afeaf7ae904a5b3a6a22d578d` granted writes only to:

- `docs/finish-tasks/R2-gameplay-presentation/**`;
- `src/tecmo_cli_render_gameplay_checkpoint.c`, limited strictly to
  `TecmoCliGameplayCheckpointConfig`,
  `parse_gameplay_render_checkpoint_mode`, and
  `run_gameplay_shot_checkpoint`; and
- new `tools/Run-GameplayPresentationTests.ps1`.

The sole persistent implementation/revision worker produced signed candidate
`4cb0c43bcd4c7ca111c996b3788e1bd00a734424`, tree
`3bd5b4874eca46ff7ad771041e96946c3b08f233`, with sole parent
`ed060720a98b790f98591af363a490a0e0816018`. After personal QA and terminal
PASS, Sol integrated that exact object by `git merge --ff-only`.

All other product, header, source, tool, test, build-orchestrator, asset,
importer, source-map, global-document, main, staging, origin, and push mutations
remained prohibited and untouched.

## Audited surface

The completed read-only audit covered:

- court/action animation and action timing;
- camera follow, settle, projection, and travel language;
- actor sprite/pose transitions, facing, draw order, and clipping;
- dunk and referee cutaways;
- HUD and overlays;
- both screen edges and fine-scroll margins;
- violation, foul, free-throw, and restart presentation;
- jump shots, rim rattle, dunks, layups, numeric close variant 1, and ordinary
  two-point boundaries;
- current tests, runners, accepted reports, ignored historical proof, bounded
  original-reference evidence, and ownership/collision state.

## Exact completed implementation slice

The signed grant authorized and the candidate changed exactly two product/tool
paths and no others:

1. `src/tecmo_cli_render_gameplay_checkpoint.c`

   Limit edits to:

   - `TecmoCliGameplayCheckpointConfig`;
   - `parse_gameplay_render_checkpoint_mode`;
   - `run_gameplay_shot_checkpoint`.

   Added one mode family, `gameplay-layup-frameN`, for the bounded TGCS
   variant-2 route. The fixture must:

   - complete the existing production pre-tip/live handoff;
   - use the existing away holder attacking the left hoop in one legal,
     uncontested interior position;
   - establish coherent actor anchor, held-ball position, and camera settle in
     the same manner as the accepted shot checkpoints;
   - launch through normal player input;
   - never write `shot_kind`, `close_shot_variant`, pose, schedule, outcome,
     score, claimant, or settlement state directly;
   - verify that the production selector chose
     `TECMO_GAMEPLAY_SCENE_SHOT_LAYUP` and numeric variant 2;
   - reject zero and out-of-range frame suffixes transactionally.

2. `tools/Run-GameplayPresentationTests.ps1` (new focused runner)

   The runner exercises only the new layup modes and existing executable
   interfaces. It:

   - renders two deterministic passes with stable names
     `gameplay-layup-frameNN`;
   - covers the 16 source-pinned TGCS pose steps plus the first terminal/tail
     boundary exposed by the chosen deterministic fixture;
   - requires the expected `shot=layup` state during the active route and the
     exact observed terminal/tail state at the boundary;
   - rejects malformed, frame-0, and upper-bound-plus-one modes;
   - records branch, HEAD, clean state, mode, frame, image dimensions, and
     SHA-256 for every artifact in an ignored manifest;
   - requires pass-one/pass-two PNG and state equality;
   - rejects collapsed entry/transition/terminal visual sentinels;
   - leaves full-resolution human review to the Sol and independent terminal
     QA.

The existing broad scene, render-contract, shot, camera, court, HUD, violation,
rules/restarts, and warning-clean build suites remain mandatory validation, but
their files were not writable in this grant.

## Classification of the completed slice

- TGCS variant-2 step/phase data and pose resolution:
  **exact-source-pinned**.
- Existing scene/input/camera/HUD/render composition:
  **native-faithful within the named fixture**.
- Geometry/stable-sample selection, ball arc, outcome, claimant, and landing:
  **native-approximate**.
- Original trigger policy, full layup object semantics, general directions and
  profiles, contact meaning, both screen edges, and emulator parity:
  **incomplete**.

The proof preserves those labels in logs, manifests, docs, and handoff.

## Explicit exclusions preserved

The grant and accepted result exclude all edits to:

- `include/tecmo_gameplay_scene*.h`;
- `src/tecmo_gameplay_scene.c`;
- `src/tecmo_gameplay_scene_shots.c`;
- `src/tecmo_gameplay_scene_render.c`;
- actor, validation, state, court, camera, video, HUD, TGCS/TGJS/TGDK/TGSR,
  violation/referee, free-throw, defense/contact, and audio modules;
- `src/tecmo_gameplay_scene_test_*.c` and their orchestrator/header;
- `tools/Run-GameplaySceneTests.ps1` and every existing runner;
- `CMakeLists.txt`, `build.ps1`, asset-pack/import/source-map files, assets, and
  global docs;
- main, staging, merge-order, origin, push, cleanup, deletion, and archive
  actions.

No pass, defense/contact, ordinary two-point, numeric-1, camera, clipping,
renderer, violation, restart, free-throw, or new-asset implementation is part of
this bounded result.
