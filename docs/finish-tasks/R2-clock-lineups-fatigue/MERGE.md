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
5. v2 QA/proof docs-only revision
   `1567f284ff48a2334fb6a9bd82d00aadf0cdb373`.

Sol has completed the intended fast-forward-only integration into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8` and accepted remediation commit
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` by ff-only integration. All six
audited commits report Good Git signatures; see `LINEAGE.md` for the signer
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
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`; the v2 proof remains bound to
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`. Those commits are already
integrated into Sol. The single intended next action is to integrate this
new Good-signed closure docs commit into the Sol worktree with:

`git merge --ff-only <closure-docs-commit>`

Use the full SHA reported by the closure commit's external git-object/handoff;
the docs do not make an impossible self-referential SHA claim. The
independent re-audit passed; QA remains pinned only until Sol performs and
durably captures the closure-doc consistency recheck. Never merge this domain into `main`,
`origin-main`, `staging`, or `master`.
