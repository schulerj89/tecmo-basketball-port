# Native gameplay state and scene provenance

`tecmo_gameplay_state` remains a deterministic pure-C rules boundary, and is
now driven by `tecmo_gameplay_scene` in normal preseason and season games. The
scene renders a strict ROM-derived court and actor poses, drives native music,
SFX, and the held-ball/dribble DMC event, and returns a completed result to the
launching mode. It never loads a decompilation file, trace, capture, screenshot,
video, save state, or dump at runtime.

## Supported runtime boundary

Preseason final team confirmation now launches the selected teams, difficulty,
control ownership, period length, speed setting, and GAME MUSIC setting. A
completed preseason game returns to the stable PRESEASON row on the blue menu.
Season GAME START launches the exact pending schedule ordinal and teams. Its
non-tied result is committed exactly once through
`tecmo_season_commit_game_result` before control returns to the season result
rows; failure to commit leaves the gameplay result active rather than advancing
the schedule.

Live controls use the native NES responsibilities: directions move the owned
actor, NES A passes on offense or switches to the nearest defender, and NES B
starts an offensive shot or attempts a defensive steal/contact action. Offense
resolves before defense when both sides press B in one update. START and SELECT
are inert during live play, and a controller with no assigned team cannot act.

During a human free throw, the scene resolves the controller assigned to the
scoring team and launches only while that pad's current NES B level is held.
The other pad, A, directions, START, SELECT, B release, and a synthetic pressed
edge without the held level are ignored. A human attempt has no timeout. If the
scoring team has no assigned controller, native play uses the bounded slot-3
trace's 125-update inclusive CPU state-18-to-launch schedule (frames 22 through
146). This is an observed native approximation, not a decoded ROM timer. Its
counter resets for each attempt and across scene launch/end. Foul-card dismissal
remains the separate NES A-release presentation gate.

The compound scene loads `gameplay/core` TGPL-1 (23416 bytes,
`2047CCE0`), `gameplay/court` TGCT-1 (6559 bytes, `ECAB7A93`),
`gameplay/camera-projection` TGCP-2 (1536 bytes, `53247856`),
`gameplay/movement` TGMO-1 (1664 bytes, `6C82A137`),
`gameplay/ball-dribble` TGBD-1 (608 bytes, `E2CE6BFF`),
`gameplay/cpu-steering` TGAI-2 (7632 bytes, `C8CFFDC0`),
`gameplay/fatigue` TGFT-1 (512 bytes, `F80F170D`),
`gameplay/court-orientation` TGOR-1 (640 bytes, `44B0C44E`),
`gameplay/hud` THUD-1 (864 bytes, `3D13AA89`),
`gameplay/penalties` TPNL-1 (768 bytes, `980DDC76`),
`gameplay/violation-referee` TGVR-1 (4752 bytes, `2EB08CF0`),
`gameplay/close-shots` TGCS-1 (3144 bytes, `DACDC976`),
`gameplay/dunk-cutaway` TGDK-1 (20272 bytes, `E02F2D21`),
`gameplay/jump-shots` TGJS-2 (2776 bytes, `A66EE873`),
`gameplay/shot-resolution` TGSR-4 (608 bytes, `5376E82B`), `audio/music`
TMUS-1 (36784 bytes, `05C00ECB`), `audio/gameplay-sfx` TSFX-1 (2824
bytes, `968A5DE6`), `audio/gameplay-dmc` TDMC-1 (2515 bytes,
`AD70E6E8`), and the exact 262144-byte `chr/all` revision from one asset pack.
Exact sizes, payload fingerprints, deep indexes, reserved bytes, source-map
spans, CHR fingerprints, and the shared pack path are validated before the
scene becomes available. Missing, malformed, oversized, wrong-revision, or
cross-pack dependencies fail closed without a partial frame.

THUD-1 retains Bank01 `$BDF0-$BECC` for the exact two team destinations and
29 five-tile marks, plus Bank02 `$AF64-$B07B` for the exact character tables
and first-initial/dot/nine-tile-surname formatter. Its 59-character mapping is
required to match the same-pack TTDT-1 `$FA` CHR records. The live scene
prepares both scoreboard rows before drawing and keeps them fixed across TGCP
camera changes. The second row contains each selected player's BCD jersey
number and formatted name; it does not contain a shot clock or period label.
The team destinations and mappings are ROM-exact; complete-row ownership, other
field columns, colon `$16`, black backing, and live `$FA` binding are
reference-verified. Three-digit score capping and the unassigned CPU-side
holder/shooter matchup label are explicit native adapter policies.

TGAI-2 is now a strict compound-scene dependency. The live adapter owns a
fixed opposing roster-slot link, explicit target snapshot, direction result,
and decision serial for each actor. The link remains matchup/pose and defender-
reference metadata; it is not the non-holder's implicit movement target. Its
explicit no-command sentinel is important: the ROM play-command offset/link/
advance lifecycle remains unported and is not silently implied by the live
dependency.

TGOR-1 owns only the binary offensive-direction state synchronized with live
possession. Its exact Bank05 sources are the possession transition gate-and-
swap `$8FAD-$8FE7`, the slots-0..9 actor-role bit-`$10` toggle and queue-`$17`
operation `$9042-$9053`, the target-delta routine `$9054-$90AF`, and target
table `$BDEF-$BDF2` (`$00A0/$0260`). A fresh native launch uses direction 0
and AWAY possession as a cold-start-aligned policy. Same-possession handoffs
and restarts are no-ops; a real change atomically saves previous direction,
XORs current direction, updates tracked team/target X, and increments a serial.
TGCT-1 stays left-to-right. TGOR direction now drives the production camera
follow and its `$00A0/$0260` target selects the world-space shot endpoint;
launch Y is the separately proven `$8F`.

Fixed `$E537-$E548/$E699` is only TGPL presentation-selector cross-check
evidence: `$0758` is derived from `$04FC` bit 7 and IDs `$1B/$2E`; it does not
own orientation. TGSR `$B87C-$B8F5` is a conditional alternate claimant
settlement, not a universal post-shot routine. `$9042` is not described as a
general team switch. `$035B` has no direct reads and is retained only as
save-before-toggle evidence. Direct `$035A` stores are limited to `$8FC4` and
`$B8E0`; broad `STA $0300,X` is limited to fixed-bank cold boot `$CC68`.

### B87C claimant-settlement bridge

The native `LIVE` bridge deliberately exposes only a narrow typed transaction
for Bank 05 `$B87C-$B98A`. Its direct integrated caller is `$BA56-$BA9C`
(FNV-1a32 `B779AC48`, FNV-1a64 `367ED7AC43F1ACA8`): state `$11` reaches `$BA56` through `$A214`'s slot-10
dispatch table, while `$B751` reaches `$BA65` from the claimant path and
`$B180` can jump directly to `$BA8C`. After `$BA65`'s `$05A1`, `$AD01`,
`$07DE`, and `$0588` gates, `$BA8C` clears its local pending fields and calls
`$B87C`. The source span `$B87C-$B8F5` is revision locked by TGSR-4
(FNV-1a32 `9E2F1F28`), and the caller-prefix `$B87C-$B888` is independently
fingerprinted as `E903D8F9`. The claimant candidate arrives through `$9C`; the
preceding `$B73E-$B87B` scan (FNV-1a32 `574FEE44`) is evidence for the bounded
claimant-selection context, not a general possession predicate.

The bridge is entered only after the native scene's existing terminal **miss**
claimant selector has chosen an active claimant in `scene_finish_shot()` or
`scene_finish_jump_miss()`.  It is intentionally absent from generic
possession handoff, made baskets, period/restart handling, tip-off handoff,
steals, fouls, and unproven recovery paths.  This is a bounded C scene mapping,
not a claim that every original `$BA56` caller or collision condition has been
reconstructed.

The legacy direct-launch and rim-rattle debug fixtures retain their existing
generic handoff.  They can use explicitly marked fallback claimant selection
and do not establish the `$BA56` claimant/contact predicate; routing those
diagnostic adapters through `$B87C` would falsely elevate a render checkpoint
to source caller evidence.

For that accepted boundary, C preserves these source-shaped effects in order:

- `$B87C-$B888` snapshots old `$0308/$0309`; C exposes those before/after
  values as transaction observations, rather than naming the claimant a
  rebound, steal, foul, or statistic.
- `$B8BC-$B8CE` promotes `$9C` to `$0308`.  Only if it differs from old
  `$0308` **and** its `$04B0` bit `$10` is set does `$B8D0-$B8F5` save old
  `$0308` to `$0309`, swap `$030A/$030B`, and call `$9042`.
- `$9042-$9053` walks X from 9 down to 0 and XORs `$04B0` bit `$10` for every
  actor.  The native transaction mirrors precisely that selector-role toggle;
  it does not treat `$9042` as an arbitrary team/orientation reset.
- `$B8F6-$B918` scans only when `$030C[$030B]` is nonzero, descending 9..0 and
  testing `$04B0` bit `$10` plus `$06CB == $0308`.  A no-match keeps the
  already-selected `$0309`; C does not substitute the older pass-helper's
  eligibility predicate.
- `$B928-$B96F` has typed owners for the selected/candidate side mirrors,
  automatic-offense `$0547/$0551=$007D`, `$057C=$04`, and the exact `$B98B`
  remap table `01 02 03 04 00 06 07 08 09 05`.  Human-offense stream/state
  resets are not invented.

The existing `selected_defender_handoff_active` boolean is only a native C
consumer gate for the already-selected defender's later movement adapter. The
transaction derives it from the exact nonzero `$030C[$030B]` scan condition;
that downstream movement policy is not an additional `$B87C` claim.

The following observed writes remain intentionally unsupported because this
native scene has no faithful typed owner or caller proof: `$0588/$05B6` masks,
`$0359`, `$0478`, `$06DF/$06E1`, `$0743`, `$0790`, `$07E2`, `$046E/$0479`,
`$06D5`, the `$035A->$035B; EOR #1->$035A` mutation, and helper/action calls
`$B1D1`, `$BFA8`, `$88B6`, and `$C711`.  The transaction records the `$035A`
save/toggle branch as an observation only.  It emits no steals, fouls,
rebounds, blocks, player statistics, or opaque-ROM state mutations.

For opt-in diagnosis, `TGPS-1` snapshots expose typed raw labels
`$0308/$0309`, `$030A/$030B`, `$030C/$030D`, `$000E/$000F`, `$037F/$0380`,
the `$04B0` bit-`$10` mirror, `$06CB`, `$0547/$0551`, and `$057C`, alongside
semantic scene/live ownership aliases.  The `TGLP-1` JSONL
`claimant-settlement` event captures before/after snapshots and a nonzero
transaction serial after starting from the native pre-tip handoff and using a
normal controller-B miss; its coordinate/frame inputs are explicit fixture
data, not terminal ownership injection.  It is console-only developer proof:
normal play does not render it and never reads a ROM, decompilation, FCEUX,
Lua, log, screenshot, or state file at runtime.

### A023 actor-command-assignment boundary

`gameplay/actor-command-assignment` TGCA-1 is a strict nine-span Rev1
evidence asset for Bank05 `$A023-$A0DC`, its `$9DF6/$9E0A` distance helper,
the `$A214-$A25E` object-slot-10 dispatcher/table, source-complete callers
`$B73A`, `$B783`, and `$B7B6` through `$B7C0`, plus the fixed `$C711`
action-`$1D` selector/table path.  The pure typed resolver
preserves only the proven `$A0A6/$A046` stream/state writes: side-indexed
automatic selected actors receive `$000A/$0019` and state `$04`; the two
`$04B0` bit-`$10` scans descend from 9 to 0, exclude `$0308/$0309`, use
`abs16(X)+unsigned_abs8(depth)` with initial `$0505`, and let an equal score
replace the earlier candidate.  The depth subtraction uses the ROM's carry/
borrow-derived 16-bit absolute result (`0` and `200` are 200 apart, not 56).
Thus an equal-distance tie selects the lower actor index. Every successful
scan also reaches the source `$048F/$0484` clears, recorded as unowned
observations even for human control mode.
The full machine-readable span/caller/fixture boundary is
`docs/a023-actor-command-assignment-provenance.json`.
The generated `system/source-map` repeats all nine exact ROM offsets, CPU
ranges, payload offsets, and FNV32/FNV64 values. The parser compares its stored
Rev1 SHA identity, and a bounded one-span verifier gives focused tests direct
descriptor/FNV32/FNV64 coverage behind the full-ROM/aggregate guards.

This is **not** integrated into normal tip-off, jump, pass, claimant, rebound,
or generic possession handling.  The native scene's numeric `jump_ball_state`
does not own the object-slot-10 state/coordinate or the `$BA`, `$05A1`,
`$0499`, `$0588`, `$67/$68`, and `$04AF` gate family required by
`$B6E5->$B73A` or `$B775/$B7B6->$B783`; its defensive-contact path is
`$9968/$9A24`-shaped rather than the preceding `$9F2F->$9FE2` geometry,
property, direction, and `$A0DD` target-construction path.  Therefore all
source-complete calls remain synthetic fixture inputs.  `$046E`,
`$0484/$048F`, fixed `$C711`, and terminal scratch effects are observations,
not native mutations.

The opt-in `TGLP-1` `actor-command-assignment-deferred` event follows an
ordinary PRETIP-to-LIVE scene route and emits `caller_identity:"none"`,
`emitted:false`, and `production_mutated:false`; it records the selected
exclusions, unexecuted scan/winner/score fields, and unchanged selected
stream/state snapshots.  Its screenshot proves only that the normal native
scene remained intact—it is explicitly not A023 gameplay-parity evidence.
Run `tools\Run-GameplayActorCommandAssignmentTests.ps1 -RomPath <LOCAL_ROM>`
for the isolated resolver/importer/mutation suite and
`tools\Run-GameplaySceneTests.ps1 -RomPath <LOCAL_ROM>` for the real-flow
deferred JSONL/screenshot proof.

`gameplay/camera-projection` TGCP-2 is both a strict pure API and a
compound-scene dependency. Its 1536-byte canonical payload
(`53247856`) requires same-pack TGPL-1 and TGCT-1 and preserves the fixed-bank
Rev1 camera initializer, streamed-column and attribute helpers, horizontal
follow/threshold routine, forced settle, actor projector, and the exact
`$F106-$F1B0` movement clamp as a seventh source span. The exact pure
projector computes `world_x-camera_x`, accepts X only when the subtraction
high byte is zero, and for visible actors saturates
`world_y-altitude` to zero on borrow. Offscreen actors return the deterministic
native sentinel `visible=false` with X/Y zero because the ROM branches before
projecting Y.
Initialization/follow/settle also preserve scroll page, stream direction,
layout cursor, cursor bounds, and action-route movement gates. Production
launch leaves pure `$DE13` at cursor `$20`, applies one `$DDFB->$DF05` live
prime to cursor `$21`, seeds world coordinates at camera `$0100`, then settles
once. Each subsequent live update follows exactly once after all actor/ball
mutations. Free-throw entry performs the TGFL-driven typed settle; subsequent
free-throw and TGDK cutaway updates freeze the camera. Possession changes clear
only thresholds/latching.
The production validator also enforces scroll/page coherence, the reachable
direction/cursor relation, and the three exact threshold-pair states; the
weaker validator is retained only for transactional synthetic API tests.

The focused TGFL-1 -> TGCP-2 test module independently loads both assets,
derives orientation 1/shooter 6/secondary 1, and consumes TGFL's exact ten
world X/Y values. Starting from the bounded capture-derived cursor `$21`, it
proves 76 moving updates, an unchanged 77th update, transactional settle at
camera `$0198`, and the exact six visible/four neutral-offscreen projections.
Secondary slot 1 is also bounded frame evidence. This remains independent
integration verification rather than the production scene adapter.
Pure TGCP coverage separately exercises exact generic left/right steps,
disabled and suppressed-route no-ops, page carry/borrow, continuing
coarse-column changes, and direction reversals.

The live scene decodes TGCT-1's complete 96-by-30-tile court (768-by-240
pixels), slices a 32/33-column coarse/fine viewport at TGCP camera X, and clips
both partial edge columns inside a 256-by-240 framebuffer subview. Actors,
anchors, ball Q8 coordinates, shot endpoints, movement, proximity, passing,
switching, and AI share world coordinates. TGCP projects actors and the ball;
jump altitude is applied exactly once to the actor. Live free-throw entry uses
TGOR orientation to copy TGFL-1's exact ten raw positions into that coordinate
plane, settles TGCP to `$0066/$0198`, and renders the coherent TGCT/TGCP frame.
Shooter/secondary selection, ball attachment, and camera composition are native
adapters; live actor poses remain preserved rather than taken from TGFL.
The canonical slicer does not claim the original staged PPU prefetch/write
order.

The shared object-space type fixes `(0,0)` at the full TGCT court's
upper-left, with integer bounds X `0..767` and Y `0..239`. Players and
anchors use integer coordinates; the ball and shot endpoints use Q8 in the
same plane. TGOR now carries complete left/right hoop landmarks
`($00A0,$94)` and `($0260,$94)` rather than an X-only live target. Ordinary
flight still terminates at the separately proven Y `$8F`. A transactional
scene snapshot exposes all ten player coordinates, the ball, and both hoops;
live validation and rendering reject invalid coordinates before consuming
them. Bank04 `$AC8C` and `$ADA3/$ADAE/$ADB9` now supply the exact static
tip-off players and ball anchor through TPTI-1. The post-handoff live layout,
tip animation, and other scene policy do not inherit that evidence.

Typed transactional adapters now connect that state to the existing TGCP raw
contract. Q8 ball focus is validated and floored once for launch settle,
pre-tip handoff settle, and the one post-mutation live follow. Integer player
anchors and the Q8 ball route through TGCP projection adapters, producing one
transactional scene projection snapshot at the current camera X. The snapshot
contains ten player projections and one ball projection; offscreen values use
TGCP's neutral sentinel. Shooter jump altitude is subtracted exactly once,
while ball projection receives zero altitude. Raw TGCP routines and their
goldens remain unchanged; these adapters are native integration plumbing, not
new ROM semantics.

The live renderer now consumes a transactional
`tecmo_gameplay_scene_court_frame`. It combines the possession-aware TGCT
slice, all ten TGCP player projections, and the ball projection with the
scene frame and camera-follow serial. A mismatched viewport/projection camera
X is rejected before drawing. Stationary actors shift by the inverse signed
camera delta while visible, retain Y during horizontal motion, and use the
neutral zero-X/Y sentinel outside the viewport. Sweeps cover fine scroll,
coarse-tile crossings, possession reversal, and both endpoints. Native
left/center/right travel checkpoints use camera X
`102`/`256`/`408` and freeze background hashes
`4F52BCC1`/`9CC9CD31`/`033B45D5`. The underlying TGCT slices and TGCP motion
are strict; the scene binding, checkpoint focus placement, and simplified
possession choreography are native integration.

`gameplay/movement` TGMO-1 carries seven exact spans: Bank02
`$A89E-$A90D`, Bank04 `$ACE4-$AD25`, Bank05 `$879B-$8866`,
`$88F9-$89BC`, `$8E58-$8F96`, `$BF6C-$BFA7`, and fixed
`$F106-$F1B0`. Its canonical 1664-byte payload (`6C82A137`) requires exact
same-pack TGPL-1, TGCP-2, and TTDT-1, and cross-checks the repeated clamp bytes
against TGCP-2. The pure transactional state reproduces one-update direction
latency, Q4 accumulation, TTDT profile-0 plus GAME SPEED `+5/-1/-6`, condition
formula `max(8, adjusted_rating + (condition >> 4) - 6)`, diagonal
`amount-floor(amount/4)`, `$4A/$EC` compare-before-move gates, and period-8
animation phase. Invalid or unreachable state/input arithmetic rejects without
mutation.

The fixed span is now applied as the original selected-actor dispatcher. Object
state 4, action `$0F/$10`, the flags-bit-3 exemption, and the conditions that
set the boundary-violation latch surround page-0
`$00DF-floor(Y/2)`, page-1 interior, and page-2
`$0220+floor(Y/2)`. Ordinary live control supplies object state 0 and flags 0.
Only the offensive primary/ball holder can latch; selector `$0742=1` then
resolves through strict TPNL-1 as OUT OF BOUNDS, clears the latch, and enters
the existing violation/restart rules flow. The deterministic
`gameplay-out-of-bounds-frameN` renderer reaches that presentation by driving
the holder across this production boundary rather than injecting a violation
state.

Backcourt is owned independently by strict TGBC-1, not TGMO. The 512-byte
payload (`810886EF`) imports Bank05 `$970B-$9786` (`C137674F`) and depends on
same-pack TGOR-1 and TPNL-1. The exact `$971F-$9786` ordinary live detector uses
object state zero, `$0588` bit 4, the current orientation, and the 16-bit ball
X. Orientation 0 establishes at X `<=375` and violates on a return to X
`>=386`; orientation 1 establishes at X `>=392` and violates on a return to X
`<=383`. The transactional C step retains the ROM subtract/high-sign/low-byte
comparisons. The preceding selector-4 ten-second path remains unported.

The scene samples the attached held ball after ordinary movement and resets
the latch at its possession-handoff boundary. That scheduling is a bounded
adapter rather than exact caller-order proof. Selector 2 enters the existing
TPNL/TGVR flow, showing the ROM `BACKCOURT` text and exact `3,4,5,5,5`
referee-group sequence; the screen blackout/fade alignment is still
capture-bounded. `gameplay-backcourt-frameN` exercises this production path.

Initial, human-controlled, and TGAI-driven CPU rendering consumes the exact
pose-base/animation-low-nibble result and binds the record tag to its
slot-selected MMC3 R2-R5 bank. The pose-table half uses Bank05 `$8F02`'s exact
signed linked-minus-selected comparison. Which opposing roster slot supplies
that linked coordinate remains fixed scene policy rather than reconstructed ROM
matchup ownership. Opposing directions on one axis are normalized to neutral as
a native integration policy. Starting placement/direction, fixed five-player
roster-slot binding, CPU target selection, and AI remain native integration or
approximations. Ordinary CPU locomotion uses the same exact TGMO step as the
tested TGAI composition. The deterministic CLI harness remains developer-only
and does not add an in-game debug route.

`gameplay/ball-dribble` TGBD-1 is a strict 608-byte ROM-only held-ball boundary
(`E2CE6BFF`). It imports Bank05 `$B52E-$B5BF` (`DB540670`) and
`$B5C0-$B677` (`E9784D28`), and requires exact same-pack TGPL-1 and TGMO-1.
The resolver uses the holder's validated TGMO direction and eight-phase
animation, the same exact `$8F02` half comparison used for walking poses,
signed attachment offsets, and the 128-byte `$B5C8` bounce-height table. Its
ground-contact DMC condition is exactly low nibble 3 and high nibble 0.

The ordinary live scene keeps the held ball attached through this resolver.
Human/legacy cadence and the supported automatic selected-primary flow can
advance TGMO/TGBD animation. Selected primary runs through
`$8374->$83F3->$8491` before the later ordinary loop skips duplication. The original
height is flattened into canonical visible Y before the existing TGCP
projection. That adapter, the fixed opposing roster-slot link supplying the
half comparison, and complete 6502 caller scheduling remain native policy or
unproven. Free-throw and active-shot ball routes remain separate. Focused
tests cover all strict dependencies and mutations plus high, ground-contact,
alternate-half, malformed-state, human/CPU cadence, and sound vectors.

`gameplay/fatigue` TGFT-1 is a strict 512-byte ROM-only evolution boundary
(`F80F170D`). It imports Bank02 `$B4E6-$B5C7` and fixed `$ED2F-$ED3E`, requires
exact same-pack TTDT-1, and preserves difficulty cadence `6/4/1`, active
countdown/capacity/condition decay, bench `+4` recovery and caps, and Rev1's
distinct second-team recovery-countdown store. TTDT profile byte 3 supplies
maximum capacity. The live scene ticks TGFT once per live-action update and
feeds its condition to TGMO on the next update. Fixed scene roster slots `0..4`
stand in for the original active-lineup/substitution selection, and exact 6502
intra-frame caller ordering is not claimed.

`gameplay/cpu-steering` TGAI-2 isolates the ROM routines used by the bounded
live ordinary-movement slice. Its canonical 7632-byte
payload (`C8CFFDC0`) requires exact same-pack TGMO-1 and revision-locks Bank06
`$81F7-$82D3`, `$87AE-$88AF`, `$88DA-$8A95`, `$8B90-$8BE0`,
`$8BE1-$9237`, `$9280-$9329`, `$938B-$9620`, fixed `$C006-$C008` and
`$CBE0-$CBF6`, and Bank04 `$9F2E-$AC75`. It additionally revision-locks the
Bank06 opcode-15 dispatch/handler anchors and canonical records, but only
exposes the selected-defender branch as a raw transactional harness contract:
LIVE has no faithful owner for `$0499` and the related actor lifecycle bytes,
so ordinary opcode-15 execution remains a no-mutation deferred diagnostic.
The exact `$91F1-$91F5` compare preserves `$06D5` unless new X equals it; only
then does `$91F6-$91F8` store old Y to `$06D5`, before the unconditional
`$06D6=09`. The deterministic harness is synthetic source-contract evidence;
a natural FCEUX `$91C8` capture remains open.
The state-4 path adds the actor's
`$0547/$0551` offset to `$9F2E`; the fixed reader temporarily maps Bank04 and
copies one five-byte record to `$C7-$CB`; Bank06 then dispatches its opcode
through 24 exact handlers. The bounded stream has 680 aligned records, and
Bank04 code resumes at `$AC76`.

The pure direction API reproduces the `$92D4-$92DD` zero-delta guard, the
court-range inclusive 2:1 dominant-axis octant decision, and direction map
`3,6,4,7,0,1,2,5`, exposed as codes `0..7` = right, left, down, down-right,
down-left, up, up-right, up-left. It also preserves the 6502's wrapping 16-bit
doubling for synthetic extremes. The guard preserves the prior direction by
skipping the `$92FE` jump to `$88DA` at zero; that no-write case rejects
transactionally in C. The aligned record inspector reports handler entry
effects only, not semantic play names. Complete play selection, actor-link
ownership, shot/pass/steal policy, and the nearby `$B081-$B32E` candidate scan
remain outside this evidence boundary. See `gameplay-cpu-steering.md`.

The CLI-only steering harness exposes the same pure composition for
deterministic inspection without creating an in-game debug route. It validates
all ten canonical coordinates,
selected actor, possession, orientation, holder, explicit opposing
linked/matchup actor, and difficulty, and fingerprints the complete input.
Its holder hoop-approach or linked-actor target is native harness policy; the
final nonzero octant alone is TGAI-exact. Unlike the raw quantizer API, a
zero-delta harness evaluation succeeds with an explicit keep-direction/no-write
result so no prior direction has to be fabricated.

For each ordinary live update, the scene captures one immutable post-human-
input snapshot of all ten canonical coordinates. Supported automatic selected
primary executes its state-4 command once first. Eligible noncontrolled,
nonselected actors then use mirrored per-slot offensive formation coordinates or
explicit goal-side defensive offsets from their fixed linked opponent. A
goal-side target outside the shaped court uses an equal inward offset before
final validation. Those target choices are native approximations; TGAI supplies
the exact nonzero octant and TGMO supplies the converted secondary movement
step. Candidate actor/movement/target states commit together so loop order
cannot alter this frame's targets. Bank06 `$8286-$82A5` skips selected `$0308`
only in the later ordinary loop, preventing a second fetch after
`$8374->$83F3->$8491`. Typed automatic ownership and ordinary `$05A1=0`
admit the supported primary flow; human and unsupported primary gates/states
remain excluded/fail-closed. Fixed links, nonselected formation/defensive
targets, zero-vector bridging, object state/flags, and shot proximity/cadence
remain native policy.

TGSR-4 also has FNV1a64 `FACCE42B52382D6B` and requires exact same-pack
TGPL-1. Its revision-fingerprinted sources are Bank05 `$91BC-$943A`,
`$A6EE-$A9D9`, `$B73E-$B87B`, `$B87C-$B8F5`, `$BA56-$BA9C`
(`B779AC48`), `$9042-$9053` (`CE6C9466`), and `$B98B-$B994` (`404311FE`),
plus focused state-`$15`
convergence `$A2DF-$A2F7`, launch target `$AD4E-$AD64`, and orientation snap
table `$BDF3-$BDF6`, plus the exact 124-byte point-arc boundary table
`$BEEF-$BF6A` (`9EF1061B`, FNV1a64 `E8A0728513DD8BDB`): seven primary
plus four focused source spans. The older `35FB80C4` fingerprint applies only
to `$BA65-$BA9C`; it is not used for the full caller descriptor.
The safe native semantics are terminal outcome polarity, numeric
rim-route selection, the state-`$15` one-to-four-pass horizontal rattle
prefix, claimant thresholds, and claimant-driven handler/possession
decisions. Its imported `$BAB9-$BAD7` values are render-script selection
addresses, not literal sprite or CHR IDs. Completion restores the saved
velocity before `$A2DF`; only the observed diagnostic `$6A=$71` route is
connected to state `$10`. The alternate state-`$05` relaunch remains outside
the native boundary. TGSR does not name a rebound, block, steal, or generic
make. Missing, malformed, wrong-sized, wrong-revision, or cross-pack TGSR data
rejects the scene before availability.

The pure point-value API reproduces Bank05 `$B995`: shot-flag low bits select
free throw value 1; otherwise raw world X/Y and orientation 0/1 select field
goal 2 or three point 3 using the exact `$5B..$D6` Y range, carried arc table,
and 6502 low-byte subtraction/high-byte borrow. The classifier routine bytes
remain in same-pack TGPL-1's existing `$B995-$BA3F` source span rather than
being duplicated. This exact rule does not enable live ordinary two-point
makes. TGJS-2 translates `$AD4E->$B32C->$B100` only when all raw launch
inputs are explicit; exact `$AD6E` live ownership is still missing. The
supported three-point route alone uses `$AC0A-$AC6E`'s state-08 timer, deriving
handoff as frame 85 plus 26 updates.

`gameplay/penalties` TPNL-1 is a separate strict 768-byte pure rules asset
(FNV1a32 `980DDC76`) with same-pack TGPL-1 and TSFX-1 dependencies. It exposes
bounded foul classification, violation, and presentation data without
inferring contact, collision, possession, or route state. The scene now
consumes it only in a bounded human defensive-B bridge: the selected
primary/defender pair passes B05 `$9968`'s raw envelope, then the ordinary
Bank05 `$9571` fall-through adapter (`$07E3=0`, `$0478=$19`, no retained
`$05A8` selector) is classified by TPNL. Counter effects and attempts come
from that result. This is not a reconstruction of the original caller or all
routes; special routes and CPU-initiated fouls remain fail-closed. The
machine-readable map is `docs/live-foul-asm-parity.json`.

`gameplay/violation-referee` TGVR-1 is the separate strict 4752-byte visual
counterpart. It loads only exact same-pack `chr/all` and TPNL-1, decodes ROM
screen `$05`, maps all seven Bank03 violation strings through the original
character table, and retains Bank04's 15 referee metasprites and five gesture
sequences. Each `$B33F` metasprite piece is one 8x8 CHR cell: the source
records advance coordinates and consecutive tile values in eight-pixel steps,
so the renderer preserves tile bit zero and does not synthesize a second
stacked tile. Shot-clock selector 5 uses the exact sequence `9,10,10,10`; it does
not reuse selector 1's out-of-bounds pointing sequence `3,4,5,5,5`. Focused
render coverage requires the visible out-of-bounds groups 3, 4, and 5 to be
distinct, then requires terminal group 5 to hold through the wait. The Bank04
group cadence and 44-frame controller duration are ROM-derived. Alignment of
the generic screen loader to nine black frames, four-frame visible palette
steps, and first selector-specific pose at phase frame 23 remains
capture-bounded because the PPU loader is not cycle-ported. The scene consumes
strict TPNL-1 presentation metadata to request shared SFX 6 at presentation
frame 16 exactly once, removing the prior immediate-cue approximation. Full
renderer and original caller-order parity remain incomplete even though the
visible gesture and text are ROM-backed.

The two supported close-shot families retain their numeric ROM identities.
Variant 0 is the dunk family and has 32 exact steps in the
direct/held-release family; variant 2 is the layup family and has 16 exact
steps in the arc/longer-trajectory/contactable family. Their phase
tables and all 208 TGCS-stored profile/direction resolutions into TGPL pose data
are exact assets. Live play currently selects only profile 0/direction 0; that
narrower selection remains a native approximation, not a property proved by the
asset. Resolved uniform pose-cell polarity is preserved from fixed
`$D413/$D498` and `$D503` `AND #$41`, then compared with effective facing only
while that facing equals the actor's assigned TGOR goal baseline. Deliberate
movement/action overrides, mixed poses, pre-tip presentation, and encoded
tip/action poses retain the former orientation path; eligible uniform poses
mirror only when authored and goal polarity differ. Fresh orientation-0 cases
are Bank05 `$8F47/$8F57` raw `$012A` -> pose
149 -> `$A6E3/$884E` -> four resolved `$41` attributes (Away-left), versus raw
`$016A` -> pose 181 -> `$A723/$8702` -> four resolved `$03` attributes
(Home-right). The captured original frame/OAM independently confirms the visible
Away-left bit-$40 polarity; it does not claim a native pose-149 identity. `$40`
is the horizontal-flip bit, not an intrinsic left meaning for arbitrary art.
Numeric variant 1 remains unexposed. TGCS APIs and fields continue to expose
numeric IDs; the loader derives and validates the
exact 0=dunk, 2=layup semantic mapping without changing the 3144-byte payload
or its `DACDC976` fingerprint.

The dunk family now crosses into the strict TGDK-1 presentation. The importer
decodes screen `$0B` into both bounded 960-cell backgrounds, resolves the exact
profile-1/uniform-`$30` palette checkpoint, retains both side-specific seven-stage
8x16 sprite streams, and maps every background/sprite tile into same-pack
`chr/all`. Record order remains NES OAM priority, so the native renderer
composites in reverse. The observed action schedule is live 1-22, dispatch 23,
initial black 24-27, visible cutaway 28-62, black/rebuild 63-70, live return 71,
A9C5 at 87, and settlement at 132. Stage 0 is assigned at 27 and first visible at
28. Later assignment and captured first-visible frames are
32/37/42/47/52/57. Frame 63 remains black despite retained final-stage OAM; frame
64 clears OAM. The scene freezes the TGCS live pose at step 22 during the
presentation, resumes at step 23 on frame 71, reaches step 31 on frame 79, and
holds it through settlement.

Ordinary-jump evidence is narrower and fails closed outside its imported
numeric route. The scene consumes the exact TGJS
family-0/profile-0/direction-1 data for the captured human away/right
terminal-miss and three-point-make branches. For live play, a native adapter
may horizontally mirror that route for either manually controlled team. It
first requires the active holder, rules possession, TGOR tracked team, and
validated offensive hoop to agree, derives actor facing from the hoop rather
than stale movement facing, and freezes the endpoint. That two-basket adapter
does not establish another ROM direction selector. The miss
preserves current-level NES B hold and release, actor `$0C->$0D->$0E->0`
progression, Bank05 unsigned Q8.8
height/velocity seed `$02E8`, gravity, frame-40 integer-height floor clamp,
recovery to idle pose 469 at frame 46,
ball route `$01->$05->$17->$10->0`, and settlement at frame 87. Actor and ball
lifetimes are independent. The release does not request DMC; only the proven
route-10 ground/bounce condition requests `$B5AB` at frame 75. The ball's
world-space interpolation remains native geometry and is not claimed as the
ROM launch solver; its captured endpoint uses TGOR X and the proven launch Y
`$8F`.

The ordinary-jump gate still uses an explicitly native deterministic policy.
Its predicted-miss branch stores UNKNOWN at frame 1; current-B release at
frame 2 passes TGJS's bit-7-set outcome flag through TGSR and requires MISS.
At frame 87 the normal route requires TGSR's non-current, OTHER_TEAM claimant
decision, awards zero points, queues crowd 11 followed by clock-gated side
result 12/13, and gives possession to an explicitly approximate opposing
actor. A simultaneous period expiry queues only crowd 11 and retains the
current side. Outcome state clears after settlement and no rebound/block/steal
stat event is synthesized.

Family selection no longer invents the missing Bank05 `$006A` predicate from
a stable hash. `$8B12` proves the reset value is family 0; `$8B83-$8BC8` can
select family 1 only after its complete hoop/defender/side/raw-state gate. The
scene therefore keeps production on family 0 until that raw owner is retained.
When a miss route ends with no actor inside the strict claimant envelope, the
native scene uses its existing generic opposing-team handoff instead of
rejecting the update. That compatibility path emits no B87C claimant trace and
does not claim a rebound, steal, or source-exact claimant.

TGSR-4 adds a separate deterministic diagnostic for the proven state-`$15`
prefix without changing the normal frame-87 miss. Its canonical source uses
four passes. Frame 73 snaps the exact orientation-0 state to raw `(157,147)`,
altitude `$38`, timer 4, and positive `$0040` velocity. The visible
positive-first route proves a negative incoming horizontal sign, but no
capture proves its exact magnitude; the deterministic diagnostic therefore
uses `-1` as a sign-only sentinel. Rendering now preserves the raw world state
directly against the TGOR orientation-0 endpoint `(160,143)`: the initial
state `(157,147)` is the proven `(-3,+4)` offset beside that target. This
intentionally replaces the former unproven screen endpoint `(224,123)`.
Each update moves one coordinate,
positive-first through raw X 161; frames 77, 81, and 85 reverse direction,
reload the four-update timer, and queue the existing address-bound A8D6-short
DMC clip. Frame 89 restores the diagnostic sentinel and selects exit
render-script address `$BADD`. The generic state API preserves and restores
the caller-supplied horizontal and vertical values exactly. The observed raw
selector `$71` satisfies `$A2DF`'s
`>= $18` predicate, so this one diagnostic enters the existing state-`$10`
timeline and settles at frame 103. The native API also tests one through four
passes and orientation 1, but it does not implement the alternate state-`$05`
relaunch or make live selection random.

The three-point make uses the same strict TGJS/TGSR runtime dependencies.
B stays current through frames 1-8 and releases at 9.
Pose pointers are 325, 1060, 1061, 213, then neutral 469; the prepared phases
are `31/21/11/01/32/22/12/02`. TGSR requires the terminal bit-clear MAKE at
frame 19. Uninterrupted Q8.8 motion starts at frame 20 from `$0308` with
gravity `$0028`, lands at native frame 57, recovers through frame 62, and is
neutral at 63. The emulator capture displayed landing/recovery at 59-65 only
because unrelated main-loop overruns held display frames 38 and 53; those are
not native shot waits. Frame 85 awards three points and resets the shot clock
to 24. Frame 111 changes possession and queues crowd 11 only. The observed
ball checkpoints bound the route; the current world-space ball arc remains a
native approximation while its TGOR endpoint and TGCP camera projection are
strict. If B releases before frame 8, native play
normalizes directly to the bounded frame-9 release transition; this prevents a
stalled scene without claiming unobserved early-release ROM timing. A period
expiry before frame 111 applies the frame-85 basket once without resetting an
expiry-state shot clock, retains the shooting holder on settlement, and lets
the next state update enter the normal period banner.

Bounded local original execution supplies the semantic correlation. Save-state
slot 2 held numeric variant 0 through the live approach, entered its visible
cutaway, and later triggered Bank05 `$A9C5` DMC at action frame 87. A9C5 remains
address-bound and unresolved; this observation proves neither meaning nor
exclusivity.
Slot 1 held numeric variant 2 and triggered `$ABF5` at action frame 34. These
local save states, FCEUX/Lua traces, screenshots, and logs remain ignored
verification evidence only; they are not committed source-map provenance,
asset-pack inputs, or runtime dependencies.

State timing is evidence-derived: game-clock divider 45, shot-clock reset 24
with possession divider 50, an inclusive 31-update fixed expiry wait, 60-frame
period banners, a 120-frame halftime banner, the 44-frame violation
screen/referee controller followed by its four-frame input lead-in and
120-frame wait (168 frames total), and a 160-frame foul wait. Violation
presentation can be dismissed only after the controller plus input lead-in;
foul presentation retains its four-frame lead-in. Halftime and final score
screens use their separate unbounded NES A-release gate. Individual foul-out is
six, the team-bonus threshold is five in regulation and four in overtime, and
team fouls clear after the completed period/halftime banner path.

Gameplay track 5 is queued at launch and qualifying restarts only when GAME
MUSIC is enabled. Presentation track 6 is requested for halftime/final score
presentation. The scene maps clock expiry to SFX 3, late-clock seconds to 14,
violations to 6, and motion with a held ball to the proven `$B5AB`
held-ball/dribble DMC clip. A made dunk, the supported jump miss, and
every resolved free throw, make or miss, request crowd response 11 followed by
away-side 12 or home-side 13 when the clock is above 0:01. The jump miss awards
no points. The same mailbox is last-write-wins, so the side result is consumed
next; 0:00 and 0:01 retain 11. Made layups
and the bounded three-point jump make request only 11 pending separate
caller-path evidence. Neutral
SFX 5 is kept as `BANK05_9FEC_CUE` and is requested only at the evidence-bounded
violation, direct-foul, and period restart boundaries under the GAME MUSIC
gate. Foul/violation and completed-period presentation boundaries clear music,
tonal SFX, and DMC once before replacement audio. Their qualifying live returns
requeue the gated cue and gameplay track 5. A foul route entering free throws
instead queues track 5 at sequence setup without the same-numbered SFX cue. The
ignored bounded slot-3 observation begins setup at frame 10, requests gameplay
track 5 at 26 and consumes it at 27, then changes the terminal result mailbox
from `$0B` to `$0D` at 280 and consumes it at 281. It is live by 300 with no new
music-track request or SFX ID 5 through 360. A final free throw therefore keeps
its result request through the live transition and queues neither track 5 nor
`BANK05_9FEC_CUE`. Dunk action frame 87 requests address-bound A9C5. The
state-`$15` diagnostic queues address-bound A8D6-short only on nonterminal pass
repeats. Normal live miss selection still queues neither A8D6 clip. Clip IDs
0, 1, and 2 remain semantically unresolved, and no clip name asserts an impact
or rim cue.

The supported jump-miss settlement uses that same central crowd/side-result
helper independently of point accounting. Its release requests no DMC; only
the route-10 ground/bounce condition queues B5AB at action frame 75.

## ROM-derived anchors

The behavior encoded here was bounded against the Rev 1 ROM and the matching
decompilation at these CPU-address ranges:

- Bank 03 `$8374-$8378`: selectable regulation-period minute values.
- Fixed bank `$E80F-$E81E`: inclusive 31-update expiry wait.
- Fixed bank `$E58D-$E617`: period/halftime/final/overtime decisions;
  `$E59B->$E823` first prepares regulation M:00/divider 45, and only the tied
  overtime restart at `$E601-$E60F` overwrites the duration with OT minutes.
- Fixed bank `$E6ED/$E6FF`: the two team-foul clears after a completed banner.
- Fixed bank `$E765-$E76F`: shot-clock-24 and divider-50 reset.
- Fixed bank `$E7D0-$E822`: live-action settlement gate at period expiry.
- Fixed bank `$E823-$E898`: new-period M:00/divider-45 preparation and live
  clock tick behavior.
- Fixed bank `$E95E-$EA11`: foul presentation and its 160 one-frame waits.
- Fixed bank `$EA14-$EA2F`, with input helper `$D2B9-$D2CE`: four-frame
  presentation lead-in followed by dismissal on NES A release from either
  controller. Held A, directions, START, and other releases do not dismiss.
- Fixed bank `$EC5B-$ED14` and Bank 03 `$BE87-$BFA8`: violation dispatch and
  the seven numeric violation values; only shot-clock expiry is generated by
  this module.
- Bank 06 `$A05A-$A24F`: period/halftime banner selection and presentation.
- Bank 06 `$BC3C-$BCF9`: halftime/final score screen's own unbounded NES A
  release loop. It does not share the fixed-bank presentation timeout.
- Bank 05 `$94F9-$9674` and Bank 02 `$B0F8-$B398`: bounded individual/team
  foul counters, bonus-related paths, overlay state, and the two observed
  post-foul shot-24 clock-divider outcomes (45 and 50).
- Bank 05 `$8A33-$8ABD`: the human state-20 path selects the shooting side's
  controller and tests its current NES B level before launching a shot; it does
  not consume direction or button-edge state at that gate.
- Bank 05 `$96B6-$9708`: CPU ownership bypass and selection of command/script
  offsets `$007D` or `$00D7`. These values are offsets, not frame timers.
- Bank 06 `$8B8E-$8B9D`: maps the selected command offset from base `$9F2E` to
  its stream/dispatch pointer.
- Bank 06 `$9621`, `$976F-$985C`: free-throw setup/lineup state boundary.
  Strict TGFL-1 additionally covers `$88B0-$88D9`, `$9621-$976E`,
  `$976F-$985C`, and `$985D-$9918`, preserving raw world coordinates, the
  shooter-dependent stream skip, resolved nonshooter pose indexes, and base
  actor-state seeds. The current scene consumes the exact positions for both
  orientations; it does not apply the pose/state or conditional script
  overrides.
- Bank 05 `$8ABD-$8CE4`, table `$8CE5-$8D7C`, launch `$9C40-$9CC9`, actor
  progression `$86BB`, `$86DD`, `$8732`, `$8745`, result `$91BC-$943A`, ball
  path `$AF30-$B073`, and scoring `$B995-$BA3F`: numeric close-shot subtype 01
  and its surrounding shot machinery.
- The bounded ordinary make executes Bank05 `$8C57` at current-B launch,
  `$91BC->$933B->$942D` for terminal MAKE polarity, and Bank05 `$BA02` for the
  observed three-point score application. `$9C79` is not treated as a
  universal launch boundary.
- Bank 05 `$AD01-$AD0E` (FNV1a32 `B7141C72`): result crowd-response request 11;
  `$8C7D-$8CE4` (FNV1a32 `00A4D185`) is its bounded close-shot caller path.
- Bank 05 `$B1D1-$B1E6` (FNV1a32 `CFCD9759`): above-0:01 clock gate and
  pre-handoff side-result request 12/13; `$B19D-$B1A4` (FNV1a32 `ED5EE105`)
  is the bounded result caller path. `$BA56-$BA9C` (FNV1a32 `B779AC48`) and
  `$B87C-$B888` (FNV1a32 `E903D8F9`) supply the integrated jump-shot settlement
  caller evidence.
- TGSR-4 revision-locks Bank 05 `$91BC-$943A` (`4A0C68AC`),
  `$A6EE-$A9D9` (`21A416FD`), `$B73E-$B87B` (`574FEE44`),
  `$B87C-$B8F5` (`9E2F1F28`), `$BA56-$BA9C` (`B779AC48`),
  `$9042-$9053` (`CE6C9466`), and `$B98B-$B994` (`404311FE`) for terminal
  polarity, numeric rim dispatch, claimant scanning, claimant-driven
  settlement, caller gates, selector toggle, and candidate remap respectively. Focused
  provenance additionally locks `$A2DF-$A2F7` (`9D918043`),
  `$AD4E-$AD64` (`AF1D6B17`), `$BDF3-$BDF6` (`79F66DB3`), and
  `$BEEF-$BF6A` (`9EF1061B`) for conditional convergence, launch target,
  orientation snap, and point-arc table provenance.
  The X target table `$BDEF-$BDF2` is supplied by the required same-pack
  TGCS-1 `$BDEF-$BDF6` span and cross-checked against TGSR's snap data.
- TPNL-1 revision-locks Bank 05 `$9571-$9649`, Bank 02 `$B0F8-$B398`,
  fixed `$E95E-$EA11`, `$EA14-$EA2F`, `$EC5B-$ED14`, and `$D2B9-$D2CE`,
  Bank 03 `$BE87-$BFA8`, and Bank 04 `$BA1F-$BA3E`. These feed the pure
  penalty asset/API. The bounded live defensive-B bridge consumes the first
  two together with the separately tested Bank05 `$9968` envelope; its
  ordinary route profile is documented in `live-foul-asm-parity.json` and is
  not a claim of complete live caller or collision parity. Once its separate
  foul-state request accepts, the scene retains the typed defensive-pushing
  actor/team/class/counter/attempt snapshot and uses Bank02's exact `$2283`,
  `$2292`, `$22A3`, `$22A6`, `$22E3`, `$2303`, and conditional `$234B` writer
  cells through the existing TTDT/THUD font/CHR binding. `$B373-$B398` gives
  the regulation-five/overtime-four bonus side mask only; no visible BONUS
  text is inferred.
- Fixed `$EC06-$EC25` (FNV1a32 `F1BCC8E2`): clears active music, SFX, and DMC;
  bounded call sites are `$E58D`, `$E9A0`, `$E9DE`, and `$ECAF`.
- Bank 05 `$856B-$85A7` and `$85F3-$8640`: variant-0 presentation trigger and
  clear-lane helper.
- Fixed `$E770-$E78D` and descriptor `$DCD2-$DCD8`: presentation dispatch and
  screen `$0B` selection; Bank 00 `$9022-$9346` plus `$9346-$9355` supply the
  overlapping D9F6 terminator/base-palette boundary.
- Bank 01 `$B002-$B157`: controller, seven-stage setup/tables, and palette
  recipe; fixed `$C711-$C73B` plus `$CAF5-$CBAE` supply selector dispatch.
- Bank 06 `$B37C-$BC3B`: relative sprite emitter, side pointers, and all
  four-byte geometry records; fixed `$EB8D-$EC05` restores the court.
- TGJS-2 owns the otherwise-unpacked Bank05 spans `$8001-$815A`,
  `$8469-$846A`,
  `$8999-$89C0`, `$8D92-$8DD2`, `$9C29-$9C3F`, `$AC0A-$AC6E`,
  `$AD41-$AF21`, `$B6E5-$B774`, `$B7C1-$B87B`, `$BA65-$BAC0`,
  `$BCA1-$BDC6`, and `$BDF7-$BEF6`. It depends on TGPL-1 for
  actor dispatch/poses/results and TGCS-1 for dispatcher, launch-solver, Q8.8,
  and route tables already covered there. The 32 normalized pose indices are
  rederived from Bank05 `$8D3D/$8D5D` and have FNV-1a `A057A625`.

The corresponding lifted sources include
`decomp/lifted/bank03/C-0144_bank03_selection_value_table_8374_8378.asm`,
`decomp/lifted/bank05/C-0095_bank05_state_and_pose_lookup_tables_8CE5_8D7C.asm`,
`decomp/lifted/C-0005_bank05_91BC_943A.asm`,
`decomp/lifted/bank05/C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm`,
`decomp/lifted/bank06/C-0055_bank06_period_banner_dispatch_A05A_A0A9.asm`,
and
`decomp/lifted/bank02/C-0177_bank02_roster_team_player_data_9000_BFFF.asm`.
These are provenance only and are not runtime inputs.

## Explicit evidence boundaries

- Only an allowed live action reported on the update that reaches zero enters
  unbounded settlement; earlier action history is ignored. A later settled
  report completes that sequence, while an initially settled zero-clock state
  follows the fixed 31-update path.
- Every completed period first prepares regulation M:00/divider 45 through
  `$E823`, then chooses its banner, halftime, overtime, or final-score branch.
  Only a tied overtime restart at `$E601-$E60F` overwrites that duration with
  OT minutes; a completed overtime final retains regulation minutes.
- Foul subtype/detection, which counters a foul changes, post-presentation
  possession, and selection of divider 45 versus 50 are caller-supplied.
  Unsupported or malformed choices fail without mutating state.
- Free-throw controller ownership and the human current-B launch gate are
  supported. TGFL-1 strictly resolves the base raw lineup for both
  orientations, and live entry copies all ten exact positions into canonical
  court coordinates before a typed TGCP settle and coherent TGCT/TGCP render.
  CPU play uses the bounded observed 125-update launch schedule. Slot selection,
  held-ball attachment, camera composition, and post-attempt possession are
  native policies; conditional pose/state/script overrides, visual lineup
  timing, aiming, and made/missed and rebound behavior remain unresolved. Only
  explicit made/missed results and settlement are modeled.
- The exact TGCS numeric step/phase tables and the selected TGPL pose resolution
  are consumed directly by the scene. TGCS exposes 208 exact resolutions, but
  live selection is limited to profile 0/direction 0. Eligible baseline-facing
  uniform poses reconcile their authored cell polarity with the actor's TGOR
  goal; explicit movement/action overrides retain their prior facing path. That
  live selection policy remains approximate. The older state-only rightward
  actor-9 observation remains provenance for the semantic event layer, not a
  universal animation label.
- TGJS/TGSR exact playback proves only the human away/right
  family-0/profile-0/direction-1 terminal-miss and three-point-make branches.
  Live shots reuse/mirror that numeric route toward the transactionally
  validated TGOR hoop for either controlled team; this is native adapter policy.
  Current-B transitions, actor Q8.8/state/recovery timing, terminal polarity,
  the miss ball-state/bounce DMC path and claimant settlement, plus make
  score/possession/crowd checkpoints are exact within that context. The
  state-`$15` one-to-four-pass prefix is available through a deterministic
  debug/test API only; live selection remains unchanged. Unsupported
  profiles/directions/outcomes, ordinary two-point makes, the longer +157-update
  claimant route, and make ball geometry do not inherit those frame
  checkpoints. TGSR-4 can classify an input coordinate as two points and
  TGJS-2 can simulate distance flight from explicit raw inputs; neither owns
  live `$AD6E` launch inputs or admits the route. No semantic rebound,
  block, steal, or player-stat event is claimed.
- Post-handoff live actor layout and fixed five-player roster-slot binding,
  live CPU command selection, dynamic link/spacing policy, original active-
  lineup/substitution ownership, exact intra-frame fatigue caller ordering,
  violation detectors beyond the TGMO movement boundary,
  jump-ball interpolation, unsupported jump routes, general
  make/contact policy, the distance policy
  selecting dunk/variant 0 versus layup/variant 2, live close-shot
  profile/direction selection and other close-shot policies,
  state-dependent palette transitions outside the exact live-court and
  cutaway contexts, foul detection, free-throw slot selection,
  held-ball/camera composition, aim/result/rebound and CPU positioning/script
  behavior, and
  the HUD's fixed-column and unassigned-CPU actor-selection adapters are native
  approximations. THUD-1's font, team marks, and Bank02 name formatting are
  exact within the boundary above. Live court, ball, and player palette
  selection is exact for the currently bound roster slots: profile byte 2 bit
  7 and the side bit reproduce `$04B0 & 3`, TGCT-1 supplies fixed
  `$F2E2-$F2F1`, and fixed `$DEAB/$DC19` substitutes the matchup colors at
  entries 3/7/11/15. Bank01 `$B0ED` remains the distinct exact pose/cutaway
  palette recipe. The embedded FCEUX RGB profile is also exact; palette
  transitions outside the covered live-court and cutaway contexts remain out
  of scope. The exact rules state consumes
  explicit outcomes without turning those scene policies into ROM-exact claims.
- The dunk cutaway's standalone profile-1/uniform-`$30` checkpoint remains
  exact, and production now supplies the selected roster profile bit and
  matchup uniform color through the same live binding. Its native
  shot arc and deterministic make/miss policy continue behind/after the exact
  presentation and are not claimed as ROM behavior.
- Local original-frame comparisons, after normalizing the small FCEUX screenshot
  RGB-output difference, matched the frame-24 black, frame-32 stage, frame-48
  stage, and frame-64 black cutaway pixels exactly. Returned live frame 80 still
  differs in actor spacing; the bounded score/clock/jersey-number glyph silhouettes
  now match the local reference, and player matchup colors follow the exact ROM
  selection path. Horizontal
  camera/world projection is strict within the supported slice.
- The module contains no proprietary ROM bytes, screenshots, traces, save
  states, dumps, or capture artifacts.

Run `tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for the
strict full-pack scene test and deterministic 640x480 start, jump-miss through
87, jump-make through 111, dunk checkpoints through 132, and both
orientation-specific free-throw lineup checkpoints, plus held-ball high/low
bounce frames. Run
`tools\Run-GameplayDunkCutawayTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
the strict TGDK payload/provenance/render/mutation/revision checks.
`Run-GameplayShotResolutionTests.ps1`, `Run-GameplayPenaltyTests.ps1`,
`Run-GameplayFreeThrowLineupTests.ps1`, and
`Run-GameplayCourtOrientationTests.ps1` validate the strict TGSR/TPNL/TGFL/TGOR
parsers, same-pack dependencies, source mutation, and pure APIs.
`Run-GameplayMovementTests.ps1` validates TGMO's seven ROM spans, strict parser,
same-pack dependencies, malformed state transactions, deterministic harness
vectors, and live scene handoff without exposing an in-game debug path.
`Run-GameplayBallDribbleTests.ps1` validates TGBD's two source spans, strict
parser and same-pack dependencies, table/phase vectors, malformed transactions,
and the native ground-contact DMC condition. `Run-GameplayFatigueTests.ps1`
validates TGFT decay/recovery cadence and state transactions.
`--gameplay-state-test`, the TGPL/TGCT/TGCP/TGMO/TGAI/TGCS/TGJS focused
suites, the 84-entry full asset-pack regression, and
`Run-GameplayAudioTests.ps1` retain their lower-level coverage.
