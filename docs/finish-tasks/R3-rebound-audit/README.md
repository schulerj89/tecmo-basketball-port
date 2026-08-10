# R3: Rebound eligibility audit (TGRB-1)

## Status

This is deliberately a fail-closed audit, not a rebound-stat port. Native C
does not currently retain the original raw direct-carom predicate or the
fresh `$BE/$BF` ownership used at the fixed counter call. Therefore counter 8
is still unsupported:

- `TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE` remains `0x003F`; REB bit 8 is
  absent.
- The generic game-ledger mutation rejects counter 8.
- TEAM DATA and League Leaders continue to render the unsupported rebound
  value as `---`; no TTDT rating is used as a statistic.
- TGRB-1 can report a fully source-gated *non-emitting* decision in a unit
  vector, but production never supplies the missing raw inputs and never
  writes a ledger.

## Revision-locked source boundary

TGRB-1 imports and parses these exact Rev1 source spans. Both FNV-1a32 and
FNV-1a64 are checked by the importer and parser, along with switched/fixed
bank placement, CPU addresses, byte counts, descriptor ordering, reserved
bytes, and copied raw data.

Its complete serialized payload is also revision-locked as 816 bytes with
FNV-1a32 `D6363FBD` and FNV-1a64 `B6B95695306094BD`. This rejects any changed
descriptor or raw-source byte before a partial asset object is published.

| Source purpose | Location | Bytes | FNV-1a32 | FNV-1a64 |
| --- | --- | ---: | --- | --- |
| Direct-carom producer | Bank05 `$A8E9-$A9D9` | 241 | `8A09C556` | `A6C3631AD90C94D6` |
| `$BA&3` / `$0588&$80` consumer-side gates | Bank05 `$B6E5-$B73D` | 89 | `787E0E1E` | `03CBE443242CD8DE` |
| Claimant consumer and counter entry caller | Bank05 `$BA56-$BAC0` | 107 | `097B9C78` | `14B4446D08966498` |
| Counter entry | fixed `$C042-$C044` | 3 | `5CC2CBD5` | `26810E19B9D6CDD5` |
| Counter plane | fixed `$CC00-$CC2F` | 48 | `93ACD23F` | `32D90859496DF23F` |

The important ordered path is:

```text
Bank05 $A977:
  $BA & $03 == 0  ->  set $0588 bit $80

Bank05 $BA8C:
  JSR $B87C
  JSR $96B6
  clear $0588 bit $40
  if $0588 bit $80 still set:
    normalize $0588, call $C711, LDX #$08, JMP $C042

fixed $C042 -> $CC12 -> $CC1E -> $CC00
  $CC27[X] for X=8 is $C0
  increment is `INC $7B58,X` after counter-plane indexing
```

The fixed plane proves a byte-counter layout and its wrap behavior. It does
not, by itself, prove which mutable native player/roster should receive a
rebound.

## Why TGRB-1 rejects the current native event

The existing normal-B miss claimant bridge proves a bounded native claimant
settlement. It does not preserve:

1. the original raw `$BA` value at the `$A977` producer and its low-two-bit
   predicate;
2. whether `$0588` bit `$80` survives the intervening original flow to
   `$BA8C`;
3. the contemporaneous `$BE/$BF` owner mapping at fixed `$C042`; or
4. the exact source relation/identity used by the counter plane.

Accordingly, `tecmo_gameplay_rebound_audit_resolve` requires all of these
inputs explicitly, plus a valid claimant relation and nonzero, nonduplicate
claimant serial. The resolver has no ledger pointer by design. Its production
TGLP record reports `raw-ba-unavailable`, even when the bounded claimant bridge
ran, instead of treating "miss + claimant" as a rebound.

## FCEUX validation needed to close the production gap

Use only a private local canonical Rev1 ROM. Do not commit ROMs, logs,
screenshots, save states, or exported trace bytes.

### Memory watch set

Give the watches explicit labels so their role stays clear:

| Address | Suggested label | What must be recorded |
| --- | --- | --- |
| `$00BA` | `carom_selector_raw` | Low two bits at `$A977`, `$B6E5`, and `$BA8C` |
| `$0588` | `carom_ready_flags` | Bit `$80` before/after `$B87C/$96B6`; bit `$40` clear at the consumer |
| `$00BE` | `counter_side_cached` | Value and when it was refreshed |
| `$00BF` | `counter_actor_cached` | Value and when it was refreshed |
| `$009C` | `claimant_slot` | Claimant at `$BA56/$BA8C` |
| `$0308` | `primary_actor` | Before and after claimant settlement |
| `$030A` | `offense_side` | Before and after claimant settlement |

`$0309/$030B`, `$05A9[$BF]`, and the selected counter byte are useful
secondary checks, but they do not replace the required set above.

### Execution breakpoints and ordering log

Set execute breakpoints only while the mapper has the stated bank selected:

| Entry | Capture requirement |
| --- | --- |
| Bank05 `$A977` | Record `$BA`, `$0588`, `$9C`, `$0308`, `$030A` before the `ORA #$80` store. This identifies the producer-side predicate. |
| Bank05 `$B6E5` | Record the independent `$BA&3` gate and any `$0588&$80` branch. |
| Bank05 `$BA8C` | Record the full watch set immediately before `$B87C`, immediately after `$B87C`, and immediately after `$96B6`; this is the identity-freshness proof. |
| fixed `$C042` | Record X, `$BE`, `$BF`, `$05A9[$BF]`, `$CC10[$BE]`, and `$CC27[X]`; for the proposed counter, X must be 8 and `$CC27[8]` must be `$C0`. |

FCEUX expression syntax varies by debugger build, so use its condition-dialog
help rather than copying an unverified syntax string. The intended predicates
are deliberately written as pseudocode:

```text
at $A977 or $B6E5:     ($00BA & $03) == 0
at $BA8C / before C042: ($0588 & $80) != 0
identity check:        cached $BE/$BF still map to the claimant selected at $9C
```

The log must show one true original miss/direct-carom/claimant event from
producer through fixed `$C042`, and it must separately show a same-team
recovery and a non-qualifying route. A general claimant settlement, a made
shot, period expiry, a debug fixture, or a CPU/special route is not enough.

## Verification

Run the focused negative suite against the local ROM:

```powershell
.\tools\Run-GameplayReboundAuditTests.ps1 -Build -RomPath <LOCAL_REV1_ROM.nes>
```

It checks warning-free build, exact importer spans, a copied-ROM mutation in
each of the five source spans (and each error reports its own CPU range),
table-driven parser mutations in every descriptor and raw-span region, the
whole-payload fingerprint, the `system/source-map` TGRB-1 provenance record,
all resolver defer reasons, ledger byte-for-byte non-mutation, bit-8 coverage
absence, the TEAM DATA `---` self-test, and a deterministic native
claimant-bridge JSONL / screenshot whose actual scene ledger still has no REB
coverage or counter value.

The generated pack, JSONL, and PNG are local ignored build artifacts. They are
evidence of the fail-closed native diagnostic only, not proof that a real ROM
rebound was credited.
