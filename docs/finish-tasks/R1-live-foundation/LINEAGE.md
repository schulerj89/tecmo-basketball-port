# Lineage, review history, and diagnostic ledger

## Exact task lineage

- Authoritative Sol task: `019fc61e-0f2a-7fb0-a76e-e4676808c959`.
- Writable worker Luna: `019fc78a-69e1-7892-82ed-d14a39d37728`.
- Worker model/reasoning: `gpt-5.6-luna` / `max`.
- Worker created at: `2026-08-03T12:12:44.000Z`.
- Worker pinned: `true`.
- Branch: `codex/r1-live-foundation-luna`.
- Exact worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna`.
- Base, expected parent, and initial last-good: `ad0f005673692b04772bce3c3b4d3ac4b2624731`.
- Accepted CPU lineage held immutable: `db5a043 -> 8be7a9f -> a2f0238 -> ad0f005`.
- This lineage describes the first reviewed implementation/docs commit from
  this exact base; no history was rewritten, no branch was pushed, and no
  worktree was recreated.

## Completed read-only predecessor Lunas

These predecessors were read-only and completed before the writable worker; neither edited a repository, created a branch/worktree, ran a build/test/capture, or contacted any task other than the authoritative Sol task.

1. `019fc761-e213-7713-96a1-846a05fed558` — **Tecmo R1 LIVE Foundation — Original Lineup and Handoff Research — Luna Max**; `gpt-5.6-luna` / `max`; created pinned=`true`, completed, then unpinned after completion. It was read-only: no repository edits, builds, tests, emulator runs, or captures. Diagnostic/review history, in order: two missing-path `rg` diagnostics for `C:\Users\joshs\Projects\AGENTS.md` and `C:\Users\joshs\Projects\disassem\AGENTS.md` (recovered by searching the actual roots); two truncated instruction reads (`Warning: truncated output (original token count: 30028) Total output lines: 1841` and `Warning: truncated output (original token count: 50409) Total output lines: 3144`, recovered by bounded rereads); one malformed-workdir command (`Script error: exec_command failed ... CreateProcess { message: "Rejected(\"Failed to create unified exec process: The directory name is invalid. (os error 267)\")"`, recovered by correcting workdir); one missing decomp README; four missing bank manifest paths; one wrong-root search; one malformed ROM/FNV probe (`Cannot convert value "3.63426073959345E+16" to type "System.UInt32"... FullyQualifiedErrorId : InvalidCastIConvertible`, discarded and recovered with modulo-correct FNV); one additional output truncation (`Warning: truncated output (original token count: 13919) Total output lines: 1353`). Literal `{detail: bad request}` count `0`; no command-execution fault was reported beyond the one malformed-workdir signature.
2. `019fc765-8d36-7be2-b273-d5e617520061` — **Tecmo R1 LIVE Foundation — Native Integration and QA Audit — Luna Max**; `gpt-5.6-luna` / `max`; created pinned=`true`, completed, then unpinned after completion. It was read-only: zero files created/edited/deleted/renamed/formatted/generated, zero builds/tests/emulator/capture runs, and zero commits/branches/worktrees. Ordinary diagnostics, distinct from review findings: (1) one failed `rg` command, recovered with the corrected argument form; (2) one `read_thread_terminal` failure exactly `No app terminal session is attached to this thread yet.`; (3) one `functions.exec` syntax failure exactly `Unexpected identifier native_matchup_actor`, recovered by rebuilding plain text. Review findings L-01 through L-14 remain separately recorded: stale ownership/queue metadata, launch callers dropping session starters, actor%5 and 0..4 lookup limits, unwired CPU/shot lifecycle, preserved TIP handoff, zeroed fixtures, source-list/smoke/proof gaps, and the other native-audit findings corrected in the authoritative review path. Bad-request, mutation, build, test, emulator, capture, commit, and Git-context fault counts were all `0`.

## Ordered Sol review/revision history

1. Initial implementation integrated by-value starter binding, exact Bank04 static table values reused through the native-faithful/inferred LIVE layout policy, formation/play-state/step/shot adapters, phase handoff, human continuity, source-map evidence, tests, and proof tooling.
2. P0 review required restoration of `pretip`, `render_contract`, `shot_clock`, and `state_flow` orchestration exactly, including the final `tecmo_gameplay_scene_test_set_skip_pretip(false)`, and a distinct internal legacy-direct compatibility path. Restored; the unowned render-contract file was not edited.
3. Phase-safety review removed unconditional LIVE synchronization from PRETIP and required first post-handoff holder synchronization, orientation tracking, holder/team/controller coherence, and separate static `4/9` seeds. Implemented.
4. Transactionality review required candidate-only synchronization, movement, CPU metadata, ball, and shot decision; boundary-latched shot suppression; explicit unsupported/deferred playback. Implemented, including the candidate-initialization ordering correction in `scene_update_ai`.
5. Lifecycle review required one `play_state_initialize`, `step_budget=1`, source actor order, strict deferred-target validity, source-direction TGMO application, exact fixed-link/inferred matchup separation, and removal of redundant initialization. Implemented.
6. Validation review required aligned evolving offsets rather than start-offset equality, both foundation and play-state roles, controller-team mapping/duplicate rejection, serial wrap safety, strict target/direction sentinels, flag combinations, and scene ownership checks. Implemented.
7. Fixture reviews corrected the source-direction fixture (`primary_selected_actor=true`, initialized expected direction, separate setup diagnostic), the transaction snapshot timing, incoming state-flow fixture preservation, and the true out-of-range source-target negative. The raw failed focused signatures are retained below.
8. Production-path review added preseason and season starter propagation regressions, high-index profile/condition checks, bound human movement/pass/switch/routing checks, supported close-shot exact-once coverage, far-shot negatives, and source actor-target current-coordinate policy. Implemented.
9. Source-map review restored every immutable CPU-owned key/value and moved new behavior to additive `live_foundation_integration`. It also corrected TGFT selected-roster wording, TGMO static-layout classification, TGBD fixed-link classification, and added ROM/bank/span/byte-count/SHA assertions.
10. Proof-seam review added a deterministic bound nonidentity launch, real PRETIP, seven ordered events, 640x480 PNGs, machine-readable actor state, two repeats, dynamic 1920x1440 contact sheets, mandatory native MP4s, ffprobe decoded/stored validation, negative metadata/input gates, and preserved proof roots. The pretip event was added after the six-event review correction; seven events therefore produce three contact-sheet rows.
11. Proof loader review added `tecmo_music_asset_load_from_pack` and `tecmo_music_player_init` before scene loading, with shutdown, and retained the supplied same-pack/canonical ROM checks.
12. Proof event review added event-specific PRETIP/handoff/movement/pass/switch/CPU/shot assertions and full ten-actor JSON. Force-possession, stream-offset, and close-position mutations are explicitly fixture classifications.
13. Uniqueness and source-evidence reviews added per-side `seen[2][12]` ownership validation, strengthened internally consistent duplicate-starter corruption, and Bank03/Bank04 span metadata with complete hashes.
14. Contact/video/original-reference review corrected the dynamic contact-sheet height, required two native 640x480 MP4s at `39375000/655171` and `1/39375000`, validated the accepted CPU formal manifest rather than inventing an AVI, and required actual proof records instead of a template.
15. Formal-proof review added synthetic dirty/wrong-branch/wrong-base `-RequirePass` negatives, final status/SHA semantics, full artifact path/byte/SHA inventory validation, preserved proof-pack replay paths, nonempty copied logs, and final manifest rewrite after all suites.
16. Latest review corrected the nearest deterministic synthesized direction target and advanced the already-started close shot through one production outer update. The current draft records `shot_frame=1`, visible nonblack playback, and unchanged exact-once `action_serial=1`.
17. Sol’s current revise decision retained the worker uncommitted after independently passing the warning-clean build, immutable CPU wrapper (680/24/17), movement wrapper, preseason/season production flow, full four-suite scene wrapper, and prior draft proof/visual facts. It required a real production edge/corner `scene_update_ai` regression and a sustained bound running-clock integration. The first production attempt used raw-world `(0,120)` and failed before AI (`LIVE edge/corner production inert mismatch case=0 update=0 status=native pre-tip active phase=0 foundation=1 source=1/1/1 target=0/0,0 deferred=0 cpu=7/1/0,120/2/255/0/0 holder=0/0 possession=0/0`); recovery moved the fixture to scene-valid polygon boundaries and made synthesized targets require `scene_actor_coordinate_valid`. The corrected call now runs exactly once with all ten streams on the accepted deferred record, preserves actor/ball/ownership/orientation/action state, clears stale CPU metadata, retains the injected source direction, and passes. A transient owned diagnostic warning (`src/tecmo_gameplay_scene_test_state_flow.c(2372): warning C4473: 'snprintf': not enough arguments passed for format string`, one occurrence) was corrected by recording playback support in the poisoned-output diagnostic; the subsequent build was warning-clean. The final draft proof root is `build\\live-proof-edge-review-20260803-c`, produced with `-Build`, with `build_warning_clean=true`, 14/14 frames, matching 1920x1440 contacts, matching exact-cadence native videos (7/7 each), and 254 inventory entries. No bad-request signature occurred; literal `{detail: bad request}` count remains `0`.

18. Sol's precommit decision was PASS for the implementation/proof gate and
   authorized the first reviewed implementation/docs commit. The independent
   results were warning-clean `build.ps1`; CPU wrapper PASS with exactly `680`
   commands, `24` handlers, and `17` mutations; movement wrapper PASS;
   production flow PASS with exact output
   `FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit`;
   full `Run-GameplaySceneTests.ps1 -Build` PASS with all four suites; and
   `Run-Win32LaunchSmokeTest.ps1 -Build` PASS for GUI/console subsystems,
   project-root arguments, working directory/icon, roster-independent startup,
   lifetime, and clean shutdown. The personal proof root was
   `build/live-proof-sol-acceptance-20260803-d`, manifest SHA256
   `FAC2826E262E0EF5A88A8B8063D48D0D07A47CC72CF7B5F0E22BFF1954DC133D`,
   status `DRAFT`, base/current
   `ad0f005673692b04772bce3c3b4d3ac4b2624731`, final
   `PENDING_CLEAN_COMMIT`, `build_warning_clean=true`, original validated,
   `189/189` logs, `254/254` inventory entries, zero path/byte/hash mismatch,
   `14/14` frames, equal contacts hash
   `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`, equal
   native-video hash
   `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`,
   `7/7` stored/decoded frames per video, and exact cadence/timebase. The
   worker did not run `-RequirePass`; that clean formal proof and closure QA
   remain later steps.

## Sol read-only diagnostic note

The following diagnostics were read-only Sol review observations and are
separate from worker implementation faults:

- One initial manifest-schema probe passed object values with `png_path`
  instead of `.path`, producing `14` `Test-Path` null diagnostics and `14`
  `Get-FileHash` null diagnostics plus false `189/28` summaries. The corrected
  scan was `0/0/0`.
- One warning scan assumed two nonexistent build-console/build-win32 log
  names. The corrected actual `gameplay-scene-build.log` contained zero
  warnings.
- One canonical-repo orchestration `rg` used the wrong worktree and was
  recovered in the master-orchestrator worktree.
- No mutation or bad-request fault occurred.

## Diagnostic and fault ledger (raw signatures/counts/recovery)

| Raw signature | Count | Recovery/result |
|---|---:|---|
| `cmake : The term 'cmake' is not recognized...` | 1 | Used the existing MSVC `build.ps1` build path; no CMake rescope. |
| Initial MSVC `C4701` potentially-uninitialized `target` / `target_kind` warnings | 2 | Initialized candidate decision fields; later `build.ps1` output was warning-clean. |
| `rg: src\\tecmo_gameplay_scene*: filename...` | 1 | Reissued searches with explicit paths/`rg --files`. |
| `sed : The term 'sed' is not recognized...` | 1 | Used PowerShell inspection. |
| `Failed to initialize runtime from .\\` | 1 | Re-ran flow with the canonical decompilation root and same pack; passed. |
| `Gameplay scene test failed: LIVE source direction did not reach TGMO input: expected=1 actual=0 held=0 map=0` | 1 | Set `primary_selected_actor=true` in the fixture. |
| `Gameplay scene test failed: LIVE source direction did not reach TGMO input: expected=255 actual=0 held=0 map=0` | 1 | Initialized the expected direction and split setup/evaluation diagnostics. |
| `Gameplay scene test failed: LIVE failed transaction changed the complete scene` | 1 | Took the byte snapshot after intentional candidate corruption. |
| `Gameplay scene test failed: music-off violation entry failed` | 1 | Preserved and restored incoming launch/control fixtures and skip-preTIP state. |
| `Gameplay scene test failed: foul entry audio reset/policy diverged` | 1 | Stopped bound fixture state from leaking into accepted downstream cases. |
| `Gameplay scene test failed: first opposite-possession live follow failed` | 1 | Kept strict bound ownership validation while gating legacy-direct compatibility. |
| `Gameplay scene test failed: LIVE source actor-target invalid coordinate was accepted` | 1 | Restored `(0,0)` as legal and changed the negative to world-max+1. |
| `LIVE edge/corner source-direction fixture missing` | 1 | The accepted executor leaves direction sentinel metadata on normal deferred records; retained direct helper/play-step checks and changed the production fixture to inject a validated direction explicitly. |
| `LIVE edge/corner outward direction was not inert` | 1 | Replaced the direct-only coarse predicate with source-coordinate target selection and a production scene-path regression. |
| `LIVE edge/corner inert mismatch case=0 update=0 status=native pre-tip active expected_dir=1 actual_pos=0,120 expected_pos=0,120 source=1/1/0 deferred=0 cpu=0/0 holder=0/0 possession=0/0 shot=0 req=0` | 1 | Split setup/evaluation diagnostics and installed a validated candidate before the scene call. |
| `LIVE edge/corner inert mismatch case=0 update=0 phase=0 foundation=1 status=native pre-tip active expected_dir=1 actual_pos=0,120 expected_pos=0,120 source=1/1/0 deferred=0 cpu=0/0 holder=0/0 possession=0/0 shot=0 req=0` | 1 | Preserved the full foundation candidate through the synchronization call and retained the actual scene result for diagnosis. |
| `LIVE edge/corner inert mismatch case=0 target=0 update=0 phase=0 foundation=1 status=native pre-tip active expected_dir=1 actual_pos=0,120 expected_pos=0,120 source=1/1/0 deferred=0 cpu=0/0 holder=0/0 possession=0/0 shot=0 req=0` | 1 | Reworked the fixture to install all ten deferred streams and poison/clear the actual CPU record. |
| `LIVE edge/corner inert mismatch case=0 target=0 update=0 phase=0 foundation=1 status=native pre-tip active expected_dir=1 actual_pos=0,120 expected_pos=0,120 source=0/255/0 deferred=0 cpu=0/0 holder=0/0 possession=0/0 shot=0 req=0` | 1 | Preserved the validated source direction in the candidate and retained it as a post-call assertion. |
| `LIVE edge/corner inert classification mutated state` | 1 | Removed the tautological self-snapshot and replaced it with a real `scene_update_ai` transaction. |
| `LIVE edge/corner ownership baseline rejected` | 1 | Relaunched a clean bound fixture for each edge case and compared only the inert gameplay surface while allowing stream counters to advance. |
| `LIVE deferred tick did not clear stale CPU metadata` | 1 | Reset the edge fixture before the following stale-metadata regression. |
| `LIVE deferred metadata mismatch update=0 status=native pre-tip active cpu=7/305419896/528,144/2/255/0/1 shot=0/0/0 req=167772161` | 1 | Fixed the poisoned-output format/field coverage and isolated the stale test from edge state. |
| `LIVE edge/corner production inert mismatch case=0 update=0 status=native pre-tip active phase=0 foundation=1 source=1/1/1 target=0/0,0 deferred=0 cpu=7/1/0,120/2/255/0/0 holder=0/0 possession=0/0` | 1 | Raw-world edge was rejected by the playable-court validator; changed to scene-valid left/right/bottom boundaries and rejected synthesized outside-polygon targets. |
| `warning C4473: 'snprintf': not enough arguments passed for format string` | 1 | Added the missing playback-support argument and reran a warning-clean build. |
| `TGAI-1 source-map provenance is incomplete or malformed.` | 1 | Restored immutable accepted keys and used additive LIVE evidence. |
| `Decoded native LIVE proof video 'native-repeat-1' has 0 frames; expected 7.` | 1 | Fixed the framemd5 parser to accept whitespace after the final comma. |
| `Exception setting "require_pass": "The property 'require_pass' cannot be found ..."` | 1 | Added the initial manifest property before self-validation. |
| `Error formatting a string: Index ...` | 1 | Parenthesized the event-count arithmetic in the final summary format call. |
| Literal `LIVE PROOF {0}: root={1} ...` output | 1 | Replaced the ambiguous concatenation/format expression with a single formatted string. |
| `Gameplay scene test failed: LIVE home far-shot ball setup rejected` | 1 | Used valid y=`TECMO_GAMEPLAY_COURT_WORLD_MAX_Y - 15` for the far-shot fixture. |
| `Script error: exec_command failed ... Rejected ...` for the first shot-test command | 1 | Reran with an explicit workspace output path; the shot proof passed. |
| `read_thread received invalid arguments: turnLimit: Too big: expected number to be <=10.` | 1 | Reissued the read-only lineage query with `turnLimit=10`. |
| `ForEach-Object : Cannot bind parameter 'RemainingScripts'. Cannot convert the "-join" value ...` | 1 | Corrected the PowerShell inspection by assigning the collection first and joining it afterward. |
| `At line:2 char:434 ... An empty pipe element is not allowed.` | 1 | Reissued the read-only scope check with an explicit result array; all excluded paths reported unchanged. |
| `Script error: Unexpected token ';'` | 1 | Corrected an invalid local `functions.exec` inspection expression and reran the read-only proof metadata query. |
| `apply_patch verification failed: Failed to find expected lines ...` | 1 | Narrowed the documentation patch context and applied the same intended correction in smaller owned edits. |
| `Exception calling "Replace" with "2" argument(s): "String cannot be of zero length."` | 26 | A local path-normalization audit expression lost its backslash literal; reran with explicit string paths and obtained `UNOWNED_COUNT=0`. |
| `You cannot call a method on a null-valued expression.` | 26 | Same path-normalization audit expression as above; no repository mutation, recovered by the explicit-path audit. |
| `At line:11 char:115 ... Missing closing ')' in expression.` / `Unexpected token ')' in expression or statement.` | 1 | A PowerShell scope/base inspection combined command exit capture inside an invalid expression; reran with separate exit-code assignments and obtained the exact scope/base result. |
| Literal `{detail: bad request}` | 0 | No occurrence observed. |

The full wrapper runs that were successful at the suite level but rejected as proof checkpoints are preserved in the review history: the zero-decoded-video run, missing-manifest-property run, final-format-error run, and literal-placeholder-output run. They were not overwritten; each recovery produced a new accepted timestamped root. The accepted draft with the supplied original manifest is `build\\live-proof-edge-review-20260803-c`.

## Current status

This record is the handoff for the first reviewed implementation/docs commit.
The worker did not run clean `-RequirePass` in this turn. The next bounded
sequence is a clean `-Build -RequirePass` proof at that commit, then a
docs-only closure commit recording the formal proof/lineage and independent
QA, followed by QA continuity checks and an ff-only merge into Sol.
