# R0A Current-State Inventory and Adoption

## Scope and non-goals

This is the master-owned, coordination-only inventory taken after the Round 0
control plane was pushed. It records repository state, current and reported
Codex sessions, branch/worktree lineage, ignored proof sets, accepted historical
commits, source-declared fidelity gaps, and the dependency-aware backlog.

The master did not change product code, run product tests, launch the game, or
inspect product frames/audio for this task. Product assertions below are the
documented sign-off of the named Sol Max orchestrators or explicit statements in
the tracked source and documentation.

## Snapshot

- Program base before orchestration: `63b29b04b1ab4745b7b8d5dd0499942d1bf8ba4e`
- Round 0 control-plane/main SHA: `7090d2c62201ce3d330df20043a6e80fd0bdef00`
- Remote: `git@github.com:schulerj89/tecmo-basketball-port.git`
- Inventory observation window: 2026-08-03 UTC
- Primary checkout remains clean on `codex/away-left-animation-facing` at
  `63b29b04`; the persistent master worktree is clean on
  `codex/master-finish-orchestration` at `7090d2c6`.

## Sources inspected

- `AGENTS.md` and `PORTING.md`, read completely before Round 0 changes
- `README.md` and `port_iteration.json`
- Source/header status strings and source-map approximation contracts
- All top-level `tools/Run-*Tests.ps1` runner names and proof tooling names
- Git refs, logs, worktrees, status, ancestry, and branch-only commits
- Recent Tecmo thread inventory plus the complete Luna lineage reported by the
  three adopted Sol orchestrators
- Ignored `build/proof`, `build/test-logs`, `build/visual-qa`, and `temp-videos`
  sets in every registered worktree

## Adopted Sol work

| Sol thread | Accepted scope | Accepted branch/head | Main result |
|---|---|---|---|
| `019fc012-6da0-7410-8600-a6b89b402424` | CPU movement recovery and visible-window tip input | `codex/cpu-tipoff-behavior` / `11ed80cd` | Included through `1caa6453` and later main |
| `019fc081-44bd-7422-baac-3030f5356dba` | Tip jump, edge, HUD, facing, fast input, CPU sample, proof | staged through `1caa6453`, `f6e1f3a8`, `8ebf564d` | Merge `9979b136` |
| `019fc4e9-55c8-7d62-904f-30d0974d9c6f` | Goal-relative Away-left render polarity | `codex/away-facing-left-only` / `f26da588` | Final `63b29b04` |

Each has a separate adoption record in this directory's sibling task folders.
They are recorded as pushed history, not as proof that the broader acceptance
criteria are complete. In particular, original CPU play selection and exact
tip winner/trajectory policy remain unfinished.

## Worktree and branch disposition

Twenty-four worktrees and twenty-seven local branches were observed. No cleanup
was performed. All worktrees were clean except:

`C:/Users/joshs/Projects/tecmo-basketball-port-tip-input-e2e-worker`

Its five tracked modifications are preserved untouched:

- `AGENTS.md`
- `PORTING.md`
- `README.md`
- `src/tecmo_gameplay_scene_test_pretip.c`
- `src/tecmo_win32_keys.c`

Branch ancestry and patch-equivalence show that the remaining worker commits are
either ancestors of main, patch-integrated into main, safety snapshots, or
superseded experiments. `codex/tipoff-jump-visual` contains branch-only commit
`0be975c3`, but the same implementation subject was integrated as `c9c29041` and
then hardened. It must not be merged independently.

## Proof inventory

The inventory indexes each proof directory as a set with recursive file/byte
counts, image/video/audio counts, and every manifest/summary found. Important
accepted sets include:

- `build/proof/tipoff-main-9979b13`: 278 files, 136 PNGs, one video, manifest
- `build/proof/away-facing-left-only`: 145 files, 45 PNGs, three videos, manifest
- `build/proof/away-facing-left-luna`: 296 files, 216 PNGs, two videos, manifest
- `build/visual-qa` in the CPU worktree: ten files, six images
- `build/test-logs/post-merge-1caa64531ec5`: 30 files and 27-suite summary

No audio capture, waveform, or listening-proof set was found. That absence is a
Round 4 audio acceptance requirement, not an external blocker.

## Source-declared completion gaps

The tracked product itself still declares these incomplete or approximate:

- Original CPU play-command selection, dynamic links, spacing, marking policy,
  starting-lineup binding, and shot timing
- Remaining jump-shot families/outcomes, contact, steals, blocks, rebounds,
  fouls, and free-throw outcome/rebound policy
- Exact tip winner/claim settlement and original jump trajectory/timing
- Per-player game/season statistics, populated League Leaders, and All-Star game
  launch
- End-to-end season completion/save/stat integration acceptance
- Original-reference frontend/menu timing fidelity and complete audio proof
- Clean rebuild, legal/provenance audit, full game/season, and end-to-end
  visual/audio release sign-off on one final staging SHA

The machine-readable acceptance matrix and R1-R5 task queue map every item to an
owner-ready backlog task. All remain `incomplete` until a Sol signs evidence,
tests, and production-path proof.

## Coordination validation

The master may run only the control-plane schema, state, ownership, duplicate,
and Git-lineage checks for this task. Product QA remains delegated to future Sol
domain and integration orchestrators.

All state/schema JSON parsing, validator syntax compilation, schema/semantic/Git
lineage validation, synthetic failure-detection self-tests, and the generated
dashboard freshness gate passed. Inventory implementation commit:
`6bbbdc3a2726fd38d8dbe75ac5581bf3465d922b`.

## Merge instructions

After the ready-for-main metadata commit is created and validation is repeated,
fast-forward local `main` from `codex/master-finish-orchestration` only if
`origin/main` remains exactly
`7090d2c62201ce3d330df20043a6e80fd0bdef00`. Push `main` once, without force.
Record the resulting main/push SHA in the next committed control-plane update.
