# Scope and non-goals

## Writable scope used

Only the delegated R1 LIVE paths were changed:

- `CMakeLists.txt`, `build.ps1`
- `include/tecmo_gameplay_scene.h`, `include/tecmo_gameplay_scene_internal.h`, `include/tecmo_gameplay_live_foundation.h`, `include/tecmo_gameplay_live_proof.h`
- `src/tecmo_game.c`, `src/tecmo_flow_test.c`, `src/tecmo_cli_gameplay_core.c`
- `src/tecmo_gameplay_scene.c`, `src/tecmo_gameplay_scene_actors.c`, `src/tecmo_gameplay_scene_validation.c`
- `src/tecmo_gameplay_scene_test_state_flow.c`
- `src/tecmo_gameplay_live_foundation.c`, `src/tecmo_gameplay_live_proof.c`
- `src/asset_pack/tecmo_asset_pack_source_map.c`
- `tools/Run-GameplayMovementTests.ps1`, `tools/Run-GameplaySceneTests.ps1`
- this `docs/finish-tasks/R1-live-foundation/` directory

The new LIVE foundation/proof units are in the explicitly claimed
`tecmo_gameplay_live_*.c/.h` family. CMake and the explicit PowerShell source
list were updated in lockstep. `src/tecmo_gameplay_state.c`,
`src/tecmo_gameplay_movement.c`, and the scene orchestrator are not changed.

## Compatibility contract

`TecmoGameplaySceneLaunch.starter_binding_bound == false` is a source/default-initializer compatibility input for existing direct/test/render callers. Scene launch normalizes its stored arrays to canonical identity starters `0..4`, stores the launch as internally bound, and records the origin in the scene-owned `legacy_direct_launch` bit. That bit is not caller-settable. Legacy callers retain their accepted direct layout/cadence behavior; production preseason and season launch callers always validate and bind the selected TeamManagement starters.

Bound production/test launches use stable actor slots `0..4` away and `5..9` home, local slot `actor % 5`, and the exact Bank04 static table positions/directions/fixed-link values. Reusing those values as the post-tip stable LIVE layout is native-faithful/inferred, not a proven first-running-clock snapshot; selected roster identities and fatigue conditions remain bound by value.

## Non-goals and exclusions

- No TIP or pre-tip semantics, timing, input, winner, or presentation behavior was redefined.
- No edit was made to `src/tecmo_gameplay_scene_shots.c`, render files, `src/win32_platform.c`, accepted CPU/TGAI/TGMO payload sources, `Run-GameplayCpuSteeringTests.ps1`, `Run-Win32LaunchSmokeTest.ps1`, render checkpoint, gameplay-lab, root `README.md`, `PORTING.md`, `AGENTS.md`, or orchestration state.
- Dynamic Bank05 candidate/matchup assignment, complete CPU policy, original first-clock RAM snapshot, caller-derived shot workspaces, and original RNG are not claimed exact.
- No ROM, emulator capture, ASM/decomp artifact, or prohibited evidence artifact is committed.
- Worker tests do not constitute final acceptance; Sol owns the independent build, Win32 smoke, proof, visual review, and QA reruns.
