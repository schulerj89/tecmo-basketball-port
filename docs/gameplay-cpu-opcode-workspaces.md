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
state proof; no opcode-10 visual claim is made while LIVE remains deferred.

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

## What the harness proves

| Area | Exact bounded conversion | First missing live dependency | LIVE disposition |
| --- | --- | --- | --- |
| Opcode 7 | Bank06 `$8F12-$8F29`: compare `C9` with `$046E[C8]`, then choose current `+5` or `CA/CB +5`; canonical records use object slot `C8=$0A`. | Slot-10/ball-object `$046E` lifecycle at the exact command point. Actor timers and ball coordinates are not substitutes. | Deferred/diagnostic-only. |
| Opcode 10 | Bank06 `$8D59-$8E21` plus `$8E22-$8E4E`: orientation hoop delta, signed normalization, primary/non-primary threshold branches, and exact 1/2/3/4-bit shift entries. Canonical `$8DCE` is `BCC $8E03` (`90 33`): the zero-`$0798`, threshold-`<$6A` primary branch takes three shifts with no reload/decrement, despite a misleading lifted label. The harness names the Bank06 CPU X-register actor selector `actor_index`, so it cannot be mistaken for a court X coordinate. | For a primary resolved link, exact `$0798/$075F/$6A/$0760` launch/RNG cadence. The non-primary branch does not read those fields. | Ordinary LIVE projects the exact non-primary workspace from the typed linked actor position and hoop anchor. Primary links remain fail-closed. |
| Opcode 10 selector | Bank02 `$BEE7-$BFD8`: seed `$99=$FF`; apply the `$0588` and `$0478/BA` gates; find the first descending bit-`$10` actor whose `$06CB==$0308`; form the source-width X/depth window; then scan actors 9..0, excluding `$0309` and the initial actor. Equal distances replace the prior candidate, so the lowest tied slot wins. | Nonordinary `$0478` contexts and any selector no-store/retained lifecycle. | Ordinary `$0478==0` LIVE uses TGBC `frontcourt_established` for the sole `$0588` bit-`$10` producer and typed foundation roles/flags/links/positions. Only actual source stores are projected. |
| Opcode 16 | Bank05 `$9054-$90AF`: absolute hoop-X/depth workspaces. Bank06 `$9085-$90D7`: the existing executor can use them only when the pointer target and caller timing are proven. | Proof that Bank05 ran for the relevant actor immediately before the command; plus `$0309` pointer ownership. | Pure harness only. |
| `$BA` | Bank06 target application consumes only `BA & 3`; the harness exposes that mask without a clock. | The cross-bank mutable lifecycle: bits 0..1 change in Bank05 state/possession paths and gate Bank06 formation/target paths; other bits have independent meanings. | External-lifecycle diagnostic; never `frame & 3`. |

## Exact LIVE boundary

Headless source analysis establishes Bank05 `$9737-$973C` as the sole
`$0588` bit-`$10` producer; `$B534-$B539` masks with `$BF` and therefore
preserves bit `$10`. TGBC's exact Bank05 `$971F-$9786` state already owns that
frontcourt predicate. Because AI dispatch precedes authoritative backcourt
settlement, the scene previews TGBC transactionally with the current floored
ball coordinate and consumes only the previewed `frontcourt_established` bit.
It neither commits that state nor adjudicates a previewed violation.

The production selector supports only ordinary `$0478==0`. It supplies typed
foundation roles, flags, dynamic links, and positions to the pure Bank02
harness. A candidate or explicit-`$FF` store becomes a LIVE owner; final
`$99==$FF` no-store/retention remains unavailable, so no fabricated prior
`$07DF` can leak into Bank06. Nonordinary `$0478=$13,BA=$24` is outside this
seam even though its source path explicitly stores `$FF`.

For each actor, LIVE resolves the exact `$0308/$06CB` branch. When that link is
not primary, Bank06 chooses its shift solely from hoop-distance thresholds and
does not read `$0798/$075F/$6A/$0760`; the existing harness therefore supplies
the exact signed relative workspace. A primary link still defers at
`missing-linked-relative-workspace`. Bank04 `$AD26-$AD56` has context-dependent
timer/rate initialization (`$69&1`, `$0794`, or `$6006/$6007`) and `$6A` has an
RNG lifecycle, so observed zeros are not promoted to typed owners. Raw RAM is
not mirrored, and opcode 15 remains disabled and unchanged.

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

- opcode-7 missing `$046E` evidence defers, including transactional rejection
  of malformed availability bits;
- opcode-10 primary reload/decrement, the canonical three-shift sample branch,
  large-window scaling, `$50` and sub-`$50` branches, non-actor `$07DF`
  sentinel routing, signed restoration, and transactional invalid input;
- Bank02 opcode-10 selector explicit-`$FF` gates, descending initial-link
  choice, candidate exclusion and tie replacement, retained prior `$07DF` on
  the final no-store path, source-width window rejection, and transactional
  actor-index/coordinate validation;
- ordinary-LIVE current-ball frontcourt preview without TGBC mutation,
  actual-store-only selector projection, retained/no-store rejection, exact
  non-primary workspace projection, and primary-link fail-closed behavior;
- opcode-16 left/right absolute workspace arithmetic and transactional invalid
  coordinates;
- `$BA` low-bit masking preserves its external lifecycle rather than creating
  cadence.

The runner also verifies the canonical corpus still has two opcode-7 records,
one opcode-10 record, and two opcode-16 records; opcode 7 remains the slot-10
probe and both opcode-16 records remain the `$0309` pointer form.
