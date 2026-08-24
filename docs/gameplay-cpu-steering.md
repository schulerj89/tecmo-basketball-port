# CPU steering evidence boundary

Ordinary live CPU actors now use the transactional TGAI-to-TGMO adapter. This
note describes the exact Rev 1 routines isolated behind
`gameplay/cpu-steering` TGAI-3 and the bounded native target policy that feeds
their direction identity into the existing TGMO locomotion kernel. This is a
live movement integration seam, not a ROM CPU command-policy claim.

## Exact call graph

Bank06 `$81F7-$82D3` is the ten-actor update loop. It visits slots 9 through 0,
excludes the two currently selected actors at `$0308/$0309`, and dispatches
through the actor state in `$057C`. State 4 reaches `$8B90`.

At `$8B90-$8BE0`, the actor-local words at `$0547/$0551` are added to Bank04
base `$9F2E`. The call to fixed `$C006` jumps to `$CBE0-$CBF6`, which saves the
current switchable bank, maps Bank04, copies exactly five bytes into
`$C7-$CB`, and restores the prior bank. Bank06 treats `$C7` as an opcode and
dispatches it through 24 low/high handler entries. The exact handler CPU
addresses, in opcode order, are:

```text
$90E0 $934B $9280 $905E $8FFA $8F92 $8F2D $8F12
$8ED7 $8FC5 $8CD0 $8C40 $8E4F $9125 $9146 $9172
$9085 $8C1A $8C1A $8C1A $9032 $8BF6 $8BE1 $8F72
```

Bank04 `$9F2E-$AC75` is exactly 3400 bytes: 680 aligned five-byte records, all
with opcode 0 through 23. `$AC76` resumes executable code. TGAI retains this
raw revision-checked section inside the private asset pack, but committed
documentation and tests expose only record metadata, fingerprints, bounded
inspection output, and selected regression offsets—not the table contents.

Bank06 `$938B-$9620` selects formation streams and writes actor command offsets.
That proves one initialization path into the command transport; it does not
prove every condition that selects a play or formation.

## Opcode 4: canonical ball-object target

Opcode 4 is the bounded exception to the ordinary ten-player coordinate
snapshot. The state-4 caller path is Bank06 `$81F7-$82D3` to `$8B90-$8BE0`,
then fixed `$C006/$CBE0-$CBF6`, and then canonical Bank06 handler
`$8FFA-$9031`. `$8FFA` loads `$C8` as the target-object index; the next handler
begins at `$9032`. The strict
TGAI corpus contains exactly two
opcode-4 records, at stream offsets `$0000` and `$016D`; both select object
slot `$0A`.

Bank04's pre-tip setup initializes object slots `0..10`; slots `0..9` are the
player coordinates while slot `10` is the separately initialized ball object.
The native port therefore represents slot `10` as
`TecmoGameplayCpuSteeringPlayInput.ball_position` and records its identity in
`TecmoGameplayCpuSteeringPlayState.target_object`. It is not an eleventh actor,
does not receive a command stream, and is not used by fixed `$06CB` or full
play-selection policy.

The handler subtracts the selected actor from the target object as a 16-bit X
value (low/high bytes with borrow), then subtracts depth as an 8-bit value and
sign-extends its borrow to a 16-bit delta. It ORs the two complete deltas; a
zero vector skips `$88DA` and keeps the prior direction. The C result records
these bounded opcode-4 deltas for regression coverage. The production scene
captures slot `10` from the current floored-Q8 ball snapshot once, advances the
command once, and launches the exact state-5 planar route. Active route ticks
bypass TGMO and never retarget from the moving ball. Ordinary non-route targets
continue through the documented TGAI-to-TGMO compatibility adapter. This is
source-to-C target transport, not a claim to reconstruct offensive play
selection.

## Exact planar route arithmetic subset

TGAI-3 additionally exposes a pure, transactional subset of the native route
lifecycle. Bank06 `$88DA-$8A95` supplies signed-delta magnitude and octant
arithmetic, `$8A96-$8AF3` computes duration and signed Q6 velocity, and
`$8AF4-$8B8F` integrates the wrapping Q6 accumulators in state 5. The signed
division helper is independently revision-locked at `$9BD8-$9C6E` (151 bytes,
FNV1a32 `74DD2AC6`). No TGMO or fixed `$F106` clamp is called by this route.

Launch requires explicit target-minus-actor X/depth, raw `$7C48`, and raw
`$06E7` inputs. Opcode 4 captures the target coordinate once; changing that
object later cannot retarget an active route. State 5 integrates before testing
the timer. On decrement to zero, `$0359` bit 0 selects which actor half finishes
immediately; the other half performs one additional integration with timer
zero. Both side-bit values are explicit kernel inputs.

This is deliberately described as an exact *planar arithmetic subset*.
Pose-table selection at `$932B/$933B` and selected/ordinary presentation/action
effects through `$0458/$0479/$046E` are not modeled. The route kernel itself
consumes separately typed and classified projections for `$7C48`, `$06E7`,
and `$0359`.

The bounded CPU-route profile projection closes the two actor-local launch
inputs used by LIVE. Fixed `$C045->$CC00-$CC11`, retained
by strict TGRB-1 `$CC00-$CC2F`, maps the actor's `$05A9` lineup slot to the
24-row `$7C48` plane using side offsets `{0,12}`. The caller supplies the live
condition for that resolved roster slot. Strict TGMO-1's Bank02
`$A89E-$A90D` span supplies both `$A90B={+5,-1,-6}` and
`$A908={-3,-2,-1}`; projection performs wrapping byte additions exactly.

Raw `$030C/$030D` are still not controller mirrors. The projection therefore
requires an explicit typed `extra_adjust_admission_available` input and fails
transactionally when it is absent. LIVE supplies the controller-derived
admission as a labeled native approximation; that policy is not made
source-exact by this arithmetic API. The scene clock divider is the typed
`$0359` decrement/reload lifecycle and supplies bit 0 after the normal pre-AI
state update; this typed order does not claim complete original cross-bank
intra-frame parity. Selected-primary state 5 runs first; ordinary state-5 actors then
run in canonical `9..0` order, excluding the selected defender. Any role or
formation transition that invalidates a frozen target cancels its route state
transactionally so stale Q6 motion cannot resume later.

Made-score promotion is now a distinct typed lifecycle boundary, not an
opcode-barrier escape. It projects the accepted Bank05 `$8FB9-$9042` writes;
the preceding `$8FAD` raw `$05A1==0 && ($BA&3)==0` admission is not owned.
The accepted path swaps selected roles, clears both
selected states/actions, and toggles all role bits before the Bank07/Bank06
restart setup enters inbound transport. Consequently an actor carrying an
ordinary `$0A55->$0627` aggregation stream cannot become the selected holder
and execute that stale cursor before the catch assigns its selected route. The
state-`$0B` resolver itself remains unconverted here; source excludes
`$0308/$0309` from its release scan, so forcing a selected actor back to state
4 would be incorrect.

## Opcode 9 action `$21`: selected-primary autonomous pass

The exact opcode-9 executor copies Bank04 command byte `C9` into actor-local
`$046E[X]`. Canonical Rev1 Bank06 `$8FC5-$8FE7` contains that handler, with
the `C9` load/store at `$8FCA/$8FCC` and RTS at `$8FE7`; rewind begins at
`$8FE8`.

Selected primary is dispatched before the ordinary actor loop: `$827E` calls
`$935D`, `$8281` calls `$8374`, and typed automatic offense in the supported
ordinary `$05A1=0` context reaches `$83F3 JSR $8491`. State 4 dispatches
through `$8B90`; exact opcode 9 writes `$21`, advances the cursor by five, and
Bank05's selected-primary pointer table consumes index `$21` at
`$89D7`. `$89D7` writes selected-actor state `$0F` and packed `$0458=$32`;
state `$0F` dispatches through `$8695`; `$8999/$9C29` produced the observed
`$32->$22->$12->$02->$03->$04` cadence; `$86A8-$86B7` releases at the complete
byte `$04` directly into shared `$B074`; and genuine Bank05 `$B24F` performs
the catch. This route does not require slot-10 state `$03`: runtime evidence
observes `$0478=$13` at direct `$B074` entry before it writes flight state `$04`.

At launch, the typed `$037F[$030A]` candidate is locked as receiver and the
`$000E/$037F`-shaped side roles swap, while the `$0308`-shaped primary/native
holder remains the passer until catch. The controller-none deterministic
fixture cannot mutate either human assignment. Human NES-A passing continues
to use the same transport with its controller attached.

Native LIVE implements that selected-primary-first order using typed controller
ownership rather than raw `$030C/$030D`: state 4 executes exactly once before
`$8284-$82A5` skips `$0308/$0309` in the ordinary loop. Exact Bank04 `$A05F` /
stream `$0131` naturally produces `$21` and begins gather in the same update.
Human selected primary stays excluded. Broader play selection and unsupported
selected-primary states/gates remain fail-closed. The
current flight duration/interpolation also remains a labeled native adapter
until `$B42F/$BB9F/$BBA0` and `$B1E7/$B500` are imported as a strict asset.
The standalone C `$BD6E-$BDC6` arithmetic kernel exactly preserves uint16
wrap/carry and six logical shifts, but is not wired into flight until its
solver/table inputs are owned. Bank05 `$B13F` interception/contact remains
fail-closed.

At genuine catch, Bank05 `$B24F` first clears the new selected holder to state
0, then continues in the same invocation through
`$B2EC->$B2FA->$96B6-$9708`. Human offense retains state 0. Automatic offense
writes action `$18` and returns in state 4 on either source-pinned `$007D` or
`$00D7`; stopping before this tail froze the selected ball handler because
Bank06 excludes it from ordinary dispatch. LIVE now derives the raw-equivalent
`$0373/$0095/$0094` inputs through the exact descending `$B317` link scan,
ordinary `$9DF6` deltas, and `$A184` metric, then executes the exact
sign/orientation/threshold branch. Its long route's first opcode-2 record owns
the exact orientation-adjusted absolute target. The following opcode-21 gate receives
exact typed shot/game clocks. Fixed `$F07E-$F0B9` now owns raw `$007E` bit 1:
it clears each loop, requires idle slot 10 and primary depth `$7B..$AE`, then
sets beyond the orientation-specific X boundary. LIVE advances past `$00DC`
with that exact predicate.
Exact 6502 intra-frame ordering between clock evolution and Bank06 is not claimed.
Selected-primary state 6 is also scheduled exactly for alternate `$007D` via
`$82B6/$82C4->$9053-$905D`: one wrapping byte decrement per update (`0->$FF`),
with only a decrement result of zero returning to state 4. No state-6 update
fetches; the retained cursor fetches on the following state-4 update. Ordinary
actors use the same handler, and non-state-6 dispatch ignores a stale nonzero
wait byte.

Selected-primary state 0/action `$17` is not neutral. Bank05
`$81F2-$822F` dispatches action index `$17` through `$8351/$8378` to
`$8A6D->$8ACE`, the shot initializer. That pointer dispatch is exact, while
launch admission is a bounded native adapter because `$8ACE` also consumes
unowned `$0478/$0499/$007E`. LIVE transactionally reuses the existing
source-backed close and TGJS/TGSR jump playback seams. A typed autonomous owner
uses `NO_ACTOR` instead of borrowing a human pad; the public human entry retains
its controller-team gate. This closes the far/jump ball-release and terminal-
possession lifecycle without claiming exact `$8ACE` admission, variant policy,
outcome policy, or complete 6502 caller ordering.

## Exact direction selection

Two bounded paths write the same actor direction field `$0463`:

- `$87AE-$88AF` computes a direction from an actor to a selected reference.
- `$88DA-$8A95` consumes signed target-minus-actor deltas in `$A4-$A7`; its
  octant decision is bounded through `$899D`, and it uses the map at `$8A8E`.

The stored direction codes are:

| Code | Direction |
| ---: | --- |
| 0 | right |
| 1 | left |
| 2 | down |
| 3 | down-right |
| 4 | down-left |
| 5 | up |
| 6 | up-right |
| 7 | up-left |

For court-reachable deltas, an axis becomes cardinal when its magnitude is at
least twice the other axis, including equality; otherwise the result is
diagonal. The 6502 routine performs the doubling as wrapping unsigned 16-bit
math, which the C API also preserves for synthetic extreme inputs. The
target-application guard at `$92D4-$92DD` skips the `$92FE` jump to `$88DA`
when all four delta bytes are zero, thereby retaining the prior direction. The
pure C API represents that no-write case by returning false without changing
caller-owned output.

## Strict asset contract

TGAI-3 is 8016 bytes with FNV1a32 `D56EE070`. It requires exact same-pack
TGMO-1 (`gameplay/movement`, 1664 bytes, `6C82A137`) so direction identities
cannot silently diverge from the locomotion boundary. Its twelve Rev 1 source
spans are:

| Source | Range | Bytes | FNV1a32 |
| --- | --- | ---: | --- |
| Bank06 actor loop/state dispatch | `$81F7-$82D3` | 221 | `23BB7271` |
| Bank06 actor-to-reference direction | `$87AE-$88AF` | 258 | `F866B06C` |
| Bank06 target-delta direction/motion | `$88DA-$8A95` | 444 | `9616E586` |
| Bank06 route duration/projection | `$8A96-$8AF3` | 94 | `939C6882` |
| Bank06 state-5 Q6 integration | `$8AF4-$8B8F` | 156 | `C2E05331` |
| Bank06 command fetch/dispatch | `$8B90-$8BE0` | 81 | `9AD2BA91` |
| Bank06 handler cluster | `$8BE1-$9237` | 1623 | `344298FE` |
| Bank06 target application | `$9280-$9329` | 170 | `C82E6853` |
| Bank06 formation-stream selection | `$938B-$9620` | 662 | `47818A62` |
| Fixed trampoline | `$C006-$C008` | 3 | `14B2472E` |
| Fixed Bank04 reader | `$CBE0-$CBF6` | 23 | `41C5B5C8` |
| Bank04 five-byte commands | `$9F2E-$AC75` | 3400 | `71331A96` |

Import and runtime parsing validate the full-ROM SHA-256/FNV identity, exact
source fingerprints, fixed header/descriptors, zero-reserved bytes, alignment
padding, handler-table agreement with the ROM dispatch table, all 680 aligned
opcodes, the canonical payload hash, and the same-pack TGMO dependency.
Malformed or wrong-sized input fails closed.

## Opcode 15: raw primary/selected-defender source contract

Opcode 15 has exactly two canonical five-byte records in the retained Bank04
command corpus: `$9F65-$9F69` / stream `$0037` and `$9F79-$9F7D` / stream
`$004B`, both `0F 00 00 00 00`. The Bank06 dispatcher `$8B90-$8BE0` maps the
record to `$9172`; the relevant source region is `$9146-$9216` (209 bytes,
FNV1a32 `FA3E6C5E`). `$9146-$9216` is an overlapping semantic anchor inside
the existing copied `$8BE1-$9237` handler span, not an additional source span.

The canonical Rev1 ROM is authoritative over the lifted listing here. Its
separately revision-locked `$9208-$9216` tail (15 bytes, `839F9D07`) performs
`$057C,X = 07`, `$059E = X`, `TXA/TAY`, then passes selector `4` to Bank07
`$C711`. The lifted source stops too early and omits the state/`$059E` writes.
The resolver records the selector and X/Y as **observed, unexecuted**; it does
not invent a C meaning for `$C711`.

The pure raw harness implements both exact replacement paths when an external
capture supplies every raw owner at the same command
execution point. It writes the old defender's `$057C=04`, `$0547/$0551=$005A`,
`$046E=0`, `$0442/$044D` and `$0479/$0458` through `$88B0-$88D9`; then it
sets the new defender's `$0479=81`, `$057C=07`, `$059E=X`, `$06D6=09`, and the
side-indexed `$000E=X`. At `$91F1-$91F5`, the newly selected X is compared to
`$06D5`; only the equality path falls through to `$91F6-$91F8` and stores the
old defender Y into `$06D5`. A non-equal X preserves `$06D5`; `$06D6=09` is
then unconditional. Gate-noop is exact below `$0499 < $46`. Canonical raw
branches `$9185 D0 F2` and `$91C6 D0 B1` both target `$9179 RTS`, so the
typed harness classifies them as exact bit-2/bit-3 no-advance returns with no
mutation; neither retries the altitude gate nor enters opcode 14's `$9146`
mark-other loop. Invalid-direction and missing-owner paths remain
classified/deferred without mutation. The `$9187-$91C1` primary path replaces
`$0308`, applies the exact `$88B0` old-primary pose/action reset, publishes the
old primary through `$037F[$030A]` and `$06DA` with `$06DB=09`, observes
`$C060->$CBF7` invalidating `$058B/$058C` to `$FF`, and transactionally applies
an explicit `$943B->$938B` formation-output capture. It then records `$9208`'s
intermediate state-7/`$059E`/selector-4 handoff before the final
`$046E=1B`, `$057C=0`, and `$000E[$030A]=new` stores.

TGO15-1 separately models `$059E` as a persistent typed selection latch. Its
sole writer is `$920D`; `$9248-$926F` is admitted because the independently
typed dispatch actor is in state 7, then `$9248-$924D` replaces X with the
stored actor. Side scratch, selector 3, and conditional retirement therefore
use the stored actor even when it is stale and not state 7. The consumer never
clears the latch, so a retained value is explicitly stale until the next writer. Only a
serial-admitted full reset clears it; period and possession transitions retain
it.

LIVE now executes opcode 15 in the admitted shot/off-ball scheduler. Object
slot 10's translated vertical-height byte owns `$0499`; automatic-side admission
makes the relevant controller-only `$007E` bit provably clear; the live role,
formation, lifecycle, pose/action, `$06D5/$06D6`, and persistent `$059E` planes
feed the raw resolver transactionally. The ordinary play-step boundary still
defers when that scheduler ownership is absent. To inspect a valid sample, watch
`$0499` (slot 10), `$04B0,X`, `$007E`,
`$0308/$0309/$030A/$030B`, `$000E,Y`, `$06D5/$06D6`, `$0547/$0551`, `$057C`,
`$046E`, `$0463`, `$0442/$044D`, `$0479`, `$0458`, and `$059E` at a naturally
executed canonical record. The `--gameplay-cpu-steering-opcode15-harness`
command is deterministic synthetic source-contract proof only, not a gameplay
command remains deterministic synthetic source-contract proof; natural FCEUX
captures independently confirmed both replacement branches and state-7 writes.

The core API, with CLI-only inspection wrappers, provides:

- aligned command decoding with its raw opcode, four arguments, handler CPU
  address, and a deliberately bounded handler-effect category;
- exact target-minus-actor octant quantization;
- a deterministic full-snapshot harness that validates all ten canonical
  actor coordinates plus selected actor, possession, orientation, ball holder,
  opposing linked/matchup actor, and difficulty;
- a deterministic TGAI-to-TGMO movement harness that adds rating, condition,
  GAME SPEED, and frame count, maps all eight direction identities to TGMO NES
  held bits, and advances the selected CPU actor through the role-coherent
  primary-holder or secondary-nonholder clamp path;
- transactional rejection for unaligned/out-of-range commands and zero or
  invalid raw direction vectors.

The snapshot evaluator deliberately composes exact and native-owned pieces.
Default and explicit targets are deterministic harness inputs, not live CPU
policy. The fixed `$06CB,X` startup pairing is source-pinned as
`{5,6,7,8,9,0,1,2,3,4}`; dynamic `$037F/$07DF` selection remains a separate
lifecycle. A caller-supplied linked/matchup actor is still only a typed harness
input unless it is bound through one of those exact owners. Only the final
TGAI target-minus-actor octant is ROM-exact.

Every coordinate must be in TGCT canonical X `0..767`, Y `0..239`; the holder
must belong to the possession team, and the linked/matchup actor must be on the
opposing five-slot team. Actor slots `0..4` are away/team 0 and `5..9` are
home/team 1; possession uses those same values, while orientation 0/1 means
left/right offensive hoop. The output prints all ten coordinates and a
domain-separated canonical FNV1a32 snapshot fingerprint covering every
coordinate and context field. Repeating identical input is byte-stable, and a
mutation to any coordinate changes the fingerprint. A zero target delta is a
successful harness result with `write=0`/`direction=keep`, representing the
ROM guard's no-write outcome without requiring the CLI to invent a prior
direction.

The shared movement adapter requires the selected actor coordinate in the
ten-actor snapshot to match its TGMO movement state before every transaction.
A nonzero
TGAI result is mapped through the validated same-pack TGMO direction table,
not through a second inferred direction table. TGMO then owns the exact
one-update action-state latency, movement-rating/GAME-SPEED/condition amount,
Q4 fractional accumulation, diagonal reduction, animation phase, vertical
gates, and the selected actor's primary-holder or secondary-nonholder court
clamp. After a successful step, the CLI
copies the resulting canonical coordinate into the next snapshot and
re-evaluates the target and direction.

Human/legacy holders and the supported automatic selected-primary flow feed
committed TGMO direction/animation into strict TGBD-1 held-ball geometry. TGBD's
height/attachment tables are exact, while the
scene's fixed linked actor and visible-Y-before-TGCP adapter remain native
policy.

The exactness boundary has one important seam: TGAI's zero-vector guard means
"do not write direction," while TGMO consumes held controller-direction bits.
There is no exact bit value implied by that guard. The shared adapter supplies
neutral as a native policy and still allows TGMO's one-update latency to run.
The initial offensive-facing direction, explicit rating/condition values,
default/explicit target, and linked target also remain harness-owned inputs or
policy. Neither the neutral choice nor those policies are presented as ROM CPU
command behavior.

Run:

```powershell
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\build\tecmo_port.exe --gameplay-cpu-steering-test <PACK>
.\build\tecmo_port.exe --gameplay-cpu-steering-inspect <PACK> <OFFSET> <DX> <DY>
.\build\tecmo_port.exe --gameplay-cpu-steering-harness <PACK> <ACTOR> <POSSESSION> <ORIENTATION> <HOLDER> <MATCHUP> <DIFFICULTY> <X0,Y0> ... <X9,Y9>
.\build\tecmo_port.exe --gameplay-cpu-steering-movement-harness <PACK> <ACTOR> <POSSESSION> <ORIENTATION> <HOLDER> <MATCHUP> <DIFFICULTY> <RATING> <CONDITION> <SPEED> <FRAMES> <X0,Y0> ... <X9,Y9>
.\build\tecmo_port.exe --gameplay-cpu-steering-opcode15-harness <PACK>
```

These commands are developer tooling only and never add an in-game debug mode;
normal play calls the pure API directly.

## Deliberate limits and next integration

TGAI-3 does not claim a complete CPU play policy. Its supported automatic
selected-primary state-4 flow can naturally execute exact opcode-9 action `$21`
and enter the downstream pass consumer. It still does
not reconstruct every actor-link assignment,
own live collision/contact or speed-setting policy, or treat the nearby Bank06
`$B081-$B32E` candidate scan as ordinary movement targeting. That scan is now
converted separately as the per-frame receiver/defensive-switch selector; it
does not change the TGAI movement-target boundary. Fatigue evolution is owned
separately by TGFT-1 and supplies condition to TGMO. Handler-effect
names describe bounded entry behavior; they are not play names.

The scene's compatibility CPU record owns an explicit target position,
direction/write result, immutable-snapshot fingerprint, and decision serial;
its `command_offset` stays at the no-command sentinel. The actual source-stream
owner is `live_foundation.play_state`. The supported automatic selected primary
executes one source command first; non-controlled non-selected candidates then
advance in Bank06's exact descending `9..0` ordinary-loop order from the same
post-human-input snapshot. The loop skips selected primary and defender, so the
primary cannot double-dispatch after its dedicated step.

Bound production consumes validated source actor/object targets and source
directions from `play_state`. Actor references resolve against the current
immutable snapshot; direction-only records use the documented bounded TGMO
target-composition adapter. Unsupported effects retain typed defer reasons and,
without a validated target, produce no movement instead of falling back to the
older five-point offensive formation and goal-side defender policy. Those fixed
points remain compatibility/legacy native policy only. Fixed opposing links
remain pose/facing and defender-reference metadata, not universal movement
targets. The TGAI octant and TGMO movement step are exact where their inputs are
owned; direction-only target construction, zero-vector handling, pass flight
duration/interpolation, shot proximity, and cadence remain native policy.

The bounded executor accepts explicit captured inputs for opcode 10's
`$8CD0` branch context and `$8D59-$8E21` relative workspace, plus opcode
16's `$0309` / `$036E/$0370` workspace and opcodes 13/20's persistent raw
`$038D:$038E/$038F:$0390` latch words. Opcode 13 performs complete wrapping
16-bit subtraction against actor X and zero-extended actor depth, preserves
both raw latch words as typed evidence, and then uses the same
zero-vector/direction and `$92CA` behavior as the common target tail. Raw latch
words are never relabeled as an in-court movement target; the exact resulting
direction can use the existing bounded direction adapter. `$92CA`'s `$BA` gate has one narrower
LIVE owner: an explicit ordinary-LIVE/no-transient-action condition projects
only `($BA & 3) == 0`; it does not recreate `$BA` or obtain its value from a
clock or cleared C struct. It then reproduces the signed opcode-10 arrival
interval `[-8,+7]` and opcode-16's `$90AC-$90D5` depth `+10/-10` and
orientation-selected horizontal `+16/-16` adjustments. Those are pure
source-contract paths. Ordinary LIVE owns the Bank02 selector only when it
produces an actual `$07DF` store. Non-primary links use the exact distance
branch without timer input. Primary links execute only with a tagged,
single-use runtime frame binding of the fixed `$6A` sample, persistent `$0798`
timer, launch rate `$075F`, and bias `$0760`; absent or malformed binding keeps
the exact `missing-linked-relative-workspace` defer reason.

Opcode 16's workspace is scene-frame input, not actor state. At the start of
the live ordered action, before controlled movement, the scene snapshots the
typed primary coordinate and orientation and applies the exact Bank05
`$9054-$90AF` arithmetic. `scene_update_ai` validates and binds `$036E/$0370`
once; both canonical `$0309` pointer records and every eligible actor in that
tick share the result. The context is consumed with the scene frame. Direct AI
callers must explicitly bind equivalent pre-motion evidence to execute opcode
16; absence stays `missing-pointer-workspace`, while malformed available input
fails the scene transaction.

`TecmoRuntime` owns the fixed `$54:$53/$6A` cadence independently of the TPTI
bridge. It ticks once before each runtime mode dispatch and stages one `$CD96`
extra mix for each valid gameplay launch, committing that candidate only after
scene launch succeeds. The same sample is stable across every opcode-10 call
in one scene update. Preseason rate is difficulty with zero bias; season rate
is 2 with `low8(game_index>>5)` bias. `$0798` persists across gameplay scenes.
Within an update, selected-primary dispatch precedes ordinary `9..0`; only an
actually fetched, nondeferred opcode 10 publishes its pending timer, and a
failed scene transaction publishes nothing back to the runtime.

### LIVE command-input ownership and defer reasons

Bank06's dispatch table consumes several RAM planes that do not all belong to
one actor's retained scene state. `TecmoGameplayCpuSteeringPlayInput` now
models *availability* separately from each byte value. A zero is accepted only
when the relevant typed owner is available; an unavailable workspace fails
closed before the handler changes stream or actor lifecycle state. The typed
`deferred_reason` is retained per actor, shown by the F3 overlay, and included
in deterministic LIVE proof JSON.

| Source consumer | Faithful LIVE owner | LIVE result when absent |
| --- | --- | --- |
| `$9146` opcode 14, `$04B0` bit `$10` | `LiveFoundation.actor_selector_flags`, synchronized before the input is built | Executed; an unselected `0` is valid. |
| `$8F92-$8FBC` opcode 5 | Exact record offset 380 / `$A0AA` (`05 02 00 00 00`); TGOR supplies orientation. The direction mirror is `{1,0,2,4,3,5,7,6}` and canonical direction 2 is invariant. | Writes direction 2, actor state 4, and action-state `$18`, then advances to `$0181`. It does not write target storage: prior target bytes survive but become inactive metadata, while the existing TGMO adapter composes the newly owned direction. The external `$034A` pose source remains an observation and is not fabricated. |
| `$8F72-$8F91` opcode 23 | Selected-primary automatic prepass only. Exact record offset 395 / `$A0B9` (`17 00 00 00 00`); uncontrolled ownership is zero. | Advances `$018B->$0190` without RNG/defender-depth reads or a direction write. Controlled and ordinary contexts remain typed deferred. |
| `$8F2D-$8F71` opcode 6 | Selected-primary automatic prepass only. Exact record offset 400 / `$A0BE` (`06 00 00 00 00`); no controller owns the possessing team. | Automatic `$8F2D-$8F4C` retains `$0190`/state 4, writes typed object-10 state `$13` and actor action `$10`. The next AI update consumes action `$10` through fixed `$C711` selector 1 into the existing `$89DB/$89D7` pass gather. Controlled `$8F4D-$8F71` remains deferred. `$0743=0` and `$0588^=1` are not claimed. |
| `$8ED7-$8F11` opcode 8 | Immutable held-ball X and TGOR orientation. The eight exact records are `$0B68/$0BA4->$025D`, `$0B77/$0BB3->$029E`, `$0B86/$0BC2->$02DA`, and `$0B95/$0BD1->$030C`. Each redirect destination begins with opcode **decimal 17**, not opcode 11. | Orientation 0 redirects below `$0140`; orientation 1 redirects at or above `$01C0`. Redirect stores C8:C9 directly with state 4 and no +5; the complement writes state 4 and advances +5. `$0588&7` clearing remains an unretained observation. |
| `$8C40-$8CC7` opcode 11 | Immutable actor positions and the source-fixed `$06CB,X` pairing represented by `play_state.fixed_link`. Exact records are `$0050` / CPU `$9F7E` and `$005F` / `$9F8D`, both `0B 00 00 00 00`. An exhaustive scan of all 46 pinned formation starts finds no actor at `$0050/$0055/$005F`; upstream production reachability is therefore unclaimed and positive tests are explicitly canonical-record executor fixtures. | Computes wrapping 16-bit X and zero-extended 8-bit depth magnitudes, with horizontal winning ties, then writes raw pose-low `{0A,0C,0E,10}`, pose-high `$04`, packed action `$30`, and actor state 4 before +5. `$0479=$C1` remains unowned. Direction, targets, coordinates, and the native visible pose index are unchanged. |
| `$8E4F-$8ED3` opcode 12 | Sole record `$006E` / CPU `$9F9C`, `0C 00 00 00 00`. The safe LIVE subset requires typed automatic offense, actor state 4, actor != defender, exact `$07DF/$06CB` resolution, opcode-10 workspace/frame ownership, and ordinary `$BA&3==0`. An over-approximating graph from all 46 imported formation starts does not reach `$0069/$006E/$0073`; upstream cursor ownership is unclaimed and the LIVE regression explicitly parks the canonical record. | Exact close window is each wrapping axis `-8..+7`. Close applies opcode-11 raw pose/state/action and advances +5, or stalls at +0 when `$06CB` is the state-5 primary. Non-close publishes the projected target and advances +10, or +5 on that stall. Controlled, defender, malformed, and missing-owner paths defer transactionally; unowned `$0513/$051E/$07F4/$8AF4` and defender action `$1C` are excluded. |
| `$8F12-$8F2C` opcode 7, `$046E,C8` | Selected-primary prepass only: both exact records use `C8=$0A`, meaning object-slot-10 `$0478`; the ordinary selector seam proves `$0478==0` before this dispatch. The capture is cleared immediately afterward. | Selected primary takes current `+5` and state 4. Ordinary actors remain `missing-actor-046e-probe`, because opcode 6 can set `$0478=$13` during the descending traversal. |
| `$8CD0/$8D59/$92CA` opcode 10, `$07DF`, `$0478/$06CB/$0308` branch context, linked-relative workspace, and `$BA` | Ordinary `$0478==0`: the post-human held-ball/dribble projection over AI's immutable actor snapshot feeds a transactional TGBC preview and owns `$0588&$10`; `LiveFoundation` owns roles, `$04B0`, `$06CB`, and positions. Only actual candidate/explicit-`$FF` selector stores become available. Non-primary links own the distance-only workspace. Primary links additionally require the single-frame runtime timing/RNG context. The ordinary-LIVE `$BA&3==0` seam owns the tail. | Both link branches execute with their exact typed owners. Retained/no-store remains `missing-special-actor-07df`; absent/malformed primary context remains `missing-linked-relative-workspace`. |
| `$9085/$90AC` opcode 16, `$036E/$0370` | Fixed `$F031->$81F2->$8209/$833B/$9054` once-per-loop capture of the primary's pre-motion coordinate and orientation; the tagged scene context is bound once before Bank06 traversal. | Executes for eligible actors. Absent context is `missing-pointer-workspace`; malformed available context rolls back. |
| `$8BF6-$8C17` opcode 21, `$058A/$0357/$0358/$007E` | Exact typed `shot_clock/clock_minutes/clock_seconds` own `$058A/$0357/$0358`; fixed `$F07E-$F0B9` authors bit 1 from idle slot 10, primary depth `$7B..$AE`, orientation, and raw X boundary `$00F8/$0208`. | Executes the complete source +5/+10 gate; boundary-vector tests cover both orientations and depth exclusion. |
| `$92CA` common target tail, `$BA` | `scene_cpu_common_tail_has_ordinary_live_zero`: exact `LIVE`, no result/abort, violation, free throw, shot, pass, lineup, or dunk lifecycle | Supplies only typed `flags_ba=0`, so Bank06 `$92CA-$92D0` takes its five-byte `$8FD9` increment. Every other path remains `missing-ba-lifecycle`. |
| `$9125` opcode 13, raw `$038D:$038E/$038F:$0390` latch words | TGGL-1 types all five Bank05 writer families, atomic last-writer-wins serial/provenance, one-shot virgin construction, fixed full-reset clear, and period/possession retention. Ordinary nonlegacy MISS playback owns the persistent `$A0F3` launch-target write, the `$A7A9->$A790` object-position overwrite, and the frame-89 `$A9DA` projected overwrite. Successful `$A790` traces own `$BA&3==0`; eligible shot off-ball records consume the latest value, and `$A9DA->$A993` guarantees the chosen actor's same-update `$002D` consume. `$0041` is bounded unreachable: zero direct starts across 46 pinned formations and no over-approximating graph path after opcode 15's exact no-advance return. | Executes during the admitted shot lifecycle after `$A790`; other contexts remain `missing-global-target`. |
| `$9032-$9052` opcode 20, raw `$038D:$038E/$038F:$0390` latch words | Exact records are `$000F` / CPU `$9F3D` and `$0019` / `$9F47`, both `14 00 00 00 00`. TGCA types the exact `$B721` and `$B783` stores (`$7D:$F2/$FD:$00`) plus the immediate `$0019` actor mask from the same successful assignment. A single-use scene context exposes that value only to those masked actors during the following Bank06 9..0 traversal; cursor coincidence, selected/delayed `$000A`, and opcode 13 cannot consume it. | Production now binds exact object-slot-10 height `<4`, state `$17`, and `$0588&$20` through `$A214->$B775->$B783->$A023`, then consumes/expires its latch in that update. State-$10/state-$18/interaction callers remain `missing-global-target`. An accepted opcode 20 computes wrapping raw deltas, preserves target-plane bits under inactive-storage provenance, clears former semantic/raw meaning, applies exact zero-vector/state-4 or nonzero-direction behavior, and advances +5 without `$BA`. |
| `$9172-$9216` opcode 15 raw lifecycle | Shot/off-ball production owns object-slot-10 `$0499`, automatic-side `$007E` admission, live role/lifecycle/presentation fields, exact imported formation output, and persistent typed `$059E`; deterministic tests force both primary and defender replacement branches. | Executes on canonical `$0037/$004B` records for automatic actors. `$9248-$926F` consumes the retained latch and retires eligible state 7; generic play-step contexts without the shot scheduler remain `missing-opcode15-raw-lifecycle`. |

TGA9-1 narrows TGGL's `$A9DA` family to a pure target/assignment subset. It
accepts normalized A9DA-time signed velocities only, enforces fixed `$002C`,
zero-extends ball depth8, and reproduces the signed-product arithmetic shift by
six and raw16 wrap. `$AAB8` uses actor raw X16 and court depth8—not altitude—
with the exact orientation tables, 9-to-0 role exclusion, strict-lower metric,
and highest-slot tied winner. `$A993` clears chosen action-state `$046E`, not a
wait plane; `$0458` survives. Chosen/linked stream seeds also update native
`last_step_offset` solely as bookkeeping. The helper explicitly omits
`$0478=$10`, `$B3DD->$049A/$04A5`, and the presentation/audio register tail.
Natural no-write values corroborate projections `009D + (004B*002C >> 6) =
00D0`, `93 + (FFFB*002C >> 6) = 008F`, then `00C7/0088` for the second pass.
Bound TGLS raw flight inputs remain authoritative through rattle and TGVN.
At frame 89 the production shot path applies this helper, overwrites the
persistent `$A0F3->$A790` TGGL chain, commits actor assignments, and guarantees
the ordinary-loop-eligible chosen actor's immediate `$002D` opcode-13 consume.
Other eligible shot actors consume the current `$A790/$A9DA` latch according
to their own reachable stream; `$0041` is excluded by the bounded source graph.
The TGLP proof reel includes independently replayed normal controller-B rattle
frames 1, 9, 17, 25, 33, 65, and 89. A source-shaped state-5 CPU route and held
direction on the non-shooting controller move through the ordinary production
update with no ball holder; frame 89 asserts the A9DA winner and `$0032`
post-opcode cursor. The fixture seeds route state, not shot outcome or phase.

TGVN-1 is the direct raw16 C translation of `$A8E9-$A976/$AA87-$AA9E`.
Negative Z shifts both planar components twice and alone admits the exact
absolute-X clamp; nonnegative Z shifts X only. Clamp uses
`$30+($006A&$0F)` and restores the pre-clamp sign before raw `$035A` forces X
nonnegative for 0 or negative for 1. The API defines arithmetic shift and
wrapping negate on uint16 bits, so host signed-shift behavior is irrelevant.
Bound non-legacy ordinary MISS playback now supplies TGLS launch velocity to
rattle and runs this normalizer after the saved pair is restored. Legacy/debug
fixtures retain an isolated synthetic sentinel. Frame 89 supplies the normalized
production values to the bounded LIVE A9DA event.

TGLS-1 translates the direct `$A0F3` object-10 launch as a pure typed helper.
Raw `$0463` direction remains 0..7 and `$006A` remains a separate explicit
second-`$C05D` result; only
`$006A >= $40` selects the exact `$A15C` remap before the four direction
tables. The solver imports the 256-byte `$BDF7` lift table through the
sanitized TGJS source span, computes `$B32C` duration/cap, and reproduces
`$80A9-$815A` signed-numerator/unsigned-duration truncation. Nonzero divisors
retain the full raw16 quotient before wrapped sign restoration; divisor zero
alone yields `0000/7FFF/8001`. The solver then
applies `$A0F3`'s wrapping quotient double. `$7D/$F2/$FD` is the same RAM as
object-10 `$73+X/$E8+X/$F3+X` at `X=$0A`, so one typed source coordinate owns
target base, delta origin, LUT index, and Q6 seed. `$B522-$B52D` proves the
zero-duration gate, `$BD6E-$BDC6` integrate/publish call, and post-call
decrement. The proven
non-legacy scene jump direction equals pre-remap raw `$0463`. The scene
captures object-10 from the pre-shot Q8 ball snapshot, calls tagged `$C05D`
sites `$9FA1` then `$A0DD` at MISS release, performs no launch tick there, and
first integrates on frame 3. Raw planar state is authoritative downstream;
rendered flight remains presentation-only until altitude composition is owned.
Pass flight separately calls tagged `$C05D` from `$B13F` after any substep
whose exact proximity/difficulty threshold admits the second rating check.

TGFR-1 pins fixed `$CD7A-$CD7F`, `$CD8F-$CD95`, and `$CD96-$CDAB`. It is a
one-shot native LIVE continuity checkpoint seeded at accepted PRETIP handoff,
not a claim that PRETIP reproduces the canonical earlier global stream.
Rejected scene updates restore RNG bytes and serials byte-exactly.
Exact call-edge anchors cover Bank05 `$A0DD-$A0DF` and pass `$B13F-$B1B8`
(`JSR $C05D` at `$B186`), plus fixed `$C05D-$C05F` (`JMP $CD96`). Other
LIVE `$C05D` callers remain outside this bounded ledger, so captured `$006A`
is a bounded native stream, not a claim of canonical global RNG parity.

Production automatic-pass selection now enters from made-score restart:
Bank05 `$901F` state 1 reaches Bank06 `$8661-$8727`, publishes the selected
primary at `$0168`, and `$8728-$8773` refreshes the other four offense streams
and final candidate. Tests cover both orientations, exclusions, strict ties,
equality/mismatch, and the retained pose-low/action portion of the `$88B0`
displaced-primary reset. Pose-high/sprite outputs are diagnostic-only; scene
standing direction is an explicit horizontal-facing approximation. Human offense
receives those writes but does not automatically execute `$0168`.

`--gameplay-cpu-possession-proof` retains its original four output operands and
accepts an optional score-restart frame directory. With that directory, TGPH-7
captures every 640x480 scene frame from marker onset through a locally observed
pass and caught-holder/marker-retirement endpoint (maximum 512). The wrapper
runs the proof twice, inventories every contiguous `frame-%06d.png`, encodes at
the native `39375000/655171` cadence, validates dimensions/rates/frame counts
with ffprobe, and requires identical frame-inventory and MP4 SHA-256 values.
These PNGs and MP4s are presentation evidence; structured state remains the
acceptance authority and all generated artifacts remain under ignored build
output.
The capture snapshots the selected primary/passer and published candidate
receiver on its first frame. Marker retirement is accepted only on an active
pass carrying that exact pair; swapped and self-pair negatives must reject.
Every active pass frame in the entire captured window retains the pair, even
before the retirement edge, and completion requires
ball holder, selected primary, and foundation last holder to equal the
snapshotted receiver. A global pass counter cannot satisfy this lineage.

The TGLP native proof renders four deterministic automatic-pass
checkpoints: opcode 5, retained opcode-6 action `$10`, packed `$32` gather,
and released flight. Its `cpu_auto_pass_stream` JSON pins the `$017C/$018B/
$0190` records, the opcode-3 wait sequence `6..0`, passer/receiver identities,
pass phases, and player/ball position deltas. Object-slot-10 `$13` is labeled
as an inference from the separate canonical executor and scene state-flow
tests; the live scene does not retain or observe that write. The existing
isolation fixture still parks `$017C`; it proves downstream presentation only
and is not evidence for the converted production `$0168` entry. The refreshed
frame-111 baseline was visually reviewed: unlike the retired empty-court
shortcut frame, it shows all ten actors in the scored-restart formation.

Unimplemented handler effects retain their source-pinned record transport
only where that transport is already bounded, with the separate reason
`unimplemented-handler`. This does not claim CPU play, pass, shot, or dynamic
link-policy parity. Bank05 `$86DD-$8798` makes the low bits nonzero/clears them
through shot and airborne-recovery lifecycles, while `$8FAD-$8FB9` admits its
ordinary possession transition only when `($BA & 3) == 0`. Fixed-bank
violation/restart paths use additional `$BA` flags, so the native condition
excludes those transient phases rather than treating a whole byte as known.
The accepted projection is only the zero low-two-bit branch at Bank06
`$92CA-$92D0`; it makes no claim about the rest of `$BA`. A future live owner
must be introduced as a typed lifecycle with its own capture/provenance tests;
raw RAM mirrors, clock/frame substitutions, and a synthetic `$BA` are
intentionally rejected.

Formation refresh quantizes the current selected ball handler into 64-pixel
X/depth buckets. A bucket change reloads only ordinary eligible actors,
retains both `$0308` and `$0309` lifecycles, and clears source target/direction
metadata only for the ordinary actors whose `$0547/$0551` cursor it replaces.
That matches Bank06 `$944D-$9465`. An unchanged bucket is a no-op. Automatic
selected defense is active on initial possession and after pass handoff. The
selected defender uses the source `+16/-16` orientation separation rather
than chasing the holder's exact coordinate.

The selected primary is excluded only from the later ordinary Bank06 loop:
`$8286 CPX $0308` / `$8289 BEQ $82A4` prevents duplicate dispatch after the
earlier `$8374->$83F3->$8491` selected flow. `--gameplay-live-foundation-proof
<PACK> cpu-primary-stream-step <PNG>` parks canonical opcode-4 at `$0000` on
automatic selected actor 0 and proves one production update advances exactly
to `$0005`, retains its source target, and does not double-step. The scene-state
suite separately proves a real PRETIP-to-home-CPU holder advances its non-pass
opcode once while nonselected actors continue moving. These regressions do not
claim complete play-selection policy or unsupported selected-primary gates.

`tools/Invoke-CpuBallTargetOpcode4Proof.ps1 -RomPath <LOCAL_ROM.nes>` creates
an ignored two-run production proof under `build/cpu-ball-target-opcode4-proof`.
It renders the normal LIVE path and records that the canonical `$0000`
opcode-4 record has `C8=$0A`, resolves target object `10`, and uses the same
immutable ball snapshot coordinate as the stored source target. The screenshot
is integration evidence only; the handler semantics remain anchored by the
TGAI source span, focused executor tests, and source-map provenance above.

Opcode 10's `$8D59-$8E21` scaling inputs have the typed production owner
described above. Bank04 `$ACD9-$ACE3` loads fixed table `$ADD6-$ADDF` into
`$06CB`; `$AD26-$AD58` owns the primary timer/rate scaling. Dynamic
`$037F/$07DF` remains separate. The remaining nearby exact boundary is opcode 12's upstream command
reachability: its bounded executor is exact, but the imported formation graph
does not establish how normal play selection reaches its sole record.

## Regulation and overtime `$85EA` entry lifecycle

The real first-period PRETIP-to-LIVE handoff and each accepted P2-P4/OT banner
perform the surviving Bank06 `$86D2/$85EA` transaction after role/foundation
synchronization and before the first ordered LIVE update. Fixed `$E74F`
selects Bank06 before calling `$85EA`; Bank05 `$85EA` is repeated data and is
pinned separately. Each exact transaction selects the holder as primary,
scans the four same-side nonprimary actors in descending slot order, derives
the first two streams with `$8774` from their immutable pre-seed depths,
assigns fixed `$0208/$0195` to the last two, writes state 4, and publishes the
scan's final candidate to the typed `$06DA` equivalent. The primary exposes
only final cursor `$017C` and state 4; P1 also exposes wait 0, while P2-P4
retain the incoming wait. Temporary `$0168` is not observable.

The banner path follows fixed `$E71B`: equality takes Bank05 `$8F97` without
swapping roles, while mismatch takes ordinary-admitted `$8FAD` and swaps the
side and selected pairs exactly. Both converge on `$8FE8`; `$BFA8` clears
owned `$046E,X` for all ten actors, and the selected primary/defender receive
state 0 and packed action `$30` before `$85EA` writes the offense state/cursor.
One public transaction owns role resolution and seed together, so callers
cannot publish or replay a half-entry. It does not broadly clear typed target,
direction, route, or wait planes. P1 alone writes the initial primary wait 0;
P2-P4 and overtime preserve every wait byte, including the newly selected
primary.

`$0758` is a distinct binary selector `R`, not the P1 holder/offense side.
Fixed `$E537-$E542` derives it from `$04FC&$80`. The source chain begins at the
tip claimant `$A2A4/$A2B5`, follows coordinate selection `$A2B8-$A2C6`, solver
`$A2C8-$A2CA`, the `$A2CD->$AA84` halving path and state-17 publish
`$A2D0-$A2D2`, then fixed `$E51B-$E52F` waits for state 17 before
`$E537-$E53F` projects the bit. Together with two natural no-write outcomes
(P1 offense 0: `$04FC=FF,R=1`; P1 offense 1: `$04FC=00,R=0`), this proves the
typed production mapping `R = P1 tip winner/offense ^ 1`.
The regulation-entry transaction receives `R` as a distinct typed argument:
the scene derives P1 only from the committed PRETIP claimant/winner and checks
winner, claimant team, possession, and `R=winner^1` together; P2-P4 pass and
validate the stored cumulative selector. A mismatched selector or holder-side
substitution rejects the entire transaction byte-identically.

At fixed `$E71B`, `$035C` indexes the bytes beginning at `$E740`. P1 increments
`$035C` to 1 but seeds separately. P2 uses X=2 and XOR 0, P3 uses X=3 and XOR
0, and P4 uses X=4 and XOR 1, yielding targets `R,R,R^1`. Fixed `$E5E9` writes
`$035C=5` on every overtime entry, so X=5 always reads `$E745=01`; each OT
toggles the cumulative selector. Therefore odd OT targets `R` and even OT
targets `R^1`. Every period takes ordinary equality when that binary selector
equals current offense and ordinary mismatch otherwise; there is no parity
force-swap and no `$A8/$A9` selector state.

The public OT transaction uses `(period=5,overtime_count)` as its epoch key.
The last applied OT count and shared monotonic seed serial reject duplicate,
decreasing, skipped, and post-`UINT8_MAX` calls byte-identically. A non-tied
final transition publishes no new seed. Regulation and OT each expose only an
atomic role/reset/seed API, so no caller can publish a half-entry.

`Run-GameplayCpuSteeringTests.ps1` pins the canonical Rev1 raw anchors
independently of the copied TGAI payload: fixed `$E5E9-$E61D` (`5B32743D`),
Bank05 `$A2A4-$A2D5` (`ED9BAB3B`) and fixed `$E51B-$E548`
(`145DE16E`) for the two-outcome P1 selector source contract,
fixed direct index bytes `$E73F-$E747` (`6447E4ED`, exactly
`4C 47 E7 00 00 01 01 01 A9`),
fixed `$E71B-$E756` including the
`$E740` JMP-operand/table overlap (`63D4F5A3`), Bank05 `$8F97-$8FAC`
(`62809A8D`), `$8FAD-$8FE7` (`7C94E5EA`), `$8FE8-$902D` (`FFA12025`), and
`$BFA8-$BFC8` (`7AD3EC16`). Values are FNV1a32 over the inclusive range.

The coordinate tables seed the primary at `027B,94` or `0085,94` and the
first two descending teammates from the exact side table. Those primary
points intentionally lie just beyond the ordinary trapezoid. A typed
period-entry exemption owns source bit `$0588&08` only for that selected
primary, admits only the narrow seed-to-boundary re-entry corridor, and feeds
TGMO flag `$08`. Human movement expires it on natural re-entry; the automatic
opcode-5/pass route stays at the staging coordinate until catch changes the
primary, when the former primary takes the ordinary secondary clamp. Opcode-5 facing is
not composed into native locomotion during this staging lifecycle, matching
the retained source position through opcode 6/pass. Generic possession
changes, non-banner restarts, fouls, inbounds, malformed flags, and duplicate
or out-of-order period epochs remain outside this owner.
