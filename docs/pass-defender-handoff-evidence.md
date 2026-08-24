# Pass defender handoff evidence

## Shared visible pass lifecycle

Ordinary bound human passes and autonomous selected-primary action-`$21`
passes share one actor-neutral transport rather than teleporting possession:

- Made-score automatic play reaches this transport naturally. Bank05 `$901F`
  writes state 1; Bank06 `$8661-$8727` selects the primary and `$8728-$8773`
  refreshes formation/candidate streams. Automatic play begins at `$0168`.
  Human offense receives selection writes but retains inbound presentation.

- Bank06 opcode 9 is the already-owned writer of actor-local `$046E`. The
  native code does not force the Bank04 `$A05F` record or route CPU intent
  through NES A.
- Selected primary runs before the ordinary loop: `$8374` saves `$0308` in the
  selected context, typed automatic offense in ordinary `$05A1=0` reaches
  `$83F3->$8491`, and state 4 dispatches through `$8B90`. Canonical Rev1
  Bank06 `$8FC5-$8FE7` copies `C9=$21` at `$8FCA/$8FCC` and advances the
  cursor by five. Bank05's selected-primary pointer table consumes index
  `$21` at `$89D7`. `$89D7`
  seeds packed `$0458=$32` and changes the selected actor to state `$0F`;
  state `$0F` dispatches to `$8695`, not state `$0A`. A separate paused trace
  displayed `$24` beside `$89DB`, but FCEUX effective-value annotations can
  reflect later register/memory state; it neither disproves this bounded chain
  nor proves the absence of an intermediate write.
- Native LIVE mirrors that order with typed controller ownership: automatic
  selected-primary state 4 executes one play step, exact `$0131` produces
  `$21`, and gather starts in that update. `$8284-$82A5` then skips primary and
  defender in the ordinary loop, preventing a duplicate fetch. Human selected
  primary and unsupported automatic states/gates remain fail-closed.
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
- Bank05 `$B42F` reduces `max(abs(dx),abs(depth)) + min/2`, halves that sum,
  and indexes the 256 bytes at `$BBA1-$BCA0`. Every Rev1 entry reduces to
  `max(1,floor(index/7))`; the source doubles it to a base duration. `$BCF4`
  divides signed deltas shifted by six by that duration, `$B074->$9A69`
  arithmetic-halves both velocities twice, and two ASL/ROL pairs multiply the
  remaining planar count by four.
- Bank05 `$B1E7/$B500` owns exactly four flight substeps per update, not five.
  Each `$B500->$BD6E` advances two uint16 Q10.6 accumulators with wrap/carry,
  decrements the source count, and exposes integer coordinates with six
  logical shifts. Native C now preserves those accumulators, velocities,
  duration, and four intermediate coordinates in production.
- A nonterminal flight update tails through `$B2F2->$B6B1`, subtracting `$12`
  from the Q8.8 vertical velocity before adding height. Catch state `$18`
  instead follows `$B7B6->$B7F7->$B678`, using gravity `$28` until `$0499`
  reaches zero; a landing update returns and `$B783->$A023` runs on the next
  update. Native C retains that exact phase distinction.
- Genuine Bank05 `$B24F` begins `AC 0A 03`. The captured actor-2 pass locks
  offense-side raw `$037F[0]=4`, and `$B24F` later reads `$000E[0]=4` and
  stores actor 4 to `$0308`; this is the only point where native
  `ball_holder` and the typed LIVE primary move to the receiver. A human pass
  also moves its controller; the controller-none fixture leaves both human
  assignments unchanged. Bank06 `$B24F` is unrelated coordinate geometry.
- `$B24F` clears the receiver action/animation/actor-state workspaces before
  calling `$B2FA`; `$B2FA-$B300` clears only raw `$BA` bit 2. No broader name
  or semantic meaning for that bit is asserted.
- The catch does not return after `$B2FA`: `$B2EC` jumps directly to Bank05
  `$96B6-$9708`. A human offense returns with the receiver's cleared state 0.
  An automatic offense instead writes action `$18`, chooses source stream
  `$007D` or `$00D7`, and returns with the receiver in state 4. LIVE owns the
  automatic-versus-human distinction but not the same-call raw
  `$0373/$0095/$0094` route selector, so it chooses source-valid long route `$00D7` as a
  justified native approximation. Its first record publishes the exact
  absolute target (orientation-adjusted X `$00B4`, depth `$0096`). Opcode 21
  then consumes exact typed shot/game clocks; raw `$007E` bit 1 is projected
  clear as a justified approximation so `$00DC` advances.
  The selected-primary state-6 countdown reached by alternate `$007D` also
  decrements once per update, returns to state 4 at zero without fetching,
  and fetches on the following update. Ordinary and inbound catches share
  this atomic endpoint.

The former Q8 linear interpolation adapter is removed. Production flight uses
the exact `$B42F/$BCF4/$9A69`, four-`$B500` Q10.6, and `$B6B1/$B678` height
contracts. `tools/Run-GameplayPassTrajectoryTests.ps1` pins 11 decoded
Bank02/Bank05 spans, exhaustively validates all 256 `$BBA1` bytes and all 24
`$B1B9` bytes, and rejects 291 independent source mutations; the scene suite
covers table boundaries,
four-step state, and airborne state-18 gravity.

`$B13F` now runs after every one of those four substeps. It uses exact
`$9E0A->$A184` `max+min/2` proximity `<8`, the 24-byte `$B1B9` difficulty
table, preseason/season `$075F/$0760`, automatic-defense `$18` subtraction,
the live TGFR `$6A->$C05D->$6A` pair, and TTDT profile byte 4 loaded to
`$0533` by Bank02 `$A8CC-$A8D0`. Success clears the pass and reaches the
existing transactional `$BA8C->$B87C` claimant settlement with the selected
defender. `$B074`'s exact direction/long-duration/random `$07E9` inhibit is
retained, so excluded passes never over-enable interception. The focused
source gate now pins 11 spans, both complete tables, and rejects 291 source
mutations. General pass desirability and the complete Bank06 inbound formation
route remain deferred/fail-closed.
The exact `$96B6` automatic lifecycle invariant is closed, while its route
branch remains approximate. Opcode 21 now owns `$058A/$0357/$0358` through
typed scene clocks but approximates unowned `$007E` bit 1 as clear.
If a later source opcode-9 record writes selected state 0/action `$17`, Bank05
dispatches it through `$81F2-$822F->$8A6D->$8ACE` into shot initialization.
The pointer dispatch is exact, while launch admission is a bounded native
adapter because `$8ACE` reads unowned `$0478/$0499/$007E` gates. LIVE reuses
the existing source-backed close and TGJS/TGSR jump playback seams. A typed
autonomous owner uses no human pad and carries far/jump playback through ball
release and terminal possession. This does not claim exact admission,
variant/outcome policy, or complete caller ordering.

This change is bounded to Bank05 `$B24F-$B32B` and the selected-actor skip
contract at Bank06 `$81F7-$82D3`.

- `$B24F`: `primary_actor`/`last_ball_holder` becomes the actual receiver.
- `$B27B-$B291`: the prior offensive actor receives `actor_state=4`,
  `$046E` action state `0`, and command offset `$0B63`
  (`$9F2E+$0B63=$AA91`).
- `$B292-$B2CC`: automatic/human opposition is supplied by typed native
  controller assignment. Runtime evidence disproves treating raw
  `$030C/$030D` as a zero-human/nonzero-automatic encoding, so C does not use
  those raw bytes for ownership classification.
  Automatic opposition scans slots 9 down to 0 and requires both explicit
  `$04B0 & $10` eligibility and fixed `$06CB` link equality. The selected
  defender follows the new holder; the prior defender resumes its ordinary
  TGAI command path.
- `$B317-$B32B`: a match commits transactionally. The raw 6502 no-match loop
  exits with underflowed X; because that is not a valid native actor slot and
  no observed fallback is available, the port deterministically rejects the
  handoff without partial mutation.

The eligibility and link arrays are validated live inputs, not imported
asset-pack claims and not labeled ROM-exact. Their scan and state-transition
semantics are exact within the verified bounded path. `$06CB` population is
the source-pinned startup pairing `{5,6,7,8,9,0,1,2,3,4}`; dynamic
`$037F/$07DF` population remains a separate selector lifecycle.

`scene_update_ai()` preserves the Bank06 selected-defender exclusion by giving
the selected defender the on-ball holder target while all other actors,
including the released defender and former holder, continue through the
existing source-command transport. No roster/name special cases are used.
