# Pass defender handoff evidence

## Shared visible pass lifecycle

Ordinary bound human passes and the bounded autonomous action-`$21` CPU route
now share one actor-neutral transport rather than teleporting possession:

- Bank06 opcode 9 is the already-owned writer of actor-local `$046E`; only a
  naturally emitted value `$21` may start an autonomous pass. The native code
  does not force the Bank04 `$A05F` record or route CPU intent through NES A.
- Canonical Rev1 Bank06 `$8FC5-$8FE7` writes `C9=$21` to the current `$0308`
  primary's `$046E` (captured actor 9), while `$8284-$82A5` excludes `$0308`
  and `$0309` from ordinary descending `$057C` actor-state dispatch. Bank05's
  selected-primary pointer table consumes index `$21` at `$89D7`. The lifted
  Bank06 helper label is four bytes early; canonical Rev1 bytes win. `$89D7`
  seeds packed `$0458=$32` and changes the selected actor to state `$0F`;
  state `$0F` dispatches to `$8695`, not state `$0A`. A separate paused trace
  displayed `$24` beside `$89DB`, but FCEUX effective-value annotations can
  reflect later register/memory state; it neither disproves this bounded chain
  nor proves the absence of an intermediate write.
- `$8999` is not a tight loop. At bytes >=`$10` it jumps to `$9C29`, which
  subtracts `$10` from the high nibble while preserving the low nibble. The
  captured route then advances below `$10` through raw `$0385/$0391`, yielding
  `$32->$22->$12->$02->$03->$04`. Native C preserves that capture-bounded
  cadence; it does not claim general ownership of `$0385/$0391`.
- `$86A8-$86B7` releases only when the complete packed byte reaches `$04` and
  jumps directly to `$B074`. This observed route arrives with slot-10
  `$0478=$13`; `$B074` is shared and is not a state-`$03`-only entry.
- `$B074-$B0FD` locks `$037F[$030A]` as receiver and swaps the
  `$000E/$037F`-shaped side roles at launch. The `$0308`-shaped primary and
  native `ball_holder` remain the passer until catch.
- Bank05 `$B1E7/$B500` owns the original five-substep flight scheduler, and
  `$B500->$BD6E` advances two uint16 fixed-point accumulators with wrap/carry,
  then performs six logical 16-bit shifts. Native C locks that portable
  arithmetic in isolation, but current scene coordinates remain the bounded
  interpolation adapter because their launch solver/table inputs are unowned.
- Genuine Bank05 `$B24F` begins `AC 0A 03`. The captured actor-2 pass locks
  offense-side raw `$037F[0]=4`, and `$B24F` later reads `$000E[0]=4` and
  stores actor 4 to `$0308`; this is the only point where native
  `ball_holder` and the typed LIVE primary move to the receiver. A human pass
  also moves its controller; a CPU pass carries `controller=NONE` and leaves
  both human controller assignments unchanged. Bank06 `$B24F` is unrelated
  coordinate geometry.
- `$B24F` clears the receiver action/animation/actor-state workspaces before
  calling `$B2FA`; `$B2FA-$B300` clears only raw `$BA` bit 2. No broader name
  or semantic meaning for that bit is asserted.

The current Q8 flight duration and linear interpolation are explicitly native
adapters because `$B42F`, the Bank05 `$BB9F/$BBA0` trajectory lookup, and the
five-`$B500` substep scheduler are not yet a strict pass asset. The source
gather order, launch-time receiver lock/role swap, multi-update ball ownership,
and catch-only handoff are preserved. Upstream CPU play selection/cursor reach,
general pass desirability, `$B13F` interception/contact semantics, and the
complete Bank06 inbound formation route remain deferred/fail-closed.

This change is bounded to Bank05 `$B24F-$B32B` and the selected-actor skip
contract at Bank06 `$81F7-$82D`.

- `$B24F`: `primary_actor`/`last_ball_holder` becomes the actual receiver.
- `$B27B-$B291`: the prior offensive actor receives `actor_state=4`,
  `$046E` action state `0`, and command offset `$0B63`
  (`$9F2E+$0B63=$AA91`).
- `$B292-$B2CC`: automatic/human opposition is supplied by typed native
  controller assignment. Runtime evidence disproves treating raw
  `$030C/$030D` as a zero-human/nonzero-automatic encoding, so C does not use
  those raw bytes for ownership classification.
  Automatic opposition scans slots 9 down to 0 and requires both explicit
  `$04B0 & $10` eligibility and `$06CB` dynamic link equality. The selected
  defender follows the new holder; the prior defender resumes its ordinary
  TGAI command path.
- `$B317-$B32B`: a match commits transactionally. The raw 6502 no-match loop
  exits with underflowed X; because that is not a valid native actor slot and
  no observed fallback is available, the port deterministically rejects the
  handoff without partial mutation.

The eligibility and link arrays are validated live inputs, not imported
asset-pack claims and not labeled ROM-exact. Their scan and state-transition
semantics are exact within the verified bounded path; population from the
original runtime's complete `$04B0/$06CB` producers remains outside scope.

`scene_update_ai()` preserves the Bank06 selected-defender exclusion by giving
the selected defender the on-ball holder target while all other actors,
including the released defender and former holder, continue through the
existing source-command transport. No roster/name special cases are used.
