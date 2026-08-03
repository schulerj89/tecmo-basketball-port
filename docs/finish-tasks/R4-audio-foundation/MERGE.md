# Merge and handoff

Status: Sol acceptance pending. Do not merge until the Sol task accepts this
Luna revision.

## Ordered commits

1. `29611607babe31415ab063520d832631ab3c2e4c` — Implement R4 audio
   foundation transactions and proof gates.
2. `51790b832eb4bb23db07ac7965d6c2b1da877a1e` — Documentation contract.

The first commit is based directly on
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. The worker did not push, merge,
rebase, or alter shared build/platform boundaries.

## Sol fast-forward-only merge instructions

After Sol acceptance, update the Sol domain branch to the expected parent and
fast-forward it to the accepted Luna branch. The required operation is:

```powershell
git switch <Sol-domain-branch>
git merge --ff-only codex/r4-audio-foundation-luna
```

If `--ff-only` refuses, stop and report the divergence; do not create a
merge-commit, rebase, or force the branch. Confirm that the resulting diff
touches only the delegated audio paths and
`docs/finish-tasks/R4-audio-foundation/**`.

Rerun the three owned PowerShell suites with the canonical local-private Rev1
ROM and `TECMO_SKIP_SHORTCUT=1`; keep generated proof output under ignored
`build/proof/` and do not add it to the index.

After merge, review the explicit non-goals: cross-domain cue routing and
cycle-exact NES APU/DMC claims remain deferred. The later independent QA Luna
and Sol listening slots in `PROOF.md` remain open until those observations are
supplied.
