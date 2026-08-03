# Merge handoff

This lane is a domain branch for the authoritative Sol task. Merge only the
intentional domain commits into the Sol worktree by fast-forward-only means:

1. implementation/tests commit
   `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`;
2. first documentation commit
   `540ae0ba47ef44d6096781ffd0c276012e683221`; and
3. documentation metadata correction
   `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

Sol has completed the intended fast-forward-only integration into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`. The three commits report Good
Git signatures; see `LINEAGE.md` for the signer and key fingerprint.

Do not merge, rebase, or push this branch to `main`, `origin-main`,
`staging`, or `master`. The parent Sol worktree is the only intended merge
destination.

Before handoff, verify that the branch contains only the authorized paths,
that the implementation commit remains the expected parent lineage, that the
docs are sanitized, and that `git diff --check` is clean. Sol should review
the exact scene rescope in `APPROXIMATIONS.md` before any future live
substitution or active-lineup integration.

Sol's personal QA and bounded production proof are recorded in `PROOF.md` and
`TESTS.md` as v1 proof-source evidence bound to HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`. This remediation must be
fast-forwarded into Sol only after the signed commit is created; use the
precise equivalent of `git merge --ff-only <remediation-commit>` in the Sol
worktree. Independent QA task
`019fc957-a425-70f3-83b9-1e63dfdba40e` remains pinned for re-audit, and v2 Sol
QA/proof plus the terminal accepted SHA remain pending. Do not regenerate or
claim v2 proof from the v1 manifest. Never merge this domain into `main`,
`origin-main`, `staging`, or `master`.
