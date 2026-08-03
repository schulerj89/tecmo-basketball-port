# Approximations and bounded deferrals

## Clock and period boundary

The clock, period, reset, event-vector, fixed-wait, score-screen, and final
state behavior is `native_faithful`: it is a bounded C state-machine port, not
a claim of cycle-exact execution or unported presentation behavior. The exact
source spans and the tested event order are recorded in `EVIDENCE.md`.
Controller symmetry and phase-frame saturation are state-boundary contracts;
they do not claim a particular renderer, animation, or audio mixer.

## TGFT live ownership

The fatigue kernel is `native_faithful` for its cadence, active decay,
threshold, bench, cap, and recovery behavior. Its live-tick coupling is a
`native_approximation_with_justification`: the public kernel remains policy-free
over caller-provided difficulty and two 5-player active lists. This lane does
not derive substitutions, choose replacements, or claim ownership of the
production scene tick.

The current production scene supplies fixed actor slots `[0..4]` and `[5..9]`,
roster indexes, active flags, launch starter arrays, and validation. That
stable-slot boundary is recorded as a bounded native approximation; it is not
evidence of a live substitution eligibility rule or timing owner.

## TGFL predicate boundary

The original TGFL derive remains the exact pure base resolver. The separately
named caller-policy API represents only the two proven contiguous predicate /
effect tails in Bank06 `$976F-$985C`. The public parameters are generic
`shooter_predicate` and `secondary_predicate`; only zero/nonzero behavior is
claimed. No side meaning, aim, release, attempt, pose, control ownership,
scene selection, or shooter/secondary selection policy is inferred.

## Substitution and scene rescope

No live substitution or production active-lineup integration was added. The
private search found pause/substitution labels and data, but no proven live
caller, eligibility rule, or timing owner. The exact bounded rescope required
for a later integration is:

- `include/tecmo_gameplay_scene.h`
- `include/tecmo_gameplay_scene_internal.h`
- `src/tecmo_gameplay_scene.c`
- `src/tecmo_gameplay_scene_actors.c`
- `src/tecmo_gameplay_scene_court.c`
- `src/tecmo_gameplay_scene_validation.c`
- `src/tecmo_gameplay_scene_test_state_flow.c`
- `tools/Run-GameplaySceneTests.ps1`
- `src/tecmo_game.c` only if the production TeamManagement-to-launch bridge
  changes
- `src/tecmo_team_management.c` only if pregame editor behavior changes

The exact `$6023` and `$7B2E` observations are working-state/RAM starter
staging addresses. They do not establish stable-slot mapping or unported
substitution caller ownership.

## Transaction and validation boundaries

Owned TGFL and TGFT parse/load paths stage valid replacements before committing
and preserve a valid preloaded object on malformed or `NULL` replacement. A
parse/load call may use the old canonical storage as a read-input payload alias
because replacement is staged before the old object is released. Public parse
rejects payload/dependency ranges that overlap the writable assets object;
initialize/step/derive reject their writable output against both the assets
object and canonical storage. Builder payload/provenance/ROM overlaps are
rejected transactionally. These are fail-closed hardening contracts, not new
native gameplay policy.

## Accepted audit lineage

The three read-only audit reports were personally accepted and remain the
evidence basis for this lane:

- `019fc901-9608-76c1-ac13-4a5ef73f2e91` — `Tecmo R2 Clocks Periods Evidence Research — Luna Max`: exact clock/event/fixed-wait/final evidence, supported duration matrices, and the presentation boundary.
- `019fc901-d35e-7573-bf0d-36087859df58` — `Tecmo R2 Lineups Substitution Evidence Research — Luna Max`: pure base resolver, bounded contiguous caller tail, explicit slots, and no proven live substitution caller/eligibility/timing owner.
- `019fc902-0bed-7d82-aa29-acaff4d9d04e` — `Tecmo R2 Fatigue Native Audit — Luna Max`: cadence `6/4/1`, unreachable post-step cadence `6`, `0 -> 255` wrap, threshold/cap/recovery behavior, and strict object/transaction requirements.

These findings are also recorded in `LINEAGE.md` and the complete matrix in
`EVIDENCE.md`. Audio is N/A; no visual or audio proof is claimed for this
non-visible API/state lane.
