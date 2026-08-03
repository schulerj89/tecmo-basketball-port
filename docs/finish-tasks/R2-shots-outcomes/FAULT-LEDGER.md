# R2 Shots Outcomes fault ledger

This ledger records ordinary failed commands and review-found defects in the
same uncommitted Luna lineage. It is diagnostic history, not a claim that the
failures remain.

| Checkpoint | Observed failure | Cause / disposition |
| --- | --- | --- |
| Initial terminal-matrix run | Existing proof-root collision | Re-ran with a fresh ignored proof root. No source change. |
| Terminal matrix stage 310 | Exact/close search miss | Expanded bounded exact vectors and kept search misses nonfatal while preserving hard selector/ownership failures. |
| Close matrix stage 300/detail `256000000` | Prepared fixture was started twice | Removed duplicate `scene_start_shot_actor`; successful preparation is the production start. |
| Close matrix vertical cases | Pure-down fixture did not prepare | Changed vertical vectors to `dx=+/-2, dy=+/-16`, retaining the numeric-2 vertical substitution only where documented. |
| Terminal stage 3110/detail `631017` | Terminal finish returned false after 131 successful dunk updates | Diagnostic reason was subdivided into movement/idle pose, award, audio, and handoff. Later fixture investigation showed the green terminal matrix needed a resolver-valid future holder; temporary production reason API was then removed. |
| Terminal stage 3110/detail `631022` | `scene_handoff_possession` failed for a made close terminal | Matrix had moved future holders to generic far coordinates without rebuilding holder movement state. Production dribble/ball code was left untouched; the test now preflights the resolver and rebinds only the future holder. |
| Terminal holder detail `430` | Dribble resolver rejected a made recipient at far Y=20 | Made handoffs use the source-tested safe canonical `{352,198}` holder position; miss claimants retain the actual endpoint. All eight movement directions are tried, the first resolver/pose-valid candidate is committed, and linked actor state is never rebuilt. |
| Exact make entry test stage 9 | Test expected a first update to remain at frame 1 | Jump starts at frame 1; the corruption probe now runs on that initial gather frame, then normal held updates continue to frame 8. |
| Validator tail review | General frame/duration sums could mask coordinated route edits | Validator now binds selector/kind to exact 6/8/7/17 durations and binds bound jump tail base to `$87`; tests mutate duration+total and base+frame+total transactionally. |
| Validator raw-selector review | Normal jump diagnostic field was not independently zero-bound | `jump_rim_rattle_raw_selector` must be zero outside the explicit debug fixture; matrix corruption coverage exercises this. |
| Temporary instrumentation cleanup | R2DIAG/R2DBG/reason API appeared during diagnosis | All validator stderr probes and the exported/internal temporary failure-reason API were removed before this handoff. |
| Personal proof-root rerun | Supplied `r2-sol-personal-scene-20260803T231543Z` already contained Sol's ignored artifacts | Preserved the parent root and used a fresh `automated-scene-run-r1` child because the wrapper rejects nonempty roots. |
| R1 MAKE/A7A9 review | A raw low2==1 route could leave the rattle-selected bit active on a MAKE | Startup now activates selection only for nonlegacy evaluated MISS/A7A9; exact search resolves team0/roster0/vector0 `dx=-320,dy=64` frame42 sample `AC0D3E09` probability5, and both exact/native-approximate selector-1 MAKE terminal coverage enforce the distinction. |
| Post-R1 terminal-candidate verification | No implementation failure | `/W4` with `TECMO_SKIP_SHORTCUT=1` exited 0 with diagnostic scan count 0 and `BUILD_WARNING_SCAN_CLEAN`; TGCS/TGSR focused runners passed; fresh root `build/r2-sol-terminal-20260803T183632Z` produced `GAMEPLAY SCENE TEST PASS` and direct self-test `GAMEPLAY SCENE SELF TEST PASS`. |
| Native proof aggregate retry | `[Convert]::ToHexString` was unavailable in the PowerShell runtime | Read-only/ignored-output verification mistake; `BitConverter` resolved the retry. |
| Native proof exploratory helper | Helper name `H` collided with the `Get-History` alias | Read-only/ignored-output verification mistake; an unambiguous helper name resolved it. |
| Native proof aggregate formula | First recomputation used LF and mismatched the documented aggregate | The prose formula was corrected to uppercase per-frame hashes joined by CRLF with no trailing separator; actual frames never mismatched, and the terminal rerender reproduced all four aggregate hashes in both passes. |
| Docs-only final-check invocation | First whitespace-check wrapper interpolated `$path:$lineNo` without braces and failed to parse | No file or repository state changed; `${path}:$lineNo` was used on the successful rerun. |
| Docs-consistency search invocation | Initial `rg` search treated a pattern beginning with `--root` as an option | Read-only verification invocation mistake; the search was rerun with `rg --` and passed. |

No destructive recovery command, reset, merge, rebase, push, or excluded-file
edit was used. No proprietary ROM, capture, video, save state, or runtime
dependency was copied into the repository.

At the terminal candidate, all changed paths are owned paths, `git diff --check`
is clean, and explicit zero-diff checks confirm that both excluded shared files,
`src/tecmo_gameplay_scene.c` and
`src/asset_pack/tecmo_asset_pack_source_map.c`, remain untouched.

The worker implementation task is attempt 1 and has no task bad-request or
replacement fault. The separate evidence task had one ordinary failed read
command; Sol classified it as non-material, with no bad-request or replacement
fault.

## Pending review disposition

- Sol visual review is complete.
- Independent QA and final acceptance remain pending before final handoff.
- A future sequential transfer may update the shared `scene.c` shot-name
  switch and source-map wording; those shared-file follow-ups remain
  incomplete and are intentionally not part of this lane.
