# R2 defense/contact raw foundation

This candidate adds a small, pure C99/C11, raw-width foundation for three
bounded Rev. 1 address regions. It is a provenance-anchored native library
surface and focused test runner; it is not normal game integration and does
not claim that semantic defense, contact, or downstream outcome mechanics are
complete.

## Contents

- [SCOPE.md](SCOPE.md) — ownership, non-goals, transactional contract, and
  classification boundary.
- [EVIDENCE.md](EVIDENCE.md) — canonical ROM layout, bank mapping, raw spans,
  hashes, and address/control-flow observations.
- [TEST-MANIFEST.md](TEST-MANIFEST.md) — independent oracle coverage and exact
  focused commands/results.
- [LINEAGE.md](LINEAGE.md) — exact Sol/worker/auditor session lineage and QA
  status.

## Implemented surface

The authorized files are:

- `include/tecmo_gameplay_defense_contact.h`
- `src/tecmo_gameplay_defense_contact.c`
- `tools/Run-GameplayDefenseContactTests.ps1`

The public functions are the required address-anchored names:

- `tecmo_gameplay_defense_contact_b06_weighted_relative_metric`
- `tecmo_gameplay_defense_contact_b06_candidate_scan_b081`
- `tecmo_gameplay_defense_contact_b05_geometry_gate_9968`
- `tecmo_gameplay_defense_contact_b05_state17_plan_9a24`

All fields are raw numeric bytes/words, routine tags, or explicit plan data.
The `$9A24` tail reports the raw state value `$17` as `0x17U`; it records the
external `$C042` request and never invokes or applies that helper.

## Honest boundary

The evidence is limited to raw PC/RAM/address traces and deterministic C
vectors. No scene, video, audio, controller, player-facing, normal-executable,
or runtime-integration proof is produced here. No semantic result enum or
public result label is introduced for steal, block, rebound, recovery, foul,
possession, scoring, matchup, claimant, or contact outcomes.

The later sequential dependency is explicit: any future semantic use of these
raw gates/plans must wait for the separately owned R2 Shots/Outcomes work to be
accepted, then proceed through the appropriate downstream rules/settlement
review. This candidate does not replace or pre-empt that dependency.

## Correction history

- The B05 `$9A24-$9A5F` SHA-256 is the corrected authoritative
  `1751F2A4AAC9A23A385BF172BC419260D7EFB20650B360FDB604DF67A7A5A66B`, with no
  trailing character.
- The Bank05 `$9A38` immediate is raw value `0x17U` (decimal 23), so `$0478`
  and `$0528` plan stores and all expectations use `0x17U`.
- The B081 threshold is built as a `uint16_t` before the strict comparison,
  and B081 performs exactly one descending pass. The separate B104 wrapper is
  documented and byte-checked, but is not modeled as a synthetic second pass.
- The provisional delta-only `$9968` design was superseded by raw
  candidate/reference coordinate pairs. The implementation derives the
  wrapped delta and preserves the native X/depth borrow branch, including the
  paired raw `$FFFF` cases.
- The runner validates the complete legacy iNES image layout: header, optional
  trainer, PRG, and CHR. It does not incorrectly require PRG to cover the CHR
  bytes at the end of the canonical file.

## Luna lineage

See [LINEAGE.md](LINEAGE.md) for the exact session identities. The final
Good-signed branch-only candidate commit, tree, signature, and review
disposition remain pending Sol authorization after this uncommitted correction
pass.
