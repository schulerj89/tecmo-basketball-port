# Pass defender handoff evidence

## Shared visible pass lifecycle

Ordinary bound human passes now retain the Bank05 ordering rather than
teleporting possession at the NES A edge:

- `$89D7` seeds the passer-owned gather state and packed `$0458=$32`.
- `$86A8` releases only when the complete packed byte reaches `$04`.
- `$B074/$B42F/$B500` own a separately moving ball before the catch.
- `$B24F` is the only point where the native `ball_holder`, controller, and
  typed LIVE foundation move to the receiver.

The current Q8 flight duration is explicitly a native adapter because the
Bank05 `$BB9F/$BBA0` duration lookup and five-substep scheduler are not yet a
strict pass asset. The source ordering, locked receiver snapshot, multi-update
ball ownership, and catch-only handoff are preserved. This does not add a CPU
pass-decision policy, `$B13F` interception/contact semantics, or claim that the
generic violation restart handoff is the complete Bank06 inbound formation
route.

This change is bounded to Bank05 `$B24F-$B32B` and the selected-actor skip
contract at Bank06 `$81F7-$82D`.

- `$B24F`: `primary_actor`/`last_ball_holder` becomes the actual receiver.
- `$B27B-$B291`: the prior offensive actor receives `actor_state=4`,
  `timer=0`, and command offset `$0B63` (`$9F2E+$0B63=$AA91`).
- `$B292-$B2CC`: automatic/human opposition is explicit in `control_mode`.
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
