# Implementation record

Implementation commit: `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`
(`Implement R2 clock fatigue and free-throw lineups`).

## Clock and period state

`src/tecmo_gameplay_state.c` adds a portable conservative typed-range helper
and uses it at `tecmo_gameplay_update` only for unsafe overlaps involving the
writable state or event buffer. Alias failure leaves state, const inputs,
context, and events untouched. The ordinary null contract remains: a null
state clears a non-null event buffer and returns false; a null event buffer
returns false.

`tecmo_gameplay_reset_possession` now validates the state phase as `LIVE`
before changing anything. Existing violation handling still transitions to
LIVE before reset. `gameplay_events_equal` compares count and event
kind/value/detail fields rather than struct padding.

The state self-test now contains exact late-clock, shot-expiry (including
exemption detail), simultaneous-expiry, fixed-wait, final-music, and complete
vectors; both duration matrices; saturated phase-frame checks; symmetric
controller dismissal paths; and the non-LIVE reset matrix.

The independent-QA remediation stages `state_init` behind config-overlap and
validation checks, stages state/event outputs for request/settlement/free-throw
and close-shot mutators, and adds aligned exact/partial alias, invalid,
capacity, unchanged-sentinel, and successful event-vector self-tests. Scalar
score, reset, violation, and foul mutators now validate complete local next
states before one commit; successful replay/event semantics remain unchanged.

## TGFT fatigue

`src/tecmo_gameplay_fatigue.c` stages parse results in `parse_into`, validates
the complete in-memory object before commit, and frees the previous storage
only after a successful replacement. `assets_valid` checks lifecycle,
availability, storage/object non-overlap, exact size/hash, header/scalars,
dependency fingerprint, every canonical descriptor, and every canonical
descriptor bytes pointer.

`tecmo_gameplay_fatigue_state_initialize` and
`tecmo_gameplay_fatigue_step` construct local output values and reject output
overlap with the assets object, canonical storage, seeds, or input before
mutation. A null assets step is rejected before dereferencing assets.
`state_valid` rejects only the unreachable public cadence value `6` (and any
larger value); it intentionally preserves countdown byte-wrap behavior,
including `0 -> 255`.

`src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c` stages the 512-byte
payload and provenance, verifies the full Rev1 fingerprint gate, rejects all
three input/output byte-range pair overlaps, and commits both outputs once.

## TGFL free-throw lineup

`src/tecmo_gameplay_free_throw_lineup.c` adds a strict internal
`assets_valid` validator for all lifecycle/storage/hash/dependency/pointer and
four source-descriptor invariants. `find_source`, the pure base resolver, and
the caller-policy resolver all fail closed on invalid objects. Parse/load use a
staged replacement so a valid preloaded object survives failed replacement;
the old storage alias is safe because the new storage is allocated and copied
before commit. The post-commit old-storage free uses the same bounded
canonical, non-overlapping range check as destruction, so a lifecycle-valid
but corrupt in-object/zero-size/noncanonical old storage is skipped safely.

`tecmo_gameplay_free_throw_lineup_derive` remains the source-compatible pure
base resolver. New script fields are explicitly undefined there. The separate
`tecmo_gameplay_free_throw_lineup_derive_caller_policy` composes only the
proven conditional tail and accepts generic zero/nonzero predicate bytes.
Output overlap with either the asset object or canonical storage is rejected.

Opcode validation requires two contiguous canonical predicate/effect tails
within Bank06 `$976F-$985C`: one shooter tail with the predicate branch and
three proven shooter stores, and one secondary tail with its predicate branch
and proven raw-phase store. The proprietary payload bytes are intentionally
not reproduced in committed documentation.

The builder stages the unchanged 1216-byte TGFL payload and provenance,
requires the full Rev1 SHA-256 internally, and rejects all pairwise input/output
overlaps before any output write.

Both TGFT and TGFL destructors now skip freeing non-NULL storage when its size
is zero, noncanonical, overflowing, or overlaps the containing asset object;
they then safely reinitialize the object. Focused self-tests cover zero-size
and overlapping in-object corrupt storage, double destroy, outside-object
sentinels, and preservation of the live canonical allocation. Portable C still
cannot detect every arbitrary invalid pointer, so no such guarantee is claimed.

## Tests and boundary

The TGFL self-test covers every base placement, all four 0/1 control
combinations, the explicit `0xFF` nonzero case, structured corruption,
preloaded failed reload, and object/storage overlap sentinels. No production
scene caller was added. See `SCOPE.md` and `APPROXIMATIONS.md` for the exact
later integration boundary.
