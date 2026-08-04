# R2D Rules/Restarts Integration QA

Status: **SOL COMBINED QA PASS** at the accepted candidate. The guarded handoff remains conditional on a Good-SSH-signed report commit, an unchanged main guard, and the same pinned Luna task's signed-tip review.

Task identities:

- session: `S-SOL-R2D-RULES-RESTARTS-INTEGRATION-QA-001`
- task: `R2D-RULES-RESTARTS-INTEGRATION-QA`
- ownership: `OWN-R2D-RULES-RESTARTS-INTEGRATION-QA`
- lane: `LANE-R2D-RULES-RESTARTS-INTEGRATION-QA`
- report authored: `2026-08-04T07:31:25Z`

The allocated branch was fast-forwarded, without a merge commit, from reserved base `7fe2dd772af1d88f704a9272005b4ba557434cac` to accepted candidate `1f23235cd60582379da5e8f1713cd25de4ced62f` (tree `b668b45fa38807de6249b1a814d3e6baf96329f5`). No product, test, build, global-document, asset/source-map, main, staging, tracking, or remote ref was modified by this integration-QA layer. Its only tracked output is this owned report directory.

The combined gate passed:

- warning-clean canonical `build.ps1` console and GUI build;
- fresh Visual Studio CMake Release console and GUI build;
- strict TPNL, TGVR, TGBC, TGOR, TGCP, gameplay-audio, frontend-audio, music, asset-pack, and gameplay-asset suites;
- full GameplayScene, direct console scene, hidden GUI scene, and deterministic direct gameplay-state routes;
- direct flow, NativeFlow, Win32 launch, season, team-data, team-management, and current-main CLI smokes;
- canonical Rev1 ROM identity, proof determinism, repeat integrity, and original-resolution visual inspection.

No product finding was identified: `P0=0`, `P1=0`, `P2=0`, `P3=0` in the Sol matrix, matching the independent Luna candidate disposition.

The accepted boundary remains narrow. The change proves presentation/audio timing at the existing `scene_process_phase_audio` seam. It does not claim a new live foul/contact detector, any missing violation detector, free-throw/rebound/possession invention, complete blackout/fade fidelity, original 6502 intra-frame caller ordering, or a wider renderer/caller-order completion. The foul case remains a presentation fixture. Shot-clock expiry SFX 3 remains distinct at frame 0; shared foul/violation SFX 6 remains silent through frames 1-15, occurs exactly once at frame 16, and does not repeat. Dispatch remains reset, events, phase audio, then jump-miss audio.

See [LINEAGE.md](LINEAGE.md), [COMMANDS.md](COMMANDS.md), [EVIDENCE.md](EVIDENCE.md), and [INDEPENDENT-QA.md](INDEPENDENT-QA.md).
