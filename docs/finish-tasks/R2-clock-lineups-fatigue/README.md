# R2 clock, lineups, and fatigue

This finish-task record covers the native, source-grounded R2 implementation
for clock/period state, fatigue evolution, and TGFL free-throw lineup
resolution. The implementation commit is
`6c87dbed170c8ca2ba68e29671f7cfebf5adb60a` on
`codex/r2-clock-lineups-fatigue-luna`.

Sol fast-forwarded the signed worker lineage through
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`. Sol's personal QA and the
task-specific deterministic production proof are recorded in `PROOF.md` and
`TESTS.md` as v1 proof-source evidence. Independent QA froze candidate
`1536ae31e7016f6e9adbddb7868e2d40e51c1085` with a historical P2 FAIL; this
remediation is not v2 proof, and Sol v2 QA/proof plus independent re-audit
remain pending.

The worker lane is complete and remains bounded to the owned modules. The
production gameplay scene still supplies its own actor slots, active flags,
roster indexes, starter arrays, and validation. Live substitution integration
is deliberately deferred; see [APPROXIMATIONS.md](APPROXIMATIONS.md) and
[MERGE.md](MERGE.md).

Committed product changes are ROM/decomp independent at runtime. The local
Rev1 ROM is used only by focused source/build tests. No ROM, lifted ASM, raw
capture, save state, or proprietary payload is committed.

## Contents

- [SCOPE.md](SCOPE.md) — owned boundary and explicit deferrals.
- [EVIDENCE.md](EVIDENCE.md) — sanitized evidence and behavior matrix.
- [IMPLEMENTATION.md](IMPLEMENTATION.md) — changed functions and contracts.
- [LINEAGE.md](LINEAGE.md) — research and revision lineage.
- [TESTS.md](TESTS.md) — commands and observed results.
- [PROOF.md](PROOF.md) — Sol QA and deterministic production proof.
- [OBSERVATIONS.md](OBSERVATIONS.md) — bounded observations.
- [APPROXIMATIONS.md](APPROXIMATIONS.md) — exclusions and later rescope.
- [MERGE.md](MERGE.md) — fast-forward-only handoff to Sol.

Audio is N/A for the owned semantics. The clock tests assert event vectors and
state transitions; they do not claim period/halftime/final visual semantics.
The v1 Sol proof records bounded shot-clock/violation and free-throw
orientation renders only; no v2 proof was regenerated for this remediation.
