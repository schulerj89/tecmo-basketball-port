# Scope

## Authorized writable scope

The authorized tracked scope for this worker was the following bounded list.
Paths outside it remained read-only, even when they were relevant to later
production integration:

- `include/tecmo_gameplay_state.h`
- `include/tecmo_gameplay_free_throw_lineup.h`
- `include/tecmo_gameplay_fatigue.h`
- `include/tecmo_gameplay_free_throw_projection_test.h`
- `src/tecmo_gameplay_state.c`
- `src/tecmo_gameplay_free_throw_lineup.c`
- `src/tecmo_gameplay_fatigue.c`
- `src/tecmo_gameplay_free_throw_projection_test.c`
- `src/tecmo_gameplay_scene_test_shot_clock.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h`
- `src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_fatigue.h`
- `tools/Run-GameplayFreeThrowLineupTests.ps1`
- `tools/Run-GameplayFatigueTests.ps1`
- `docs/finish-tasks/R2-clock-lineups-fatigue/**`

## Implemented worker boundary

The implementation commit changed these eight product/runner paths; this is a
smaller set than the authorized scope above:

- `include/tecmo_gameplay_fatigue.h`
- `include/tecmo_gameplay_free_throw_lineup.h`
- `src/tecmo_gameplay_state.c`
- `src/tecmo_gameplay_fatigue.c`
- `src/tecmo_gameplay_free_throw_lineup.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c`
- `src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.c`
- `tools/Run-GameplayFreeThrowLineupTests.ps1`

The clock vectors live in the already-owned state self-test in
`src/tecmo_gameplay_state.c`. No scene/core/shared gameplay source was edited.
The existing test-only projection composition remains source-compatible and
was not expanded because no production integration is in scope.

## Included behavior

- fail-closed LIVE-only possession reset, while preserving the violation path
  that transitions to LIVE before resetting possession;
- ordered clock/shot-clock/period events, fixed-wait completion, final music,
  completion, phase-frame saturation, controller symmetry, duration matrices,
  and transactional alias rejection;
- strict TGFT asset-object validation, staged parse/load replacement,
  transactional builders, cadence/active/bench/recovery evolution, and
  caller-provided 2x5 active-list validation;
- strict TGFL asset-object validation, staged parse/load replacement, the pure
  base resolver, and the separately named caller-policy tail API;
- focused mutation tests and canonical payload/fingerprint checks without a
  runtime ROM dependency.

## Explicitly excluded files and behavior

No substitutions or live active-lineup policy was invented or integrated.
Pause/substitution labels and data were not treated as a proven gameplay
caller, eligibility rule, or timing owner. Scene ownership, actor binding,
launch staging, and game-flow integration remain outside this lane.

The broader later rescope is recorded exactly in
[APPROXIMATIONS.md](APPROXIMATIONS.md) and includes the scene internal header,
scene actor/court/validation modules, scene state-flow tests/runner, and
conditional `src/tecmo_game.c` or `src/tecmo_team_management.c` only when the
specified bridge/editor behavior changes.

## Private evidence boundary

The canonical Rev1 ROM and any decompilation/capture material are test-only
inputs. Committed documentation records sanitized bank/address spans, roles,
fingerprints, commands, and bounded conclusions only.
