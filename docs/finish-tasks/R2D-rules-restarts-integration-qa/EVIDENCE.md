# Combined QA evidence

## Static integration seam

The only production behavior added by the accepted product parent is delayed presentation-audio dispatch inside `scene_process_phase_audio`. Foul presentation selects penalty metadata selector 0; violation presentation selects its violation metadata. A request is queued only when `phase_frame == presentation_sfx_delay_frames`.

The surrounding update order remains:

1. `scene_apply_phase_audio_reset`
2. `scene_process_events`
3. `scene_process_phase_audio`
4. jump-miss result dispatch through `scene_shot_queue_result_audio`

No immediate violation cue remains on phase entry. The additive test source is registered in both build systems and invoked by the GameplayScene orchestrator.

The focused rules/restarts assertions passed for both teams and music enabled/disabled cases:

- shot-clock expiry queues SFX 3 at frame 0;
- shared SFX 6 is absent on frames 1-15;
- foul and violation presentations queue SFX 6 exactly once at frame 16;
- consuming the mailbox produces no repeat through the tested remainder;
- OOB/backcourt restart events, possession, holder, TGOR serial, TGBC reset, camera/projection/ball coherence, clock freeze, and music policy remain coherent.

## Build and suite matrix

| Gate | Sol result |
| --- | --- |
| canonical `build.ps1` console + GUI | exit 0; diagnostic-clean |
| fresh VS CMake configure | exit 0 |
| fresh VS CMake Release `tecmo_port` | exit 0; diagnostic-clean |
| fresh VS CMake Release `tecmo_port_game` | exit 0; diagnostic-clean |
| TPNL / TGVR / TGBC / TGOR / TGCP | all exit 0 |
| strict gameplay/frontend audio and music | all exit 0 |
| ROM-only asset pack and TGPL | both exit 0 |
| full GameplayScene | exit 0; `GAMEPLAY SCENE TEST PASS` |
| direct console / hidden GUI scene | exit 0 / exit 0 |
| direct gameplay-state repeat | exit 0 twice; equal output/hash |
| direct flow / NativeFlow / Win32 | all corrected exits 0 |
| season / team data / team management | all exit 0; 13 / 15 / 9 pixel checkpoints |
| direct current-main CLI matrix | all exit 0 |

Canonical Rev1 identity was independently read and not copied:

- bytes: `393232`
- SHA-256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

## Sol deterministic proof

Proof root: ignored `build/R2D-Sol-scene-proof-001`.

- schema: `tecmo.live-proof-manifest/TGLP-1`
- status: `DRAFT`, solely because optional `-RequirePass` is hard-coded to an unrelated R1 base; it was not used
- candidate: `1f23235cd60582379da5e8f1713cd25de4ced62f`
- branch and clean state: exact and `true`
- `build_warning_clean=true`, `suites_complete=true`, `require_pass=false`
- native frame size: `640x480`
- cadence: `39375000/655171`
- repeats: `2`
- stored/decoded frames: `14/14`
- normalized repeat pairs: `25`
- missing or mismatched pairs: `0`
- manifest bytes/SHA-256: `554078` / `DB0784AFA6E4ADA0CE9F203C80B74A8C5EA7856BB167A676175857C1E69FC822`
- proof asset pack bytes/SHA-256: `1406713` / `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`
- each contact sheet: `1920x1440`, 7 frames, `163089` bytes, SHA-256 `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`
- each native MP4: `37915` bytes, SHA-256 `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`
- each native video: 7 stored and 7 decoded frames, decoded-list SHA-256 `6FA0AA43130E1EFF92986485EC6305ABE9A86781968FEC24371FA45616F13E9B`

The two contact sheets are byte-identical. The two MP4s are byte-identical. All seven event JSONL pairs and their frames match after the repeat-label normalization.

## Original-resolution visual inspection

The Sol inspected the 1920x1440 contact sheet and representative named 640x480 captures at original detail, not thumbnails.

- `gameplay-live-start`: court, HUD, player sprites, crowd, hoop, and camera composition are intact; no broken sprite, text collision, or HUD overlap was observed.
- OOB frames 23 and 39: coherent centered referee presentation with readable `OUT OF BOUNDS`; frame 31 is a coherent partial gesture between bounded pose groups.
- shot-clock frame 0: intentional black capture consistent with `black_frames=9`.
- shot-clock frame 16: centered referee and readable `SHOT CLOCK VIOLATION`, aligned with the tested phase-audio cue frame.
- shot-clock frame 80: coherent later referee gesture; no complete-fade or caller-order conclusion is drawn from this capture.
- backcourt frame 27: centered referee and readable `BACKCOURT`.
- backcourt frame 167: coherent partial terminal gesture within the documented 168-frame settlement; it is not treated as a complete blackout/fade claim.

The direct render hashes matched the same Luna captures for shot-clock frame 80 and backcourt frame 167, excluding a task-viewer discrepancy. Early/late black or partial referee frames are classified by the accepted `black_frames=9`, `sequence_visible_start_frame=23`, and 168-frame bounded sequence metadata.

## Boundaries and disposition

The evidence proves the semantic asset/native-faithful presentation and audio integration seam. It does not prove or add:

- live foul/contact detection;
- additional violation detectors;
- free-throw, rebound, or possession policy;
- complete blackout/fade equivalence;
- exact original 6502 intra-frame caller order;
- completion of every renderer or caller-order boundary.

The foul case remains an explicit presentation fixture. No accepted incomplete/approximate statement was removed or widened.

Sol disposition: **PASS**, `P0=0`, `P1=0`, `P2=0`, `P3=0`.
