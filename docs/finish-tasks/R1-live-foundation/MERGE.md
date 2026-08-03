# Review and merge handoff

## Current state

This handoff contains the first reviewed implementation/docs commit created
from expected parent `ad0f005673692b04772bce3c3b4d3ac4b2624731`. No history
was rewritten and no branch was pushed.

The latest reproducible draft proof root is
`build/live-proof-edge-review-20260803-c`. Its manifest is `DRAFT` with
`final_sha=PENDING_CLEAN_COMMIT`; it validated the accepted CPU reference
manifest, native videos, dynamic contact-sheet dimensions, and every artifact
inventory entry. It must not be relabeled PASS until the reviewed clean commit
is rerun with `-RequirePass`.

## Required post-commit sequence

1. At this handoff, create the first reviewed implementation/docs commit from
   the exact bounded tree. Its SHA is supplied by the worker handoff; the
   draft proof's `final_sha` remains pending and is not circularly recorded in
   this first commit.
2. From that clean commit, rerun the warning-clean console/Win32 build,
   CPU/movement/LIVE wrappers, production flow test, Win32 launch smoke, proof
   capture, visual review, and independent QA. For the formal proof use
   `-OriginalReferenceManifestPath C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna\temp-videos\gameplay-lab\cpu-lifecycle\20260803-053244\proof-manifest.json -RequirePass`.
3. Create a docs-only closure commit recording the formal proof result,
   final SHA, lineage, and independent QA. Do not rewrite the first commit.
4. QA checks the exact two-commit chain, proof continuity, immutable
   TGAI/TGMO fingerprints, and the bounded path list.
5. In the Sol worktree, merge only with:

```text
git merge --ff-only codex/r1-live-foundation-luna
```

Do not cherry-pick from an unreviewed revision, force-push, reset, clean
unrelated worktrees, merge main, or alter the excluded TIP/render/accepted CPU
paths.
