# R4B lineage, registry, and handoff

## Git identities

| Role | SHA | Tree | Ordered parent(s) | Signature |
| --- | --- | --- | --- | --- |
| Exact base/current main | `819b0e5eabca11683786e45474ca60329dff7f5f` | `a3795c1338dd1f70af631b2827af8c06712ea00c` | `351f446dddc96c34c838c5a9642a0be9d7f1411e` | good SSH signature; verify exit `0` |
| Immutable candidate | `757283edba5f87c2998b16e06bd1831e54ba04b5` | `81adcdec1559b34055406c8be4ea8d646bfb82f1` | `d12073511c80f7eef8b606776415db20b16623a6` | **unsigned**; `%G?=N`; verify exit `1` |
| Signed integration merge | `6b5d43546128408de8ab246d22f1b48322714183` | `4b9879f693c370c1a1dda25212d8222ae6dd5196` | first `819b0e5...`, second `757283e...` | good SSH signature; verify exit `0` |

The merge-base between the base and candidate is
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. Both base and candidate are
ancestors of `6b5d435...`; neither was an ancestor of the other before the
merge. The immutable staging ref resolved exactly to the candidate:

```text
codex/round-4b-frontend-intro-title-staging
757283edba5f87c2998b16e06bd1831e54ba04b5
```

The signed merge was created only on
`codex/r4b-frontend-integration-qa-sol` with:

```powershell
git merge --no-ff -S 757283edba5f87c2998b16e06bd1831e54ba04b5 `
  -m 'Merge accepted R4 frontend into R4B integration QA'
git verify-commit 6b5d43546128408de8ab246d22f1b48322714183
```

The resulting tree exactly matched the pre-mutation `git merge-tree
--write-tree` prediction `4b9879f...`.

## Candidate acceptance and the unsigned fact

The immutable candidate is not cryptographically signed. `git verify-commit
757283e...` returns `1`, and `git log --format=%G?` returns `N`. Master
explicitly authorized continuation with that fact preserved. Candidate
acceptance therefore rests on its exact SHA/tree plus the raw accepted-report
digest, then on this task's signed merge and signed terminal docs commit.

The byte-exact raw blob digest of the candidate's accepted report is:

```text
docs/finish-tasks/R4-frontend-intro-title/recovery-sol-acceptance.md
SHA-256 B59B9C42A9DE8885C02C6DC6BA1545C70B47FDCA7EF2EBA42ECC09D9AE5F4725
```

Base and integration-merge signatures both verify for
`jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

Master reservation/control at assignment was
`f4aaf307961530931b6748823810b89ab502984c`. The master durably recorded the
dynamic QA/proof checkpoint and exact worker registry in signed control commit
`8152f0bb74511c4c7a2a30b0fcc9dc5934002265`; that external control commit is
not an ancestor required by this integration branch.

## Path and collision accounting

Normalized path counts from `6d8f9c7...`:

- current-main/base delta: `75` paths;
- candidate delta: `19` paths;
- exact intersection: `0` paths;
- first-parent merge delta `819b0e5...6b5d435`: the same `19` paths;
- ownership escapes: `0`;
- forbidden binary/proprietary artifact paths: `0`;
- binary `--numstat` rows: `0`.

The candidate's exact 19 paths are:

1. `docs/finish-tasks/R4-frontend-intro-title/finale-fidelity-luna.md`
2. `docs/finish-tasks/R4-frontend-intro-title/native-contract-hardening-luna.md`
3. `docs/finish-tasks/R4-frontend-intro-title/production-proof-replay-luna.md`
4. `docs/finish-tasks/R4-frontend-intro-title/recovery-sol-acceptance.md`
5. `include/tecmo_intro_arena.h`
6. `include/tecmo_intro_arena_scene.h`
7. `include/tecmo_intro_finale.h`
8. `include/tecmo_intro_stage.h`
9. `src/asset_pack/tecmo_asset_pack_arena.c`
10. `src/asset_pack/tecmo_asset_pack_arena.h`
11. `src/asset_pack/tecmo_asset_pack_finale.c`
12. `src/asset_pack/tecmo_asset_pack_finale.h`
13. `src/tecmo_cli_render_scene_modes.c`
14. `src/tecmo_intro_arena.c`
15. `src/tecmo_intro_arena_scene.c`
16. `src/tecmo_intro_finale.c`
17. `src/tecmo_intro_stage.c`
18. `src/tecmo_title_screen.c`
19. `tools/Run-IntroSequenceTests.ps1`

All are within accepted R4 frontend ownership, including the separately
granted proof-only CLI path. Build/CMake registries already contained every
frontend unit exactly once. Personal combined inspection found coherent arena
handoff frame `540`, finale hold frame `1001`, exact CHR identity, TFIN/TASG
validation, render parser, asset-pack registry, and current CPU/LIVE/season/
audio contracts.

## Independent Luna registry

- Task ID: `019fc941-51c3-72a0-b24e-8554691124f1`.
- Exact active/final-review title: `Tecmo R4B Integration QA — Luna Max`.
- Initial create request title:
  `Tecmo R4B Frontend Integration — Independent QA and Proof Audit — Luna Max`;
  the task was later given the concise exact registry title above.
- Model/reasoning: `gpt-5.6-luna` / `max`.
- Host: `local`.
- Created at: `2026-08-03T20:12:09Z`
  (`2026-08-03T15:12:09-05:00`, Unix `1785787929`).
- Completed independent disposition at: `2026-08-03T20:40:54Z`
  (`2026-08-03T15:40:54-05:00`, Unix `1785789654`).
- Initial-turn duration: `1,725,092` ms.
- Allocation: projectless directory
  `C:\Users\joshs\Documents\Codex\2026-08-03\tecmo-r4b-frontend-integration-independent-qa-luna`.
- Git allocation: null. The allocation has neither a `.git` file nor a `.git`
  directory; branch, worktree, and writable Git scope are null.
- Read-only reference: signed merge `6b5d435...`.
- Creation lineage: one successful `create_thread` call; no creation/setup
  fault, retry, replacement, or predecessor.
- Pin lineage: initial pin call succeeded immediately. The worker remains
  pinned at this documentation snapshot so its accepted result is first
  recorded in the signed terminal docs commit.
- Contact boundary: worker reported only to this R4B Sol; it did not contact
  master or any other task and created no task/subagent.

The worker independently returned **ACCEPT**, P0 `0`, P1 `0`, P2 `0`, no
product rescope. Its proof review is recorded in `INDEPENDENT-QA.md`. After the
signed terminal docs commit is verified, the Sol must unpin this worker and
record the exact unpin response/time in the terminal handoff. The current Sol
task itself stays pinned because retirement is master-owned.

## Main observations and master-only handoff

At assignment, after the signed merge, after proof, and immediately before
documentation, all three main observations were unchanged:

```text
local main   819b0e5eabca11683786e45474ca60329dff7f5f
origin/main  819b0e5eabca11683786e45474ca60329dff7f5f
live main    819b0e5eabca11683786e45474ca60329dff7f5f
```

This task stops before main/push. After the Sol terminal handoff supplies
`<SIGNED_TERMINAL_REPORT_SHA>`, the master alone may run this guarded,
non-force fast-forward sequence from the main worktree:

```powershell
git fetch origin
if ((git rev-parse main).Trim() -ne `
    '819b0e5eabca11683786e45474ca60329dff7f5f') { throw 'local main moved' }
if ((git rev-parse origin/main).Trim() -ne `
    '819b0e5eabca11683786e45474ca60329dff7f5f') { throw 'origin/main moved' }
$live = ((git ls-remote origin refs/heads/main) -split '\s+')[0]
if ($live -ne '819b0e5eabca11683786e45474ca60329dff7f5f') {
    throw "live main moved to $live"
}
git switch main
git merge --ff-only <SIGNED_TERMINAL_REPORT_SHA>
git push origin main
```

No force option is permitted. If any guard differs, stop and reconcile the
exact movement through the master rather than using these instructions.
