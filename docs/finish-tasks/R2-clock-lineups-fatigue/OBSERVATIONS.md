# Observations

- The late-clock event is ordered before expiry events at the tested 0:01
  boundary. Simultaneous shot/game expiry therefore produces four ordered
  events, including both expiry SFX events.
- Fixed-wait entry at 0:01 with shot clock 10 emits the late-clock and
  game-clock expiry events. The subsequent wait/completion edge is empty in
  the tested vector.
- Exempt shot-clock expiry carries detail `1`; the non-exempt vector carries
  detail `0`.
- Regulation and tied-overtime duration matrices were retained together.
  Final music and the final `GAME_COMPLETE` vector are covered at the state
  boundary; visual/audio playback is not claimed.
- A valid `LIVE` state is required for public possession reset. The established
  violation transition enters `LIVE` before the reset call, so that path is
  preserved.
- Fatigue reloads cadence `6/4/1` and decrements in the same public step.
  Consequently post-step cadence `6` is rejected, while active countdown zero
  remains valid and wraps byte-wise to `255` on a cadence-triggering step.
- Fatigue state evolution remains policy-free over arbitrary unique caller
  active lists. The two team recovery paths, threshold behavior, bench zero,
  and `+4` caps are tested without adding substitution policy.
- TGFL base derivation is intentionally separate from the caller-policy tail.
  The caller-policy test covers all 720 binary predicate combinations and an
  explicit `0xFF` nonzero vector; this does not prove production selection.
- Strict TGFL/TGFT validation checks canonical descriptor metadata and bytes
  pointers as well as storage/scalars/fingerprints. Structured corruption,
  object/storage overlap, and writable-output aliases fail without mutation.
- Owned asset builders stage payload/provenance data and commit only after
  validation. The TGFL builder enforces the complete canonical Rev1 SHA-256
  internally; the payload schemas and fingerprints remain unchanged.
- The focused runners use the canonical Rev1 ROM only as test input. Runtime
  modules remain independent of ROM, decompilation, and capture files.

## Sol bounded visual observations

These observations belong only to Sol's deterministic production proof and do
not expand the state/API behavior classifications:

1. Frame 0 is intentionally blank before the violation referee appears.
2. Frames 9 through 27 show a clean referee entrance and distinct gesture
   progression; frame 27 is the settled raised-hand pose.
3. Frames 27 through 80 remain visually settled while the presentation timer
   advances.
4. Left/right free-throw screens are distinct mirrored court orientations;
   shooter, lane actors, benches, basket, ball, and HUD are visibly placed.
5. No clipping, HUD overlap, corrupt pixels, missing actors, or orientation
   collapse was observed.
6. No period/halftime/final render mode is owned here. Those mechanics are
   state/event-only in this scope and are proved by gameplay-state vectors plus
   scene regression, not a new visual-semantic claim.

Audio is N/A because this boundary changes no audio semantics. These v1
observations are bound to proof-source HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`; Sol's v2 proof at
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` re-inspected the same bounded
observations and all v2 render artifacts matched v1 byte-for-byte. No period,
halftime, or final render ownership is claimed. The independent QA task's
historical P2-only FAIL was resolved by the closure re-audit at
`1567f284ff48a2334fb6a9bd82d00aadf0cdb373`, which passed with no remaining
actionable findings and no P0/P1. The auditor did not rerun product tests or
personally visually accept frames; Sol's v2 execution and visual acceptance
remain authoritative. Dynamic substitutions and production active-lineup
ownership remain incomplete, and no exact visual/audio semantics claim is
added. QA remains pinned only until Sol captures the closure-doc consistency
recheck.
