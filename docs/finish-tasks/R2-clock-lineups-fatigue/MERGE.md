# Merge handoff

This lane is a domain branch for the authoritative Sol task. Merge only the
intentional domain commits into the Sol worktree by fast-forward-only means:

1. implementation/tests commit
   `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`;
2. first documentation commit
   `540ae0ba47ef44d6096781ffd0c276012e683221`; and
3. documentation metadata correction
   `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`;
4. signed independent-QA remediation
   `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`; and
5. this forthcoming Good-signed v2 QA/proof docs-only revision, using the
   full SHA printed by its final handoff as `<v2-docs-commit>`.

Sol has completed the intended fast-forward-only integration into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8` and accepted remediation commit
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` by ff-only integration. The four
existing commits report Good Git signatures; see `LINEAGE.md` for the signer
and key fingerprint.

Do not merge, rebase, or push this branch to `main`, `origin-main`,
`staging`, or `master`. The parent Sol worktree is the only intended merge
destination.

Before handoff, verify that the branch contains only the authorized paths,
that the implementation commit remains the expected parent lineage, that the
docs are sanitized, and that `git diff --check` is clean. Sol should review
the exact scene rescope in `APPROXIMATIONS.md` before any future live
substitution or active-lineup integration.

Sol's v1 proof remains bound to HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`; the v2 proof is bound to
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`. To integrate the current lane
after this docs-only commit is signed, use the precise equivalent of:

`git merge --ff-only bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`

followed by:

`git merge --ff-only <v2-docs-commit>`

in the Sol worktree. The forthcoming independent re-audit candidate is a
docs-only descendant of the v2 proof-source HEAD and does not invalidate its
artifact binding. Independent QA task
`019fc957-a425-70f3-83b9-1e63dfdba40e` remains pinned for re-audit, and the
terminal accepted SHA remains pending. Never merge this domain into `main`,
`origin-main`, `staging`, or `master`.
