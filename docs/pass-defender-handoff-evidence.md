# Pass defender handoff evidence

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
