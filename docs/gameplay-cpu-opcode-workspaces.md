# CPU opcode workspace boundary

`gameplay-cpu-opcode-workspaces` is a strict harness for three Bank06 command
handlers. It does not feed production `TecmoGameplayCpuSteeringPlayInput`.

Run it with a private canonical Rev1 ROM:

```powershell
.\tools\Run-CpuOpcodeWorkspaceTests.ps1 -RomPath <LOCAL_ROM.nes>
```

The runner validates the canonical iNES SHA-256 before it checks the listed
source range fingerprints and command-corpus facts. It neither writes the ROM
nor copies ROM/table bytes into the repository. The output is deterministic
state proof; LIVE uses the same pure opcode-10 workspace only through the
typed runtime/scene lifecycle described below.

## Provenance

| Purpose | Canonical source | Bytes | FNV-1a32 |
| --- | --- | ---: | --- |
| Opcode 7 `$046E[C8]` branch | Bank06 `$8F12-$8F29` | 24 | `495E0788` |
| Opcode 10 gate/helper/follow-up | Bank06 `$8CD0-$8ED3` | 516 | `5661731D` |
| Opcode 10 orientation-anchor table | Bank06 `$9C97-$9C9A` | 4 | `A27B0F6F` |
| `$07DF` candidate/retention evidence | Bank02 `$BEE7-$BFD8` | 242 | `C1B08476` |
| Opcode 16 pointer adjustment | Bank06 `$9085-$90DF` | 91 | `EBDD5956` |
| Target-application `$BA&3` tail | Bank06 `$92BA-$9314` | 91 | `087BF69F` |
| `$036E/$0370` arithmetic producer | Bank05 `$9054-$90AF` | 92 | `FE092D62` |
| `$BA` actor-state lifecycle evidence | Bank05 `$86BB-$879A` | 224 | `15CFFC00` |
| `$BA` possession-gate lifecycle evidence | Bank05 `$8FAD-$8FE7` | 59 | `7C94E5EA` |
| `$BA` formation-gate lifecycle evidence | Bank06 `$943B-$9465` | 43 | `D9664D46` |
| `$BA` round-setup lifecycle evidence | Bank06 `$9621-$9764` | 324 | `F2543C57` |
| Five-byte command corpus | Bank04 `$9F2E-$AC75` | 3400 | `71331A96` |

The existing TGAI-3 asset already pins the broader Bank06 handler and Bank04
command spans. The private-ROM runner adds only narrow provenance checks for
the caller-local helper and `$BA` lifecycle that TGAI-3 does not claim to own.

The adjacent automatic-pass chain is an executor/scene contract, not a new
workspace in this module. Its exact records are opcode 5 at offset 380
(`$A0AA`), opcode 23 at 395 (`$A0B9`), and opcode 6 at 400 (`$A0BE`). Tests
park the selected-primary cursor at opcode 5 because the upstream play-selection
route is not yet converted; they do not skip or claim natural reachability past
the intervening opcode 9, opcode 3, or its six-update wait.

Opcode 13 is an executor input contract rather than a workspace owned by this
module. Bank06 `$9125-$9145` consumes persistent raw 16-bit words at
`$038D:$038E` and `$038F:$0390`, subtracting actor X and zero-extended actor
depth with source wrapping, and then enters the shared `$92CB` tail. Neither
high byte is a court-coordinate validity field. The command corpus contains the
two exact zero-argument records at offsets 45/65 (`$9F5B/$9F6F`), each followed
by opcode 14 and opcode 15. TGGL-1 now owns a dedicated four-byte raw16
representation with five producer kinds (`$A0F3`, `$A790`, `$A9DA`, `$B721`,
`$B783`), atomic last-writer-wins updates, immutable snapshots, monotonic
serial admission, full-reset-only clear, and explicit retention across periods
and possessions. Construction is one-shot and accepts only byte-zero virgin
storage, so it cannot bypass reset serial admission. This is a provenance
model, not a production binding. Unit
fixtures may supply its raw words; LIVE does not, because the object/event
schedulers, `$A214` gates, opcode 15, and exact latest-writer timing remain
unowned. The current ball and TGCA's one-frame opcode-20 capability are not
substitutes, so LIVE remains `missing-global-target`.

The `$002D` opcode-13 record is source-linked to the anchored
`$A9DA->$A993` family. The later `$0041` record retains an explicit
latest-writer/scheduling boundary; neither relationship authorizes native
scene input. Exact importer anchors cover all five writer spans, Bank06
`$9125-$9145`, and fixed `$CC30-$CC85` reset/page clear, with independent
mutation rejection for every span.

## What the harness proves

| Area | Exact bounded conversion | First missing live dependency | LIVE disposition |
| --- | --- | --- | --- |
| Opcode 7 | Bank06 `$8F12-$8F2C`: compare `C9` with `$046E[C8]`, then choose current `+5` or `CA/CB +5`. The only records are offset 315 / `$A069` (`07 0A 00 36 01`) and offset 370 / `$A0A0` (`07 0A 00 68 01`); slot `$0A` means object-slot-10 state `$0478`. | `$0478` at the exact command point. Actor timers and ball coordinates are not substitutes. | Selected-primary prepass only: ordinary selector context proves `$0478==0`, so both records take current `+5` and state 4. Ordinary 9..0 dispatch remains unavailable because opcode 6 can change `$0478` mid-traversal. |
| Opcode 10 | Bank06 `$8D59-$8E21` plus `$8E22-$8E4E`: orientation hoop delta, signed normalization, primary/non-primary threshold branches, and exact 1/2/3/4-bit shift entries. Canonical `$8DCE` is `BCC $8E03` (`90 33`): the zero-`$0798`, threshold-`<$6A` primary branch takes three shifts with no reload/decrement, despite a misleading lifted label. The harness names the Bank06 CPU X-register actor selector `actor_index`, so it cannot be mistaken for a court X coordinate. | Primary links require a valid single-frame runtime binding of `$0798/$075F/$6A/$0760`; the non-primary branch does not read them. | Ordinary LIVE projects both branches when their typed owners are available. Missing/malformed primary context remains fail-closed. |
| Opcode 10 selector | Bank02 `$BEE7-$BFD8`: seed `$99=$FF`; apply the `$0588` and `$0478/BA` gates; find the first descending bit-`$10` actor whose `$06CB==$0308`; form the source-width X/depth window; then scan actors 9..0, excluding `$0309` and the initial actor. Equal distances replace the prior candidate, so the lowest tied slot wins. | Nonordinary `$0478` contexts and any selector no-store/retained lifecycle. | Ordinary `$0478==0` LIVE uses TGBC `frontcourt_established` for the sole `$0588` bit-`$10` producer and typed foundation roles/flags/links/positions. Only actual source stores are projected. |
| Opcode 16 | Bank05 `$9054-$90AF`: absolute primary-to-hoop-X/depth workspaces. Fixed `$F031` calls Bank05 `$81F2` once per gameplay loop; `$8209-$8217/$833B` snapshots `$0308` before source player movement, and every later Bank06 `$9085-$90D7` invocation shares the result. | A tagged pre-motion scene-frame capture plus typed `$0309` play-state ownership. | Ordinary LIVE captures once before controlled movement and binds the immutable workspace once before selected/descending dispatch. Absent input defers; malformed input rejects transactionally. |
| `$BA` | Bank06 target application consumes only `BA & 3`; the harness exposes that mask without a clock. | The cross-bank mutable lifecycle: bits 0..1 change in Bank05 state/possession paths and gate Bank06 formation/target paths; other bits have independent meanings. | External-lifecycle diagnostic; never `frame & 3`. |

## Exact LIVE boundary

Headless source analysis establishes Bank05 `$9737-$973C` as the sole
`$0588` bit-`$10` producer; `$B534-$B539` masks with `$BF` and therefore
preserves bit `$10`. TGBC's exact Bank05 `$971F-$9786` state already owns that
frontcourt predicate. Because AI dispatch precedes authoritative backcourt
settlement—and controlled actors move before the stored Q8 ball is
reattached—the scene first resolves the held-ball/dribble coordinate over the
same immutable post-human actor snapshot used by AI. It previews TGBC
transactionally with that current coordinate and consumes only the previewed
`frontcourt_established` bit. It neither commits that state nor adjudicates a
previewed violation. The same current snapshot supplies PlayInput's ball
coordinate, so opcode-4 ball targeting cannot observe the older attached Q8.

The production selector supports only ordinary `$0478==0`. It supplies typed
foundation roles, flags, fixed `$06CB` links, and positions to the pure Bank02
harness. A candidate or explicit-`$FF` store becomes a LIVE owner; final
`$99==$FF` no-store/retention remains unavailable, so no fabricated prior
`$07DF` can leak into Bank06. Nonordinary `$0478=$13,BA=$24` is outside this
seam even though its source path explicitly stores `$FF`.

For each actor, LIVE resolves the exact `$0308/$06CB` branch using the fixed
startup pairing `{5,6,7,8,9,0,1,2,3,4}`. Dynamic `$07DF` remains a distinct
selector result. When the fixed link is
not primary, Bank06 chooses its shift solely from hoop-distance thresholds and
does not read `$0798/$075F/$6A/$0760`; the existing harness supplies the exact
signed relative workspace without consuming a timer.

Primary links use a typed, runtime-owned fixed cadence. Runtime initialization
sets the persistent timer `$0798`, counter `$54:$53`, and sample `$6A` to zero.
Each runtime player update increments the low counter with carry, then runs the
fixed `$CD9C` left-shift/`$1D` feedback and zero fallback before mode dispatch.
A valid preseason or season launch stages exactly one `$CD96` xor/remix before
scene launch and commits it only when launch succeeds. Preseason binds
`$075F=difficulty,$0760=0`; season binds `$075F=2` and
`$0760=low8(game_index>>5)`. The timer persists across game end and later
launches. This is a supported-slice owner, not a raw RAM mirror, and the
separate TPTI bridge is not the opcode-10 RNG source.

Before each scene update, the runtime binds one tagged frame context containing
the stable sample, current timer, rate, and bias. The context is consumed after
that update attempt, including failure. Projection is non-mutating. Only an
actually fetched, nondeferred opcode 10 or admitted opcode 12 commits its pending timer result, in
selected-primary then ordinary actor `9..0` order; later commands in the same
update see the earlier committed candidate timer. Other opcodes, skipped,
deferred, and non-primary paths do not consume it. A failed scene update does
not publish the candidate timer back to the runtime. Absent or malformed frame
context therefore keeps only primary links at `missing-linked-relative-workspace`.
Raw RAM is not mirrored, and opcode 15 remains disabled and unchanged.

Opcode 16 has a separate ephemeral scene owner. Fixed `$F031` calls Bank05
`$81F2` once per gameplay loop. Its unconditional `$8209-$8217` primary load,
`$833B` position snapshot, and `$9054-$90AF` arithmetic run before source
primary movement. Native therefore captures the typed primary actor,
orientation, and position at the start of `scene_update_live_action_ordered`,
before `scene_move_controlled_actor`. The pure harness validates and produces
`$036E/$0370`; `scene_update_ai` binds those values once to the shared play
input before selected-primary and ordinary `9..0` traversal. Both canonical
`AC35/AC44` records (`10 09 03 00 00`) see the same immutable values even if a
controlled primary crosses the X-versus-depth comparison boundary afterward.
There is no persistent workspace, raw-RAM mirror, current-position recompute,
or per-actor recompute. An absent frame context retains
`missing-pointer-workspace`; a malformed available context rejects without a
partial scene commit.

A natural read-only observation provides non-authoritative corroboration:
all 3,937 observed opcode-10 entries followed an actual candidate or explicit
`$FF` selector store; no retained/no-store selector fed opcode 10. Entry values
also showed `$0798=$075F=$0760=0` and nonzero `$6A`, but those observations are
not treated as universal source proof for the primary-link branch.

`capture_complete` in the assessment means that a debugger/harness supplied
all required owners at one command point. It deliberately does **not** mean a
native scene has a faithful live producer. `live_producer_available` remains
false for every result from this module, preventing a complete capture from
silently becoming a production-parity claim.

## Test coverage

The standalone test executable checks:

- opcode-7 exact records/arguments, both selected-primary zero advances,
  nonzero loop/rewind harness branches, ordinary missing-probe deferral,
  selected-capture nonleakage, opcode-6-before-opcode-7 traversal isolation,
  and transactional late-scene failure;
- opcode-10 primary reload/decrement, the canonical three-shift sample branch,
  large-window scaling, `$50` and sub-`$50` branches, non-actor `$07DF`
  sentinel routing, signed restoration, and transactional invalid input;
- Bank02 opcode-10 selector explicit-`$FF` gates, descending initial-link
  choice, candidate exclusion and tie replacement, retained prior `$07DF` on
  the final no-store path, source-width window rejection, and transactional
  actor-index/coordinate validation;
- ordinary-LIVE post-human held-ball/dribble projection while the stored Q8
  remains stale, current-ball frontcourt preview without TGBC mutation,
  actual-store-only selector projection, retained/no-store rejection, exact
  non-primary workspace projection, primary rate/reload branches, serial
  actual-command timer commits, transactional malformed binding, and
  single-use frame context consumption;
- opcode-12 `$006E/$9F9C` record identity, exact close `-8..+7` axes,
  close/non-close and linked-primary-state-5 cursor outcomes, opcode-11 pose
  composition, scoped automatic-offense/defender admission, target publication,
  timer commit, transactional `BA&3` values 1/2/3, missing-context defer, and
  late-scene rollback. The exhaustive
  formation command graph records that upstream reachability is not owned;
- opcode-16 left/right absolute workspace arithmetic, transactional invalid
  coordinates, exact dual canonical-record execution from one pre-motion
  capture, X/depth branch flip proof, absent-context defer, and malformed
  context rollback;
- `$BA` low-bit masking preserves its external lifecycle rather than creating
  cadence.

The runner also verifies the canonical corpus still has two opcode-7 records,
one opcode-10 record, and two opcode-16 records; opcode 7 remains the slot-10
probe and both opcode-16 records remain the `$0309` pointer form.
