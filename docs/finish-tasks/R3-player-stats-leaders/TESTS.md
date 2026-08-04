# R3 Player Stats Leaders — Test Handoff

## Test inputs and evidence root

The local QA root was:

`build/integration-qa-r3-20260804T184202450Z/`

All generated packs, logs, frames, videos, and reports were kept below
ignored `build/` output. The validated private Rev1 ROM was used only as a
local ignored input and had SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`; no ROM
or raw ROM-derived artifact was added to the repository.

## Commands and results

The authoritative PowerShell runners were invoked in child processes so that
expected negative cases could not leak their exit code into the parent shell.
The final corrected Season, Team Data, and build runs were performed after
signed checkpoint `20dcf9a`.

| Check | Command/result |
| --- | --- |
| Warning-clean build | `$env:TECMO_SKIP_SHORTCUT='1'; .\build.ps1` — final corrected run exit 0; console and GUI built; warning count 0. Log: `build-warning-clean-final-corrected.log`. |
| Direct asset-pack/season/state checks | `build\tecmo_port.exe --assetpack-test`, `--season-test`, and `--gameplay-state-test` — corrected runs each exit 0. Logs: `assetpack-self-test-corrected.log`, `season-self-test-corrected.log`, `gameplay-state-self-test-corrected.log`. |
| Season suite | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath <validated-local-rev1-rom> -DecompRoot <validated-local-decomp-root> -SkipBuild` — final corrected exit 0; 19 pixel checkpoints and strict persistence/provenance matrix passed. Log: `season-suite-final-corrected.log`. |
| Season repeat | Same Season command in a fresh child — exit 0 with the same pass summary. Log: `season-suite-final-corrected-repeat.log`. |
| Team Data regression | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath <validated-local-rev1-rom> -DecompRoot <validated-local-decomp-root> -SkipBuild` — exit 0; 15 pixel checkpoints passed. Log: `team-data-suite-final-corrected.log`. |
| Full GameplayScene | `.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath <validated-local-rev1-rom> -Build -ProofRootPath <qa-root>\gameplay-scene-proof-final-corrected` — exit 0; full scene, shot, camera, malformed-pack, determinism, and proof checks passed. Log: `gameplay-scene-suite-final-corrected.log`. |
| R2E GameplayPresentation | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplayPresentationTests.ps1 -ProjectRoot . -RomPath <validated-local-rev1-rom> -Build -ProofRootPath <qa-root>\gameplay-presentation-proof-final-corrected` — exit 0; positive two-pass and all six negative framing cases passed. Log: `gameplay-presentation-suite-final-corrected.log`. |
| Win32/flow smoke | With the generated season pack bound in the child process, `Run-Win32LaunchSmokeTest.ps1` and `Run-NativeFlowTests.ps1` — both corrected runs exit 0. Logs: `win32-launch-smoke-corrected.log`, `native-flow-suite-corrected.log`; report: `native-flow-report-corrected.json`. |
| Control-plane read-only checks | `Test-ControlPlane.ps1` and `Verify-Lineage.ps1` — both exit 0 with zero warnings. Logs: `control-plane-test.log`, `control-plane-lineage.log`. |

The first Win32 invocation without an asset-pack binding failed the explicit
developer-flow prerequisite (`ROM-derived settings cursor anchors were not
loaded`). The bounded rerun supplied the generated pack and passed; this was a
setup finding, not a product failure.

## Required persistence and regression matrix

The Season suite passed strict TSAV-1/TSAV-2 acceptance, migration, and
rejection coverage: valid legacy migration and current-format load/save;
wrong version, wrong size, malformed header, reserved-field, checksum,
truncated, trailing-data, and dependency/provenance rejection; new-save
precedence; and transactional rollback after a rejected replacement. It also
passed malformed asset-pack guards. Team Data passed its ROM-only parser,
all-star mapping, input/state transitions, malformed rejection, and 15-pixel
checkpoint regression. The R2E runner passed its GameplayPresentation layup
regression and framing negative cases.

The corrected Season suite reported:

`SEASON TEST PASS: strict ROM-only TSNS provenance/dependencies, TSAV isolation/migration/rejection, native gameplay handoff/result flow, malformed-pack guards, and 19 pixel checkpoints`

The full scene suite reported the TGMO/TGBD/TGFT/TPNL/TGBC/TGVR/TGAI/TGCP,
orientation, two-basket ownership, jump, shot, dunk, audio, halftime/final,
render-hash, repeat, and malformed dependency checks as passing. Its proof
manifest is intentionally `DRAFT`; that status is retained and is not
promoted to emulator-perfect original parity.

## Corrected leader hashes and visual proof

The proof-only seed was corrected to FGA 800, 3PA 500, and FTA 500. The
corrected first and repeat captures matched byte-for-byte. Exact current-QA
paths, timestamps, and hashes—including both capture roots—are recorded in
`docs/finish-tasks/R3-player-stats-leaders/PROOF.md`.

| Mode | Corrected SHA-256 |
| --- | --- |
| category 0 page 0 | `600E13073B9D8509E7E5648E8AFA5221E7E038CB51D28C40AA952E5B4B80C1AB` |
| category 0 page 6 | `F07071A9032AB6CD6B2307ED4C007AE1995B5C8CD4E37A1F205D0890368AAE14` |
| category 0 page 12 | `9C2C058CA7EB355C48ED6533536088A641D7866B16EB57C5CF01410F1FEF4FD1` |
| category 3 | `794DA4AE2CC6FB0B75B1F30A4F682B565F5B16A3DBC26BD0E594ABC9763A182E` |
| category 5 | `74871EF3FFE4EA643CD707A95B29389EC487552AB9B4D7571F3B56A526EB96FE` |
| category 6 | `D2561DF4460C843B85127C1B6D4AA59DBDC0640DCF186562796BAF0FCB5F1FBD` |

The six corrected frames were inspected at original resolution. Names, teams,
titles, labels, metrics, glyphs, and layout were readable; the displayed
percentages no longer exceeded 1.000. These are native-port frames, not
original-reference images.
