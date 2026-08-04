# Diagnostic and revision ledger

No listed item produced data loss, unauthorized mutation, task replacement,
duplicate terminal QA, main/staging action, or an unresolved product finding.
All generated output remained ignored.

## Research and orchestration diagnostics

1. Initial auditor registry plumbing had one wrapper fault, durably recorded in
   signed control `ccea216b`; the three intended auditors were retained and no
   replacement lineage was created.
2. One master transmission payload had a local JavaScript `SyntaxError:
   Unexpected identifier control` before the send tool was invoked because
   inline delimiters broke quoting. A quoting-safe resend succeeded. No app,
   repository, thread, or task mutation occurred.
3. Sol's first PowerShell tree query left `HEAD^{tree}` unquoted and PowerShell
   parsed it incorrectly. The quoting-safe query returned the expected tree;
   no mutation occurred.
4. Sol first requested 100 recent tasks from a tool whose maximum is 50. The
   rejected read-only collision query was rerun at 50 and found no dedicated
   terminal-QA duplicate.
5. A read-only auditor-thread query requested an output cap above the tool's
   20,000-character maximum. The request was rejected, then rerun at the valid
   cap; no task or repository state changed.
6. One aggregate auditor read and one large proof-manifest read hit output
   truncation. Narrower reads supplied the needed evidence; files were not
   changed.
7. A Windows `rg` query used shell-style filename globs as literal path
   arguments and emitted an invalid-filename diagnostic. The corrected
   `--glob` query succeeded; no mutation occurred.
8. One personal proof-hash command assumed nonexistent names
   `native-video-pass1.mp4`/`native-video-pass2.mp4`. The actual ignored files
   `native-repeat-1.mp4`/`native-repeat-2.mp4` were located and hashed
   successfully.
9. Before signing task docs, Sol found that root `AGENTS.md`, root `PORTING.md`,
   and the global gameplay-state narrative still described the pre-change
   immediate cue. Because the root files are active contracts, Sol stopped,
   requested exact rescope, and changed only the three passages after
   Good-signed control `6028f997`; no product change or acceptance rollback was
   required.
10. The first composite documentation preflight returned no captured detail;
    its follow-up also showed why porcelain alone was insufficient by collapsing
    the untracked task-doc directory to one entry. The final read-only ledger
    combined `git diff --name-only` with `git ls-files --others
    --exclude-standard` and proved the exact 10/10 documentation paths with zero
    delta and zero trailing-whitespace matches.

## Worker review/revision lineage

Sol review caught and the same worker corrected these draft issues before the
signed candidate:

- conflation of SFX ID 6 with `TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID`;
- use of a nonexistent `backcourt_detected` test field;
- inverted music-playing polarity;
- a weak camera/projection assertion;
- a test expecting a restart event at violation entry instead of handoff;
- a disabled-music test requiring the already-consumed SFX 6 playback to be
  cleared;
- a post-frame-16 loop that overran the release lead-in by 16 frames;
- a transient dispatch experiment that would have suppressed accepted
  shot-clock expiry SFX 3.

Worker execution diagnostics:

- one rebuild command lacked the Visual Studio developer environment and failed
  to locate `stdbool.h`; the environment-corrected rebuild passed;
- one gameplay-audio runner invocation used the wrong positional pack contract;
  the corrected documented invocation passed.

## Inherited test/build collisions resolved under signed rescope

Three existing state-flow blocks still expected immediate SFX 6: TGMO OOB entry,
shot-clock violation, and music-disabled violation. Master granted exact
line-bounded test corrections in `c1eecd14` and `bb461df`; all now prove the
source-backed delayed sequence.

Before `build.ps1` registration was granted, its static source list omitted the
new test translation unit and produced LNK2019/LNK1120. Signed control
`bb461df` authorized exactly one additive entry; canonical console/GUI builds
then passed warning-clean.

## Sol personal QA diagnostics

The first fresh CMake command used bare `cmake`, which is absent from PATH. In
PowerShell that command-not-found path also left a misleading shell exit state.
No build root was created. Sol reran with Visual Studio's bundled CMake; fresh
console and GUI builds passed warning-clean.

## Independent terminal-QA diagnostics

- The first repository snapshot process was rejected before execution with
  `directory name is invalid`; rerun in the exact frozen worktree succeeded.
- The first combined AGENTS/PORTING read hit the tool output cap. Sequential
  chunk reads then covered all 1,456 AGENTS lines and all 1,685 PORTING lines.
- Bare `cmake` was not on PATH. QA independently found Visual Studio's bundled
  CMake and both fresh builds passed.
- `Run-GameplaySceneTests.ps1 -RequirePass` was intentionally not used because
  that optional identity gate is hard-coded to the unrelated R1 live-foundation
  branch/base. The complete suite still passed with `suites_complete=True`; this
  is a tooling boundary, not a scene failure.

The research-only proof-gap auditor briefly began a provisional support QA pass
before the master clarified lineage. Sol stopped it, and its partial report
explicitly made no terminal-acceptance claim. It is not counted as terminal QA.
The sole terminal-QA task was created once with zero creation/pin/retry/
replacement faults and returned `ACCEPT` with no P0-P3 findings.
