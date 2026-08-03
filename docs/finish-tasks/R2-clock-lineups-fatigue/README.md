# R2 clock, lineups, and fatigue

This finish-task record covers the native, source-grounded R2 implementation
for clock/period state, fatigue evolution, and TGFL free-throw lineup
resolution. The implementation commit is
`6c87dbed170c8ca2ba68e29671f7cfebf5adb60a` on
`codex/r2-clock-lineups-fatigue-luna`.

Sol fast-forwarded the original signed worker lineage through
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8` and personally accepted the
Good-signed remediation commit
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` by fast-forward-only integration.
The v1 and v2 personal QA/proof records are in `PROOF.md` and `TESTS.md`.
Independent QA froze candidate `1536ae31e7016f6e9adbddb7868e2d40e51c1085`
with a historical P2 FAIL; its re-audit remains pending. The forthcoming
independent-re-audit candidate will be a docs-only descendant of the v2
proof-source HEAD, which does not invalidate artifacts bound to that HEAD.

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
The v1 and v2 Sol proofs record bounded shot-clock/violation and free-throw
orientation renders only; neither claims period, halftime, or final render
ownership. Terminal acceptance remains pending.
