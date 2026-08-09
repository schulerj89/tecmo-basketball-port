# CPU steering evidence boundary

Ordinary live CPU actors now use the transactional TGAI-to-TGMO adapter. This
note describes the exact Rev 1 routines isolated behind
`gameplay/cpu-steering` TGAI-1 and the bounded native target policy that feeds
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

TGAI-1 is 7616 bytes with FNV1a32 `D6C4DB35`. It requires exact same-pack
TGMO-1 (`gameplay/movement`, 1664 bytes, `6C82A137`) so direction identities
cannot silently diverge from the locomotion boundary. Its ten Rev 1 source
spans are:

| Source | Range | Bytes | FNV1a32 |
| --- | --- | ---: | --- |
| Bank06 actor loop/state dispatch | `$81F7-$82D3` | 221 | `23BB7271` |
| Bank06 actor-to-reference direction | `$87AE-$88AF` | 258 | `F866B06C` |
| Bank06 target-delta direction/motion | `$88DA-$8A95` | 444 | `9616E586` |
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
For the ball holder it uses the scene's implementation-owned hoop
approach (`48/48/40` pixels for difficulty `0/1/2`); orientation 0 targets the
left hoop and orientation 1 the right. Every other selected actor targets the
explicit opposing linked/matchup actor supplied by the caller. That link is a
typed harness input, not a claim that the ROM's `$06CB,X` assignment policy has
been reconstructed. Only the final TGAI target-minus-actor octant is ROM-exact.

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

For the live ball holder, the committed TGMO direction and animation phase also
feed strict TGBD-1 held-ball geometry. This makes CPU and human holders share
the same ROM-derived bounce phases and ground-contact sound trigger. TGBD's
height and attachment tables are exact; the scene's fixed linked actor and its
visible-Y-before-TGCP adapter remain native policy.

The exactness boundary has one important seam: TGAI's zero-vector guard means
"do not write direction," while TGMO consumes held controller-direction bits.
There is no exact bit value implied by that guard. The shared adapter supplies
neutral as a native policy and still allows TGMO's one-update latency to run.
The initial offensive-facing direction, explicit rating/condition values,
holder hoop approach, and linked target also remain harness-owned inputs or
policy. Neither the neutral choice nor those policies are presented as ROM CPU
command behavior.

Run:

```powershell
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build -RomPath <LOCAL_ROM.nes>
.\build\tecmo_port.exe --gameplay-cpu-steering-test <PACK>
.\build\tecmo_port.exe --gameplay-cpu-steering-inspect <PACK> <OFFSET> <DX> <DY>
.\build\tecmo_port.exe --gameplay-cpu-steering-harness <PACK> <ACTOR> <POSSESSION> <ORIENTATION> <HOLDER> <MATCHUP> <DIFFICULTY> <X0,Y0> ... <X9,Y9>
.\build\tecmo_port.exe --gameplay-cpu-steering-movement-harness <PACK> <ACTOR> <POSSESSION> <ORIENTATION> <HOLDER> <MATCHUP> <DIFFICULTY> <RATING> <CONDITION> <SPEED> <FRAMES> <X0,Y0> ... <X9,Y9>
```

These commands are developer tooling only and never add an in-game debug mode;
normal play calls the pure API directly.

## Deliberate limits and next integration

TGAI-1 does not claim a complete CPU play policy. In particular, it does not
identify the shot/pass/steal selector, reconstruct every actor-link assignment,
own live collision/contact or speed-setting policy, or treat the nearby Bank06
`$B081-$B32E` candidate scan as ordinary movement targeting. That scan is now
converted separately as the per-frame receiver/defensive-switch selector; it
does not change the TGAI movement-target boundary. Fatigue evolution is owned
separately by TGFT-1 and supplies condition to TGMO. Handler-effect
names describe bounded entry behavior; they are not play names.

The scene now owns a fixed opposing roster-slot link, an explicit target
position, direction/write result, immutable-snapshot fingerprint, and
monotonically advancing decision serial for each actor. All non-controlled
candidates are evaluated from the same post-human-input snapshot and committed
together, then their held direction is advanced through TGMO. The offensive
holder takes the primary path; every other actor takes the secondary path.

The ball holder uses the orientation-aware `48/48/40` hoop approach. Offensive
non-holders use five scene-owned formation points (`256,148`; `288,112`;
`288,184`; `352,96`; `352,200`) and mirror X as `767-X` for the other
orientation. Defenders target a point 32 pixels goal-side of their linked
offensive actor, with per-slot court-depth splits `0,-10,10,-14,14`. When the
goal-side candidate leaves the shaped court at that depth, the adapter uses the
equal 32-pixel offset toward the court before its final bounds check. The fixed
link remains pose/facing and defender-reference metadata instead of forcing two
actors onto one coordinate. All of these target choices are explicit native
approximations; only the resulting TGAI octant and TGMO movement step are
ROM-exact. Shot proximity and cadence remain separate native policy.

The live state deliberately carries a no-command sentinel and no pending
advance. It does not fabricate actor-local ROM stream offsets or claim that a
fixed roster matchup reconstructs `$06CB,X`. Replacing these native explicit
targets with the original formation/play command offsets, dynamic reference/link
assignments, target fields, and command-advance transitions is the next CPU
policy slice.
