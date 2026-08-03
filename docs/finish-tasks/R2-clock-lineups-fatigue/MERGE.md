# Merge handoff

This lane is a domain branch for the authoritative Sol task. Merge only the
intentional domain commits into the Sol worktree by fast-forward-only means:

1. implementation/tests commit
   `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`;
2. the docs-only commit or commits recorded in `LINEAGE.md` after they are
   created; and
3. Sol-owned terminal QA and production proof, which are not part of this
   worker's commit.

Do not merge, rebase, or push this branch to `main`, `origin-main`,
`staging`, or `master`. The parent Sol worktree is the only intended merge
destination.

Before handoff, verify that the branch contains only the authorized paths,
that the implementation commit remains the expected parent lineage, that the
docs are sanitized, and that `git diff --check` is clean. Sol should review
the exact scene rescope in `APPROXIMATIONS.md` before any future live
substitution or active-lineup integration.

The final merge/QA identity is deliberately left for Sol to record in
`PROOF.md` and `LINEAGE.md`; this worker does not claim production integration
or visual/audio proof.
