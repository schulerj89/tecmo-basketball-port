# Scope and contract

## In scope

This task owns only a pure, bounded, raw/neutral native foundation for the
following Rev. 1 regions:

| API | Raw region | Contract |
| --- | --- | --- |
| `tecmo_gameplay_defense_contact_b06_weighted_relative_metric` | Bank06 `$B081` metric path | Wrapped 16-bit X absolute, absolute 8-bit depth, and `max + floor(min/2)` metric with 16-bit result wrap. |
| `tecmo_gameplay_defense_contact_b06_candidate_scan_b081` | Bank06 `$B081-$B103` | One descending candidate pass, self skip, `$04B0` bit-$10 gate, stale threshold low-byte behavior, strict improvement, and `$037F[$030B]` mirroring. |
| `tecmo_gameplay_defense_contact_b05_geometry_gate_9968` | Bank05 `$9968-$999D` | Raw candidate/reference X and depth coordinate subtraction, explicit borrow branches, and the exact bounded raw gate. |
| `tecmo_gameplay_defense_contact_b05_state17_plan_9a24` | Bank05 `$9A24-$9A5F` | Transactional report of the local raw write plan and external `$C042`, X=`$07` request. |

The header is self-contained and C-compatible. The C source has no ROM, ASM,
file, asset, scene, audio, controller, or runtime dependency.

## Raw-width rules

- B06 X arithmetic is an explicitly wrapped 16-bit subtraction. Depth is an
  explicitly wrapped/absolute 8-bit difference. The metric sum is narrowed to
  `uint16_t` deliberately.
- `$9968` receives coordinate pairs, not a lossy signed byte or a delta-only
  surrogate. X computes candidate minus reference and records whether the
  16-bit subtraction borrowed. Depth does the same at 8 bits.
- The no-borrow X branch accepts delta `0x0000..0x0007`; the borrow branch
  accepts `0xFFF8..0xFFFF`. The no-borrow depth branch accepts `0x00..0x05`;
  the borrow branch accepts `0xFA..0xFF`.
- The `$9A24` counter increments with explicit 8-bit wrap. The stored state
  byte is raw `$17` / `0x17U`, not decimal 17.
- The `$0754` view uses its own address-specific
  `TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT` constant (`10U`);
  it is not coupled to the Bank06 candidate-count definition.

## Transactional and fail-closed behavior

Every public operation validates all required pointers, exact routine tags,
CPU addresses, bounded array lengths, indices, and raw context preconditions
before reading a caller-owned mutable output. A local result is fully formed
and assigned only after all validation and dependency reads succeed. NULL,
bad tags/addresses, bad lengths/indices, missing route context, or illegal
aliasing returns `false` and leaves every caller-visible output byte unchanged.

Read-only B06 table views may alias one another. The read-only `$0754` view may
be shared only as read-only input. Otherwise, a result may not overlap its
input record or any caller-provided table view; such overlap is rejected before
mutation. The module does not permit a caller-visible mutable region to serve
as both input and output.

The `$9A24` helper request is data in a plan result. It is never called. The
plan never mutates scene, audio, possession, foul, stat, controller, or
scoring state.

## Explicit non-goals and exclusions

This module does not implement or name semantic steal, block, rebound,
recovery, contact, foul, defender, matchup, claimant, possession, scoring,
statistics, or downstream outcome behavior. It does not infer human/CPU
meaning, orientation semantics, or a player-facing action from the raw bytes.
The B104-$B108 wrapper is an evidence-only control-flow boundary; B081 does
not inspect a wrapper predicate and never performs two passes in one call.

No existing scene, CPU/TGAI, TPNL, TGSR, pre-tip, asset, import, source-map,
registry, CMake, build, normal executable, game/flow/Win32, main, or other
module file is edited. The runner hashes selected CPU/LIVE/TIP boundaries and
checks stable public boundary statements read-only.

## Evidence classification

| Item | Classification | Meaning |
| --- | --- | --- |
| Canonical ROM identity, iNES layout, direct raw-span fingerprints | exact | Independently checked test-only provenance. |
| B06 arithmetic, scan order, gate, strict threshold, and stale preservation | native-faithful | Bounded raw behavior mapped from the cited bytes, with explicit C-width wrapping. |
| `$9968` coordinate/borrow gate | native-faithful | The native carry/borrow branch is represented directly rather than approximated by a signed byte. |
| `$9A24` local stores and request record | native-faithful | Local plan bytes are represented; the external helper effect remains outside this contract. |
| Native-approximate semantic defense/contact behavior | none in this module | No semantic approximation is introduced or hidden behind these APIs. |
| Runtime integration and player-facing proof | incomplete | Excluded by the signed assignment boundary and must be reviewed later. |
