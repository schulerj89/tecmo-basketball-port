# Implementation notes

## Launch and roster binding

`gameplay_bind_session_starters` in `src/tecmo_game.c` validates the existing TeamManagement session, selected distinct team IDs, five in-range unique starters per side, and copies the arrays by value. `launch_preseason_gameplay` and `launch_season_gameplay` fail closed before mode/scene mutation when validation fails.

`scene_launch_prepare` and `scene_starter_binding_valid` normalize unbound direct callers to identity `0..4` and preserve the internal legacy origin. Bound launches validate duplicates/ranges transactionally, bind local slots to selected roster values, widen player lookup to all 12 TTDT entries, and preserve the selected fatigue condition.

## LIVE foundation

`tecmo_gameplay_live_foundation.c` owns `TecmoGameplayLiveFoundation` around the accepted CPU play state. Initialization performs adapter initialization, actual formation selection, exactly one `play_state_initialize`, then copies source starts and metadata. Validity checks the contract tag, flags, actual source-pinned formation identity, aligned/decodable evolving offsets, exact links, `{2,7}` seeds, synchronized primary/defender, holder/possession/team coherence, controller routing uniqueness, strict target/direction sentinels, shot flag combinations, and serial contract.

Synchronization is restricted to LIVE or the explicit post-handoff boundary. It records the real holder, updates both foundation and play-state roles, tracks orientation/controller changes, and invalidates old source targets/directions on a real role transition as a native-faithful safety policy. Adapter observation counters wrap modulo `2^32`; accepted CPU `step_serial` retains natural `uint16_t` wrap.

## Scene transaction and human continuity

`scene_update_ai` stages foundation synchronization, play state, CPU metadata, TGMO movement, ball attachment, and shot decision in candidates. A movement boundary latch suppresses the shot request for that tick. Unsupported existing playback is recorded as explicit deferred/non-launch rather than silently discarded or made fatal. Human TGMO input retains one-update latency, contradictory-axis neutral handling, offensive A pass, defensive A nearest switch, swapped controller-team routing, and existing B behavior.

`scene_ownership_valid` checks canonical launch arrays against actor team/local slot/profile/condition and checks LIVE foundation ownership, controller observations, holder/primary/defender coherence, and each CPU actor's exact fixed link.

When an accepted play step supplies a direction without a source target, the
owned TGMO composition chooses the nearest deterministic coordinate inside the
playable court polygon whose TGAI octant equals that direction, with
deterministic distance/tie-break ordering. This synthesized target is native
adapter policy, not an original command argument. If an outward source
direction at an edge or corner has no legal playable target, the source
direction is preserved and TGMO application is inert/deferred without failing
the scene transaction or fabricating an out-of-court target.
When a source target references an actor, LIVE follows that actor's current
coordinate on every immutable post-human snapshot/tick; original Bank05
dynamic retarget/matchup semantics remain incomplete/unproven.

## Tests and provenance

`src/tecmo_gameplay_scene_test_state_flow.c` restores the full accepted pre-tip, render-contract, shot-clock, and state-flow orchestration and adds named bound/unbound, formation, lifecycle, phase-safety, transactionality, serial, target/direction, human, shot, and ownership regressions. `src/tecmo_flow_test.c` covers both production launch callers, high roster indices `5/6/10/11`, fatigue conditions, invalid session transactionality, and selected lineup propagation.

The accepted CPU source-map object retains its old isolated/legacy values. Bound normal-flow behavior is in the additive `live_foundation_integration` sibling and is consumed only by the LIVE scene wrapper.

`tecmo_gameplay_live_proof.c` and `Run-GameplaySceneTests.ps1` provide the
actual draft proof record: seven events, two repeats, 640x480 numbered frames,
1920x1440 contact sheets, two mandatory native MP4s, validated original CPU
reference manifest, full actor JSON, exact-once visible close-shot playback,
and path/byte/SHA artifact inventory validation.
