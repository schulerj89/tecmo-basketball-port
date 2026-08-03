# Tecmo Basketball Finish Status Dashboard

Generated from committed JSON at `2026-08-03T19:32:58Z`. This dashboard reports coordination state only; it is not product QA.

## Program

- Base SHA: `63b29b04b1ab4745b7b8d5dd0499942d1bf8ba4e`
- Inventory: `complete`
- Project acceptance: `incomplete`
- Open external blockers: `0`
- Task states: `backlog` 12, `in_progress` 5, `pushed` 9, `ready_for_round_staging` 2
- Fidelity classifications: `incomplete` 24

## Sol Orchestration Capacity

- Single master authority: `True`
- Second-master policy: `recovery_replacement_only`
- Active domain Sols: `5`
- Cleared for creation: `0`
- Target active domain Sols: `4`
- Monitoring limit: `8`

| Lane | Domain | Readiness | Dependencies | Tasks | Sol | Branch | Next gate |
|---|---|---|---|---|---|---|---|
| LANE-R1-GAMEPLAY-FOUNDATION | gameplay_foundation | active | sequentially_active | R1-TIP-FIDELITY | S-SOL-R1-GAMEPLAY-001 | codex/r1-gameplay-foundation-sol | TIP continues independently from accepted CPU+LIVE base 222d75cf under the existing domain Sol and projectless evidence worker. It no longer gates the R1A CPU+LIVE delivery subround. |
| LANE-R3-SEASON-DATA-FOUNDATION | season_data | complete | complete | R3-SEASON-DATA-FOUNDATION | S-SOL-R3-SEASON-DATA-001 | codex/r3-season-data-foundation-sol | Accepted season/data foundation is frozen at f536a193. Its separate R3A Integration QA lane is cleared for creation and no longer waits for downstream statistics, save, management, or All-Star tasks. |
| LANE-R1A-INTEGRATION-QA | integration | active | ready | R1A-INTEGRATION-QA | S-SOL-R1A-INTEGRATION-QA-001 | codex/r1a-cpu-live-integration-qa-sol | First reconciliation/proof at f98fea3 is valid but no longer fast-forwardable after R4A advanced main to bcacd5b. Explicitly reconcile bcacd5b into only the R1A branch after zero-overlap verification, rerun full combined proof, reuse the same Luna, and sign a true fast-forward handoff. |
| LANE-R3A-INTEGRATION-QA | integration | complete | complete | R3A-INTEGRATION-QA | S-SOL-R3A-INTEGRATION-QA-001 | codex/r3a-season-data-integration-qa-sol | Complete: signed dd096cb passed dedicated R3A Integration QA and was non-force pushed to main/origin. Integration Sol may be unpinned after this durable master record; downstream R3 features must start from then-current main and retain documented deferrals. |
| LANE-R4-FRONTEND-INTRO-TITLE | frontend | active | ready | R4-FRONTEND-INTRO-TITLE | S-SOL-R4-FRONTEND-RECOVERY-001 | codex/r4-frontend-intro-title-sol | Independent QA is NOT CLEAR at 0a3fc57f: correct synthetic-only fingerprint handling without weakening strict public import, enforce canonical 262144-byte/FNV1a64 finale chr/all identity on first and later checks, add standalone/corrupted-CHR regressions, rerun full proof, and close the same QA Luna at P0/P1/P2 zero. |
| LANE-R4-AUDIO-FOUNDATION | audio | complete | complete | R4-AUDIO-FOUNDATION | S-SOL-R4-AUDIO-001 | codex/r4-audio-foundation-sol | Complete: accepted audio foundation and signed R4A Integration QA report are pushed on main/origin at bcacd5b. Broader cross-domain cue routing/full ACC-AUDIO remains deferred in parent R4. |
| LANE-R4A-INTEGRATION-QA | integration | complete | complete | R4A-INTEGRATION-QA | S-SOL-R4A-INTEGRATION-QA-001 | codex/r4a-audio-integration-qa-sol | Complete: same-Luna P0/P1/P2-zero closure and signed report bcacd5b were master-verified, fast-forwarded, non-force pushed, and remote-verified. Luna and Sol are completed/unpinned; branches/worktrees are preserved. |
| LANE-R2-SHOTS-OUTCOMES | gameplay_behavior | active | ready | R2-SHOTS-OUTCOMES | S-SOL-R2-SHOTS-001 | codex/r2-shots-outcomes-sol | One persistent writable Luna remains active/pinned with owned-only uncommitted corrections. Complete loader transactions, deep validation, exhaustive tests/docs, stable commit, proof, and independent terminal QA while preserving LIVE far-shot deferral. Source-map stays excluded during active TIP ownership; document numeric-1 pose-only approximation versus unchanged exact TGCS metadata and stale wording honestly. |
| LANE-R2-CLOCK-LINEUPS-FATIGUE | gameplay_behavior | active | ready | R2-CLOCK-LINEUPS-FATIGUE | S-SOL-R2-CLOCK-LINEUPS-001 | codex/r2-clock-lineups-fatigue-sol | Pinned Sol confirmed clean takeover and complete governing-file read. Three projectless read-only Luna Max audits cover clocks/periods, lineups/substitutions, and fatigue/native coupling while Sol performs authoritative inventory; reconcile reports before a single persistent writable lineage and request any shared scene/core rescope explicitly. |

## Rounds

| Round | Status | Base | Tasks | Staging | Combined QA | Push |
|---|---|---|---:|---|---|---|
| R0 | pushed | 63b29b04b1ab | 1 | codex/master-finish-orchestration | coordination_only | succeeded |
| R0A | pushed | 7090d2c62201 | 4 | codex/master-finish-orchestration | coordination_only | succeeded |
| R1A | combined_qa | 6d8f9c7a99a7 | 3 | codex/round-1-gameplay-foundation-staging | in_progress | not_attempted |
| R1 | in_progress | 222d75cfafa9 | 1 | codex/round-1-tip-staging | pending | not_attempted |
| R2 | in_progress | 7090d2c62201 | 5 | codex/round-2-gameplay-mechanics-staging | pending | not_attempted |
| R3A | pushed | 6d8f9c7a99a7 | 2 | codex/round-3-season-data-staging | accepted | succeeded |
| R3 | in_progress | 6d8f9c7a99a7 | 4 | codex/round-3-season-completion-staging | pending | not_attempted |
| R4A | pushed | 6d8f9c7a99a7 | 2 | codex/round-4a-audio-foundation-staging | accepted | succeeded |
| R4 | planned | 6d8f9c7a99a7 | 3 | codex/round-4-frontend-audio-staging | pending | not_attempted |
| R5 | planned | 7090d2c62201 | 3 | codex/round-5-release-staging | pending | not_attempted |

## Queue

| Priority | Task | Domain | Round | State | Sol session | Branch | Result commits | QA | Merge |
|---:|---|---|---|---|---|---|---:|---|---|
| 100 | R0-CTRL-001 | orchestration | R0 | pushed | S-MASTER-001 | codex/master-finish-orchestration | 2 | coordination_validated | pushed |
| 100 | R0A-INV-001 | orchestration | R0A | pushed | S-MASTER-001 | codex/master-finish-orchestration | 2 | coordination_validated | pushed |
| 100 | R1-CPU-PLAY-LIFECYCLE | gameplay_behavior | R1A | ready_for_round_staging | S-SOL-R1-GAMEPLAY-001 | codex/r1-gameplay-foundation-sol | 4 | passed | staged |
| 100 | R1A-INTEGRATION-QA | integration | R1A | in_progress | S-SOL-R1A-INTEGRATION-QA-001 | codex/r1a-cpu-live-integration-qa-sol | 1 | pending | not_ready |
| 100 | R2-SHOTS-OUTCOMES | gameplay_behavior | R2 | in_progress | S-SOL-R2-SHOTS-001 | codex/r2-shots-outcomes-sol | 0 | in_progress | not_ready |
| 100 | R3-PLAYER-STATS-LEADERS | season_data | R3 | backlog | - | - | 0 | pending | not_ready |
| 100 | R3-SEASON-DATA-FOUNDATION | season_data | R3A | pushed | S-SOL-R3-SEASON-DATA-001 | codex/r3-season-data-foundation-sol | 7 | passed | pushed |
| 100 | R3A-INTEGRATION-QA | integration | R3A | pushed | S-SOL-R3A-INTEGRATION-QA-001 | codex/r3a-season-data-integration-qa-sol | 1 | passed | pushed |
| 100 | R4-FRONTEND-INTRO-TITLE | frontend | R4 | in_progress | S-SOL-R4-FRONTEND-RECOVERY-001 | codex/r4-frontend-intro-title-sol | 10 | in_progress | not_ready |
| 100 | R4A-INTEGRATION-QA | integration | R4A | pushed | S-SOL-R4A-INTEGRATION-QA-001 | codex/r4a-audio-integration-qa-sol | 2 | passed | pushed |
| 100 | R5-ASSET-BUILD-PROVENANCE | assets_build | R5 | backlog | - | - | 0 | pending | not_ready |
| 99 | R0A-ADOPT-CPU-TIP | gameplay_behavior | R0A | pushed | S-SOL-CPU-TIP-LEGACY | codex/cpu-tipoff-behavior | 1 | historical_sol_accepted | pushed |
| 99 | R4-AUDIO-FOUNDATION | audio | R4A | pushed | S-SOL-R4-AUDIO-001 | codex/round-4a-audio-foundation-staging | 10 | passed | pushed |
| 98 | R0A-ADOPT-TIP-VIS | gameplay_presentation | R0A | pushed | S-SOL-TIP-VIS-LEGACY | codex/tipoff-visual-orientation | 6 | historical_sol_accepted | pushed |
| 98 | R1-LIVE-FOUNDATION | gameplay_behavior | R1A | ready_for_round_staging | S-SOL-R1-GAMEPLAY-001 | codex/r1-gameplay-foundation-sol | 3 | passed | staged |
| 98 | R2-DEFENSE-CONTACT | gameplay_behavior | R2 | backlog | - | - | 0 | pending | not_ready |
| 98 | R3-SEASON-PROGRESSION-SAVE | season_data | R3 | backlog | - | - | 0 | pending | not_ready |
| 98 | R4-MENUS-UI | frontend | R4 | backlog | - | - | 0 | pending | not_ready |
| 98 | R5-PARITY-GAP-CLOSURE | integration | R5 | backlog | - | - | 0 | pending | not_ready |
| 97 | R0A-ADOPT-AWAY-FACING | gameplay_presentation | R0A | pushed | S-SOL-AWAY-FACING-LEGACY | codex/away-facing-left-only | 2 | historical_sol_accepted | pushed |
| 96 | R1-TIP-FIDELITY | gameplay_behavior | R1 | in_progress | S-SOL-R1-GAMEPLAY-001 | codex/r1-gameplay-foundation-sol | 0 | in_progress | not_ready |
| 96 | R2-RULES-RESTARTS | gameplay_behavior | R2 | backlog | - | - | 0 | pending | not_ready |
| 96 | R3-TEAM-MGMT-DATA | season_data | R3 | backlog | - | - | 0 | pending | not_ready |
| 96 | R4-AUDIO | audio | R4 | backlog | - | - | 0 | pending | not_ready |
| 96 | R5-E2E-RELEASE-QA | integration | R5 | backlog | - | - | 0 | pending | not_ready |
| 94 | R2-CLOCK-LINEUPS-FATIGUE | gameplay_behavior | R2 | in_progress | S-SOL-R2-CLOCK-LINEUPS-001 | codex/r2-clock-lineups-fatigue-sol | 0 | pending | not_ready |
| 94 | R3-ALLSTAR | season_data | R3 | backlog | - | - | 0 | pending | not_ready |
| 92 | R2-GAMEPLAY-PRESENTATION | gameplay_presentation | R2 | backlog | - | - | 0 | pending | not_ready |

## Active Sessions

| Session | Role | Model/thinking | Status | Pin | Tasks | Branch | Worktree | Last good |
|---|---|---|---|---|---|---|---|---|
| S-MASTER-001 | master | gpt-5.6-sol/max | active | pinned | R0-CTRL-001, R0A-INV-001 | codex/master-finish-orchestration | C:/Users/joshs/Projects/tecmo-basketball-port-master-orchestrator | 5b8a13b30620 |
| S-SOL-R1-GAMEPLAY-001 | domain_orchestrator | gpt-5.6-sol/max | active | pinned | R1-CPU-PLAY-LIFECYCLE, R1-LIVE-FOUNDATION, R1-TIP-FIDELITY | codex/r1-gameplay-foundation-sol | C:/Users/joshs/Projects/tecmo-basketball-port-r1-gameplay-foundation-sol | 222d75cfafa9 |
| S-SOL-R1A-INTEGRATION-QA-001 | integration_orchestrator | gpt-5.6-sol/max | active | pinned | R1A-INTEGRATION-QA | codex/r1a-cpu-live-integration-qa-sol | C:/Users/joshs/Projects/tecmo-basketball-port-r1a-cpu-live-integration-qa-sol | f98fea320bf2 |
| S-SOL-R4-FRONTEND-RECOVERY-001 | domain_orchestrator | gpt-5.6-sol/max | active | pinned | R4-FRONTEND-INTRO-TITLE | codex/r4-frontend-intro-title-sol | C:/Users/joshs/Projects/tecmo-basketball-port-r4-frontend-intro-title-sol | 0a3fc57f0a38 |
| S-SOL-R2-SHOTS-001 | domain_orchestrator | gpt-5.6-sol/max | active | pinned | R2-SHOTS-OUTCOMES | codex/r2-shots-outcomes-sol | C:/Users/joshs/Projects/tecmo-basketball-port-r2-shots-outcomes-sol | 222d75cfafa9 |
| S-SOL-R2-CLOCK-LINEUPS-001 | domain_orchestrator | gpt-5.6-sol/max | active | pinned | R2-CLOCK-LINEUPS-FATIGUE | codex/r2-clock-lineups-fatigue-sol | C:/Users/joshs/Projects/tecmo-basketball-port-r2-clock-lineups-fatigue-sol | 222d75cfafa9 |
| S-SOL-GITHUB-SIGNING-001 | security_operator | gpt-5.6-sol/high | active | pinned |  | - | - | bcacd5b6963f |

## Active Ownership

| Claim | Task | Round | Mode | Writable globs | Concurrency group |
|---|---|---|---|---|---|
| OWN-R1-CPU-PLAY | R1-CPU-PLAY-LIFECYCLE | R1A | exclusive | include/tecmo_gameplay_cpu_*.h<br>src/tecmo_gameplay_cpu_*.c<br>src/asset_pack/tecmo_asset_pack_gameplay_cpu_*.c<br>src/asset_pack/tecmo_asset_pack_gameplay_cpu_*.h<br>tools/Run-GameplayCpuSteeringTests.ps1<br>tools/gameplay-lab/**<br>docs/finish-tasks/R1-cpu-play-lifecycle/** | R1-gameplay-foundation |
| OWN-R1-TIP | R1-TIP-FIDELITY | R1 | exclusive | include/tecmo_gameplay_pretip.h<br>src/tecmo_gameplay_pretip.c<br>src/tecmo_gameplay_scene_render.c<br>src/tecmo_gameplay_scene_test_pretip.c<br>src/tecmo_gameplay_scene_test_render_contract.c<br>src/tecmo_cli_render_gameplay_checkpoint.c<br>src/tecmo_win32_keys.c<br>src/asset_pack/tecmo_asset_pack_gameplay_pretip.c<br>src/asset_pack/tecmo_asset_pack_gameplay_pretip.h<br>src/asset_pack/tecmo_asset_pack_source_map.c<br>src/tecmo_gameplay_scene.c<br>tools/New-TipoffVisualProof.ps1<br>tools/Run-GameplayPreTipTests.ps1<br>tools/Run-GameplaySceneTests.ps1<br>docs/finish-tasks/R1-tip-fidelity/** | R1-gameplay-foundation |
| OWN-R4-FRONTEND-INTRO-TITLE | R4-FRONTEND-INTRO-TITLE | R4 | exclusive | include/tecmo_intro_*.h<br>src/tecmo_intro_*.c<br>include/tecmo_title_screen.h<br>src/tecmo_title_screen.c<br>src/asset_pack/tecmo_asset_pack_opening.c<br>src/asset_pack/tecmo_asset_pack_opening.h<br>src/asset_pack/tecmo_asset_pack_arena.c<br>src/asset_pack/tecmo_asset_pack_arena.h<br>src/asset_pack/tecmo_asset_pack_finale.c<br>src/asset_pack/tecmo_asset_pack_finale.h<br>src/asset_pack/tecmo_asset_pack_post_arena.c<br>src/asset_pack/tecmo_asset_pack_post_arena.h<br>src/asset_pack/tecmo_asset_pack_d9f6.c<br>src/asset_pack/tecmo_asset_pack_d9f6.h<br>src/asset_pack/tecmo_asset_pack_title.c<br>src/asset_pack/tecmo_asset_pack_title.h<br>tools/Run-IntroSequenceTests.ps1<br>tools/New-IntroLayoutDraft.ps1<br>tools/Import-IntroArenaCapture.ps1<br>tools/Find-Intro*.ps1<br>tools/Find-NesReferenceIntro.ps1<br>tools/Find-Title*.ps1<br>tools/emu_intro_*.lua<br>docs/finish-tasks/R4-frontend-intro-title/**<br>src/tecmo_cli_render_scene_modes.c | R4-frontend-intro-title |
| OWN-R2-SHOTS-OUTCOMES | R2-SHOTS-OUTCOMES | R2 | exclusive | include/tecmo_gameplay_close_shots.h<br>include/tecmo_gameplay_jump_shots.h<br>include/tecmo_gameplay_dunk_cutaway.h<br>include/tecmo_gameplay_shot_resolution.h<br>include/tecmo_gameplay_scene.h<br>include/tecmo_gameplay_scene_internal.h<br>src/tecmo_gameplay_close_shots.c<br>src/tecmo_gameplay_jump_shots.c<br>src/tecmo_gameplay_dunk_cutaway.c<br>src/tecmo_gameplay_shot_resolution.c<br>src/tecmo_gameplay_scene_shots.c<br>src/tecmo_gameplay_scene_validation.c<br>src/tecmo_gameplay_scene_test_internal.h<br>src/tecmo_gameplay_scene_test_orchestrator.c<br>src/tecmo_gameplay_scene_test_state_flow.c<br>src/tecmo_cli_gameplay_shots.c<br>src/tecmo_cli_gameplay_shot_resolution.c<br>src/asset_pack/tecmo_asset_pack_gameplay_close_shots.c<br>src/asset_pack/tecmo_asset_pack_gameplay_close_shots.h<br>src/asset_pack/tecmo_asset_pack_gameplay_jump_shots.c<br>src/asset_pack/tecmo_asset_pack_gameplay_jump_shots.h<br>src/asset_pack/tecmo_asset_pack_gameplay_dunk_cutaway.c<br>src/asset_pack/tecmo_asset_pack_gameplay_dunk_cutaway.h<br>src/asset_pack/tecmo_asset_pack_gameplay_shot_resolution.c<br>src/asset_pack/tecmo_asset_pack_gameplay_shot_resolution.h<br>tools/Run-GameplayCloseShotTests.ps1<br>tools/Run-GameplayShotResolutionTests.ps1<br>docs/finish-tasks/R2-shots-outcomes/** | R2-shots-outcomes |
| OWN-R2-CLOCK-LINEUPS-FATIGUE | R2-CLOCK-LINEUPS-FATIGUE | R2 | exclusive | include/tecmo_gameplay_state.h<br>include/tecmo_gameplay_free_throw_lineup.h<br>include/tecmo_gameplay_fatigue.h<br>include/tecmo_gameplay_free_throw_projection_test.h<br>src/tecmo_gameplay_state.c<br>src/tecmo_gameplay_free_throw_lineup.c<br>src/tecmo_gameplay_fatigue.c<br>src/tecmo_gameplay_free_throw_projection_test.c<br>src/tecmo_gameplay_scene_test_shot_clock.c<br>src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.c<br>src/asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h<br>src/asset_pack/tecmo_asset_pack_gameplay_fatigue.c<br>src/asset_pack/tecmo_asset_pack_gameplay_fatigue.h<br>tools/Run-GameplayFreeThrowLineupTests.ps1<br>tools/Run-GameplayFatigueTests.ps1<br>docs/finish-tasks/R2-clock-lineups-fatigue/** | R2-clock-lineups-fatigue |
| OWN-R1A-INTEGRATION-QA | R1A-INTEGRATION-QA | R1A | exclusive | docs/finish-tasks/R1A-cpu-live-integration-qa/** | R1A-integration-qa |

## External Blockers

| Blocker | Task | Category | Status | Required action |
|---|---|---|---|---|
| BLOCK-R4-AUDIO-LISTENING-001 | R4-AUDIO-FOUNDATION | material_product_decision | resolved | Listen to the WAV and report approval or defect timestamps for opening/tail/end, music loops/stinger, cue separability, mixed override, and TDMC held/retrigger/stop windows; alternatively explicitly authorize objective waveform/spectrum/event inspection as the accepted substitute for this round. |

## Completion Matrix

| Criterion | Domain | Classification | Status | Tasks | Evidence |
|---|---|---|---|---|---|
| ACC-FRONT-OPENING: Opening, intro, title, attract, and start animations/timing match accepted original references | frontend | incomplete | pending | R4-FRONTEND-INTRO-TITLE |  |
| ACC-FRONT-MENUS: All menu transitions, input behavior, and UI details are complete | frontend | incomplete | pending | R4-MENUS-UI |  |
| ACC-DATA-PAGES: Player/team data pages are fully wired, arrow-aligned, and free of extra zeros/periods | season_data | incomplete | pending | R3-TEAM-MGMT-DATA, R4-MENUS-UI |  |
| ACC-PRESEASON: Preseason selection, launch, completed game, and return flow are complete | season_data | incomplete | pending | R3-SEASON-PROGRESSION-SAVE, R4-MENUS-UI |  |
| ACC-SEASON-PROGRESSION: Season schedule, progression, completed games, and completed-season flow are complete | season_data | incomplete | pending | R3-SEASON-PROGRESSION-SAVE |  |
| ACC-STANDINGS-SAVE: Standings and save/load preserve validated season state | season_data | incomplete | pending | R3-SEASON-PROGRESSION-SAVE |  |
| ACC-MANAGEMENT: Team management, starters, playbook, substitutions, and roster wiring are complete | season_data | incomplete | pending | R1-LIVE-FOUNDATION, R3-TEAM-MGMT-DATA |  |
| ACC-ALLSTAR: All-Star functionality is complete and launches/returns correctly | season_data | incomplete | pending | R3-ALLSTAR |  |
| ACC-LEADERS: League Leaders uses real ranked per-player statistical data | season_data | incomplete | pending | R3-PLAYER-STATS-LEADERS |  |
| ACC-PLAYER-STATS: Per-player game/season statistics feed data pages, saves, and leader screens | season_data | incomplete | pending | R3-PLAYER-STATS-LEADERS, R3-SEASON-PROGRESSION-SAVE |  |
| ACC-CPU-POLICY: CPU play selection, formation, movement, spacing, links, matchups, switching, decisions, and shot timing are complete | gameplay_behavior | incomplete | pending | R0A-ADOPT-CPU-TIP, R1-CPU-PLAY-LIFECYCLE, R1-LIVE-FOUNDATION | EVID-ADOPT-CPU-KERNEL |
| ACC-MOTION-ANIMATION: Directional poses, dribble, pass, jump, shot, defense, contact, foul, free-throw, rebound, block, and steal animations are complete | gameplay_presentation | incomplete | pending | R0A-ADOPT-TIP-VIS, R0A-ADOPT-AWAY-FACING, R2-GAMEPLAY-PRESENTATION | EVID-ADOPT-TIP-VIS-PROOF, EVID-ADOPT-AWAY-PROOF |
| ACC-SHOTS: Shot selection, launch, resolution, supported outcomes, layups, dunks, and jump shots are complete | gameplay_behavior | incomplete | pending | R2-SHOTS-OUTCOMES |  |
| ACC-TIPOFF: Tip-off input, visible jump, presentation, claim, tie, and live handoff are complete | gameplay_behavior | incomplete | pending | R0A-ADOPT-CPU-TIP, R0A-ADOPT-TIP-VIS, R1-TIP-FIDELITY | EVID-ADOPT-TIP-INPUT, EVID-ADOPT-TIP-VIS-PROOF |
| ACC-CLOCK-PERIODS: Game clock, shot clock, periods, halftime, overtime, scoring, and final-game flow are complete | gameplay_behavior | incomplete | pending | R2-CLOCK-LINEUPS-FATIGUE, R3-SEASON-PROGRESSION-SAVE |  |
| ACC-RULES-POSSESSION: Possession, violations, contact, fouls, referee, restarts, and free throws are complete | gameplay_behavior | incomplete | pending | R2-DEFENSE-CONTACT, R2-RULES-RESTARTS |  |
| ACC-LINEUPS-FATIGUE: Substitutions, active lineups, matchup ownership, and fatigue affect live play correctly | gameplay_behavior | incomplete | pending | R1-LIVE-FOUNDATION, R2-CLOCK-LINEUPS-FATIGUE, R3-TEAM-MGMT-DATA |  |
| ACC-COURT-CAMERA-HUD: Court, camera, HUD, edge rendering, sprite ordering, clipping, and cutaways are complete | gameplay_presentation | incomplete | pending | R0A-ADOPT-TIP-VIS, R0A-ADOPT-AWAY-FACING, R2-GAMEPLAY-PRESENTATION | EVID-ADOPT-TIP-VIS-PROOF, EVID-ADOPT-AWAY-ASM, EVID-ADOPT-AWAY-PROOF |
| ACC-AUDIO: Opening, menu, gameplay, halftime, final music/SFX/samples/cue routing/device behavior are complete | audio | incomplete | pending | R4-AUDIO |  |
| ACC-ASSET-PACK: Asset-pack rebuild/import is deterministic, strict, provenance-safe, and owns normal runtime data | assets_build | incomplete | pending | R5-ASSET-BUILD-PROVENANCE |  |
| ACC-REBUILD: Executable and assets rebuild completely from documented clean commands | assets_build | incomplete | pending | R5-ASSET-BUILD-PROVENANCE, R5-E2E-RELEASE-QA |  |
| ACC-FULL-QA: All automated suites, smoke tests, full game, and full season pass on accepted staging | integration | incomplete | pending | R5-PARITY-GAP-CLOSURE, R5-E2E-RELEASE-QA |  |
| ACC-END-TO-END-REFERENCE: End-to-end original-reference visual/audio comparison is complete | integration | incomplete | pending | R5-PARITY-GAP-CLOSURE, R5-E2E-RELEASE-QA |  |
| ACC-LEGAL: Repository and runtime preserve the clean legal/provenance boundary with no prohibited tracked artifacts | assets_build | incomplete | pending | R5-ASSET-BUILD-PROVENANCE, R5-E2E-RELEASE-QA |  |

## Recovery

Read `MASTER_PLAN.md`, validate all state, verify Git lineage, then contact only active Sol orchestrators registered above.
