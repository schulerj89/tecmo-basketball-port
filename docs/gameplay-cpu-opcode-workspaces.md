# CPU opcode workspace boundary

`gameplay-cpu-opcode-workspaces` contains a strict harness for three Bank06
command handlers. One narrow opcode-10 projection is linked into LIVE; the
remaining capture-only handlers do not feed `TecmoGameplayCpuSteeringPlayInput`.

Run it with a private canonical Rev1 ROM:

```powershell
.\tools\Run-CpuOpcodeWorkspaceTests.ps1 -RomPath <LOCAL_ROM.nes>
```

The runner validates the canonical iNES SHA-256 before it checks the listed
source range fingerprints and command-corpus facts. It neither writes the ROM
nor copies ROM/table bytes into the repository. The output is deterministic
state proof. Production movement/render coverage belongs to the existing LIVE
scene proof, which emits deterministic frames and native-cadence video.

## Provenance

| Purpose | Canonical source | Bytes | FNV-1a32 |
| --- | --- | ---: | --- |
| Opcode 7 `$046E[C8]` branch | Bank06 `$8F12-$8F29` | 24 | `495E0788` |
| Opcode 10 gate/helper/follow-up | Bank06 `$8CD0-$8ED3` | 516 | `5661731D` |
| Opcode 10 orientation-anchor table | Bank06 `$9C97-$9C9A` | 4 | `A27B0F6F` |
| `$07DF` eligible-candidate proof | Bank02 `$BEE7-$BFD8` | 242 | `C1B08476` |
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

## What the harness proves

| Area | Exact bounded conversion | First missing live dependency | LIVE disposition |
| --- | --- | --- | --- |
| Opcode 7 | Bank06 `$8F12-$8F29`: compare `C9` with `$046E[C8]`, then choose current `+5` or `CA/CB +5`; canonical records use object slot `C8=$0A`. | Slot-10/ball-object `$046E` lifecycle at the exact command point. Actor timers and ball coordinates are not substitutes. | Deferred/diagnostic-only. |
| Opcode 10 | Bank06 `$8D59-$8E21` plus `$8E22-$8E4E`: orientation hoop delta, signed normalization, primary/non-primary threshold branches, and exact 1/2/3/4-bit shift entries. Canonical `$8DCE` is `BCC $8E03` (`90 33`): the zero-`$0798`, threshold-`<$6A` primary branch takes three shifts with no reload/decrement, despite a misleading lifted label. The harness names the Bank06 CPU X-register actor selector `actor_index`, so it cannot be mistaken for a court X coordinate. | Exact `$07DF/$0478/$06CB/$0308` entry selection and caller-timed target workspace/continuation. `$07DF` itself is a comparison byte and may be a non-actor sentinel; only the selected `$0308`/`$06CB` target is dereferenced. | LIVE resolves only a bit-clear actor whose typed dynamic link is not `$0308`; all other cases defer. |
| Opcode 16 | Bank05 `$9054-$90AF`: absolute hoop-X/depth workspaces. Bank06 `$9085-$90D7`: the existing executor can use them only when the pointer target and caller timing are proven. | Proof that Bank05 ran for the relevant actor immediately before the command; plus `$0309` pointer ownership. | Pure harness only. |
| `$BA` | Bank06 target application consumes only `BA & 3`; the harness exposes that mask without a clock. | The cross-bank mutable lifecycle: bits 0..1 change in Bank05 state/possession paths and gate Bank06 formation/target paths; other bits have independent meanings. | External-lifecycle diagnostic; never `frame & 3`. |

## Narrow LIVE opcode-10 projection

Here it is: Bank02 `$BF87-$BF8C` admits only `$04B0` bit-`$10` actors before
`$BFD5` can write `$07DF`; its failure path writes `$FF`. A LIVE actor whose
typed `actor_selector_flags` bit is clear therefore cannot be `$07DF` and must
take Bank06's `$06CB,X` route. `TecmoGameplayLiveFoundation.dynamic_link` is
the existing typed owner used by the exact Bank05 `$B317` and `$B8F6` scans,
including pass/claimant updates. This proves the branch without adding a raw
RAM mirror.

For a resolved link other than typed `$0308`, `$8DDD-$8E1B` chooses its shift
from the linked actor's hoop-relative magnitude and does not read
`$0798/$075F/$006A/$0760`. LIVE therefore calls the exact workspace harness
and supplies that resolved link/vector to the opcode executor. The executor no
longer substitutes `fixed_link_target` on this path.

The boundaries remain fail-closed:

- bit-`$10` actors may equal `$07DF` and retain
  `missing-special-actor-07df`;
- a resolved dynamic link equal to `$0308` needs the unowned timer/rate/sample
  lifecycle and retains `missing-linked-relative-workspace`;
- opcode 15 remains disabled and unchanged;
- `$0478`, the complete `$07DF` lifecycle, and raw RAM are not mirrored.

`capture_complete` in the assessment means that a debugger/harness supplied
all required owners at one command point. It deliberately does **not** mean a
native scene has a faithful live producer. `live_producer_available` remains
false for every result from this module, preventing a complete capture from
silently becoming a production-parity claim.

## Test coverage

The standalone test executable checks:

- opcode-7 missing `$046E` evidence defers, including transactional rejection
  of malformed availability bits;
- opcode-10 primary reload/decrement, the canonical three-shift sample branch,
  large-window scaling, `$50` and sub-`$50` branches, non-actor `$07DF`
  sentinel routing, signed restoration, and transactional invalid input;
- LIVE bit-clear/non-primary dynamic-link projection, bit-`$10` special-actor
  defer, primary-link timer defer, and transactional invalid input;
- opcode-16 left/right absolute workspace arithmetic and transactional invalid
  coordinates;
- `$BA` low-bit masking preserves its external lifecycle rather than creating
  cadence.

The runner also verifies the canonical corpus still has two opcode-7 records,
one opcode-10 record, and two opcode-16 records; opcode 7 remains the slot-10
probe and both opcode-16 records remain the `$0309` pointer form.
