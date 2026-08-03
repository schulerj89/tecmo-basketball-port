# Implementation and changed seams

## Review gate

Sol accepted the source/ABI/pack/test review for the first TIP implementation
commit. The post-rescope build, explicit console-flow and GUI/console Win32
smoke, focused pre-tip harness, and broad scene suite all passed; clean-commit
formal proof and independent QA remain pending. The latest broad DRAFT is
`build/live-proof-20260803T205847090Z`.

## TPTI-2 payload contract

The stored `gameplay/pre-tip` payload is 7680 bytes with 512-byte header,
29 source records of 32 bytes, and the following semantic layout:

| Region | Offset | Size |
|---|---:|---:|
| Header | 0 | 512 |
| Source records | 512 | 928 |
| Descriptor/encoded/decoded/palette/wait/sequence/strings/pointers/charmap/tiles/selectors | 1536 | through 4448 |
| Existing close-up family | 4448 | through 5696 |
| Existing TIP setup family | 5696 | through 6144 |
| Launch bridge/handoff/orientation | 6144 | through 6508 |
| TPM2 mechanics block | 6560 | 96 |
| Validated zero padding | 6656 | 352, through 7007 |
| First separately stored new exact source | 7008 | actor dispatcher onward |
| New exact records | 7008 | through 7679 |

The gap `6656..7007` is fully validated zero padding. Header field 204 and
`EXACT_SOURCE_OFFSET` both equal `7008`; no bytes between the TPM2 block and
the first separate source escape validation. The capture/error and opposing
actor spans intentionally point into existing close-up/TIP setup storage, so
the source-containment and alias/bounds negatives cover both overlap and
separate-source rules.

The canonical ordinary full-payload identities are:

```text
size       7680
FNV-1a32   28910BC1
FNV-1a64   7EA1596E8DFAC0C1
TPM2       offset 6560, size 96
TPM2 FNV32 3572752A
TPM2 FNV64 A52B415F53DA85CA
```

`TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32` retains its repository-wide
ordinary stored-payload meaning. It is not a masked/self-referential hash.
Reserved header bytes 208..219 are zero and are covered by ordinary hashing;
the tooling also rejects stale/ordinary-metadata mutations transactionally.

The TPM2 block stores input mask `$40`, no-sample error `12`, max sampled
error `11`, automatic threshold `$3D/1F/2`, claim minimum `$3A`, claim
limit `$3A`, actor jump-commit state `$0B`, slot-10/global claim-commit state
`$17`, opaque selector seeds 7/2, `$A2D5`, `$CD96`/22, and `$E56E`/1. Cached
threshold and claim-limit fields are cross-checked against storage and
canonical constants on every asset validation.

## Native state seam

`src/tecmo_gameplay_pretip.c` owns transactional candidate updates:

- `assets_valid` validates the full payload, TPM2, all cached mechanics fields,
  strict same-pack TGPL/TTDT/TMUS/TWAR/TGJS-2/CHR dependencies, source records,
  and exact TIP_INPUT fingerprints.
- `tip_error_for_sample`, `sample_tip_controlled`, and `tip_rng_mix` preserve
  the bounded capture/error/RNG seams. Only contest updates `0..29` sample or
  mix; later visible updates do not mutate the capture clock/RNG.
- `tip_automatic_threshold_met` uses the strict B05 `BCS` relation (`>`).
  `tip_automatic_target_frame` calibrates only the accepted bounded 20/21/22
  dynamic classes.
- `tip_commit_jumper` keeps raw `$048F`-analogue claim height separate from
  genuine Q8 visual velocity/altitude. Automatic raw heights are explicitly
  approximate and preserve the accepted CPU-vs-CPU Away outcome without raw
  selector encoding.
- `tip_claim_ready` rejects ball-under-jumper underflow and accepts only
  `ball_high >= claim_height` with difference `< $3A`; `tip_try_resolve_claim`
  defers exact equality and resolves only after capture completion.
- `tip_update_altitudes` and `tip_expected_altitude` use each jumper's elapsed
  visible age (`presentation_age - commit_frame`, capped at 30), including
  late automatic commits and LIVE treated as age 60.
- `tecmo_gameplay_pretip_state_validate` rejects early/fabricated resolved or
  deferred claims, wrong/not-ready claimants, completed-ready unresolved
  states, invalid sample/error ranges, unresolved claimants, and pre-contest
  automatic request flags. No-input unresolved and equal deferred states remain
  valid where their invariants permit.
- `tecmo_gameplay_pretip_update` remains the human-only compatibility wrapper;
  `tecmo_gameplay_pretip_update_controlled` latches automatic requests and
  commits the candidate transactionally. A rejected update leaves the source
  state unchanged.

## Scene seam

`src/tecmo_gameplay_scene.c` keeps the visible scene arc independent of raw
claim/Q8 diagnostics. It routes B by launch team, requests automatic samples
only for unassigned teams, increments `scene->frame` only when total TPTI time
advances, and leaves a stalled frame-720 scene unchanged on repeat.

At a resolved handoff, the claimant is obtained through
`tecmo_gameplay_pretip_claimant_jumper`, mapped to the validated visible actor,
and its team becomes possession. The accepted holder is then derived as
`scene_first_actor_for_team(possession)` (0 or 5). Raw `$0380/$037F` values and
jumper slots 4/9 never become receiver identity.

The scene-owned crouch/rise/apex/fall/land arc remains unchanged. The ball-X
bridge is smooth and bounded: center through capture completion, then one
pixel per presentation update, capped at eight pixels. The eight changed
checkpoint hashes are documented in [TESTS.md](TESTS.md); pixel differences
are ball-only.

## Changed functions and files

| File | Main changed seams |
|---|---|
| `include/tecmo_gameplay_pretip.h` | TPTI-2 constants, raw/Q8 state fields, controlled API, state contract. |
| `src/tecmo_gameplay_pretip.c` | Asset validation, capture/automatic/commit/claim/RNG state machine, altitude bridge, state validation, self-tests. |
| `src/asset_pack/tecmo_asset_pack_gameplay_pretip.c/.h` | 29 records, TPM2 builder, layout, overlap/padding, ordinary fingerprints, TGJS dependency. |
| `src/asset_pack/tecmo_asset_pack_source_map.c` | TPTI-2 roles, mechanics labels, source provenance, false-friend/incomplete boundaries. |
| `src/tecmo_gameplay_scene.c` | Controlled update routing, skip fixture normalization, claimant-team/holder handoff, stall clock, smooth ball X. |
| `src/tecmo_gameplay_scene_test_pretip.c` | Automatic vectors, sample/error separation, bounded helper, no-progress/no-input, late altitude, handoff/holder/ball-X regressions. |
| `src/tecmo_gameplay_scene_test_render_contract.c` | Skip-PRETIP validation and team-routed B for the LIVE HUD probe only. |
| `src/tecmo_cli_render_gameplay_checkpoint.c` | Automatic raw sample-frame versus max-error expectations. |
| `src/tecmo_cli.c`, `src/tecmo_asset_pack.c` | Authorized active TPTI-2 labels only. |
| `tools/Run-GameplayPreTipTests.ps1` | TPTI-2 identity, PS5.1-safe hashes, dependency/mutation negatives, changed visual hashes. |
| `tools/Run-GameplaySceneTests.ps1` | One strict TPTI-2 row and reachable OR-based stale-TPTI-1 metadata check only. |
| `tools/New-TipoffVisualProof.ps1` | TPTI-2/FNV32+64 identity and bounded sample/error proof metadata. |
