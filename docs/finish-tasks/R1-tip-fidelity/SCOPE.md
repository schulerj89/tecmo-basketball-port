# Scope and non-goals

## Current review status

Sol accepted the source, ABI, asset-pack, and test review for the first TIP
implementation commit `a37e10207455933be3930e90c55b10b669cb0ef3`, and accepted
its clean-commit formal proof. The post-rescope build, Win32 smoke, focused
pre-tip, and broad scene gates pass. Independent QA and Sol-branch integration
remain pending.

## Worker boundary

The implementation is confined to the delegated TPTI/scene/test/tool/docs
surface. The tracked files changed by this patch are:

- `include/tecmo_gameplay_pretip.h`
- `src/tecmo_gameplay_pretip.c`
- `src/tecmo_gameplay_scene.c`
- `src/tecmo_gameplay_scene_test_pretip.c`
- `src/tecmo_gameplay_scene_test_render_contract.c`
- `src/tecmo_gameplay_live_proof.c` (only `live_proof_advance_pretip`, the
  durably authorized fixture-only P1/Away held-B phase routing)
- `src/tecmo_flow_test.c` (only `flow_finish_gameplay_pretip`, the durably
  authorized fixture-only P1/Away cancel routing inside its existing 721-loop)
- `src/tecmo_cli_render_gameplay_checkpoint.c`
- `src/tecmo_cli.c` (the separately authorized one-line help-label change)
- `src/tecmo_asset_pack.c` (the separately authorized one-line failure-label
  change)
- `src/asset_pack/tecmo_asset_pack_gameplay_pretip.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_pretip.h`
- `src/asset_pack/tecmo_asset_pack_source_map.c`
- `tools/New-TipoffVisualProof.ps1`
- `tools/Run-GameplayPreTipTests.ps1`
- `tools/Run-GameplaySceneTests.ps1` (only the authorized strict TPTI row and
  reachable named stale-metadata check)
- this `docs/finish-tasks/R1-tip-fidelity/` directory

Authorized paths `src/tecmo_gameplay_scene_render.c` and
`src/tecmo_win32_keys.c` were inspected but did not require changes. No other
path is part of this patch.

## Compatibility and preserved boundaries

- `tecmo_gameplay_pretip_update` remains source-compatible and human-only.
  Automatic branches are available only through
  `tecmo_gameplay_pretip_update_controlled`.
- The CPU/LIVE chains through the expected parent are frozen.
- Physical Player 1 X remains the accepted production path to NES B semantics;
  the one-line proof/checkpoint diagnostics retain the existing unmapped
  literal-B evidence.
- Pre-card, close-up, toss, center camera, both visible jumpers, inward contest
  facing, the scene-owned 60-update crouch/rise/apex/fall/land arc, and the
  accepted successful frame-721 presentation/LIVE handoff remain the visible
  contract.
- The LIVE holder remains scene slot 0 or 5, selected through
  `scene_first_actor_for_team(possession)`. Native jumper slots 4/9 are never
  silently promoted to receiver/holder identity.
- Scene time advances only when the transactional TPTI state advances. A
  no-input stall at total/frame 720 is invariant-valid and repeats without
  scene-frame drift.

## Non-goals and exclusions

- No ROM execution, emulator, Lua, ASM/decompilation runtime dependency,
  capture/log/save-state dependency, or new asset family.
- No modification of CPU, LIVE production behavior, shots, gameplay
  internal/state, court, import layout, source-map header, Win32 platform/smoke,
  or root control documents. The sole live-proof exception is the exact
  fixture-only `live_proof_advance_pretip` input routing described above; no
  other function or line movement is authorized. The separate flow-test
  exception is exactly `flow_finish_gameplay_pretip`: each of its existing 721
  iterations recomputes P1 cancel from the current `JUMP_CONTEST` phase, keeps
  P2 neutral, and calls the existing runtime update; no production function is
  changed.
- No claim that raw `$0758` selector `00` maps to Away/Home, no invented
  TTDT-to-raw-`$7C48` trajectory, and no complete original tie-settlement or
  CPU decision trajectory.
- No claim that the single-byte `$E56E` record independently proves a running
  loop or handoff; those labels remain mapper-gated dynamic evidence.
- No generated screenshots, video, contact sheets, logs, proof manifests, ROM
  bytes, or build output are committed. Focused scratch remains ignored.
- Formal `New-TipoffVisualProof.ps1` passed at the exact implementation commit.
  Independent QA and Sol-branch integration remain outside this worker's
  closure and are still pending.
