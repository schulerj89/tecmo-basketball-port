# Merge and handoff

## Ordered commits

1. `29611607babe31415ab063520d832631ab3c2e4c` — Implement R4 audio
   foundation transactions and proof gates.
2. Documentation contract commit — the commit that adds this documentation
   subtree; its exact ID is supplied in the final handoff because a commit
   cannot contain its own hash.

The first commit is based directly on
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. The worker did not push, merge,
rebase, or alter shared build/platform boundaries.

## Sol merge instructions

From the authoritative Sol task, verify the expected parent and cherry-pick the
two commits in the order above. Confirm that the resulting diff touches only
the delegated audio paths and `docs/finish-tasks/R4-audio-foundation/**`.
Rerun the three owned PowerShell suites with the canonical local-private Rev1
ROM and `TECMO_SKIP_SHORTCUT=1`; keep generated proof output under ignored
`build/proof/` and do not add it to the index.

After merge, review the explicit non-goals: cross-domain cue routing and
cycle-exact NES APU/DMC claims remain deferred. The later independent QA Luna
and Sol listening slots in `LINEAGE.md` remain open until those observations
are supplied.
