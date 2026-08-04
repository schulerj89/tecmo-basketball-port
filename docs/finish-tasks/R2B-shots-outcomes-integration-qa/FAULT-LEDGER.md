# R2B fault and retry ledger

This ledger separates orchestration, shell, proof-tool, and configuration
diagnostics from accepted product results. None of the entries below changed a
candidate/product path, main, staging, `origin/main`, live remote main, or a
remote ref.

## Ledger

| ID | Observation | Classification and effect | Corrective action / terminal result |
| --- | --- | --- | --- |
| R2B-F01 | The first projectless reviewer creation call included an unsupported extra top-level `type` field and was rejected by schema validation. | Pre-creation orchestration request; no task, output, pin, registry entry, or Git change was created. | Registry/app collision checks still found zero reviewer tasks. One corrected request created sole task `019fca5b-3b84-7a82-b2ab-588c50a4b7fd`, which was pinned and reused. |
| R2B-F02 | A combined clock/lineup/fatigue wrapper saw `$LASTEXITCODE=1` after TGFT printed `TGFT-1 fatigue tests passed.` | Wrapper bug: an expected negative native subprocess inside the PowerShell test left a stale native exit code. The test script itself did not throw. No product failure. | TGFT was rerun as an isolated `powershell.exe -File` process; it printed PASS and returned process exit 0. |
| R2B-F03 | Direct `--root <DECOMP> --flow-test` without `TECMO_ASSETPACK` stopped with `ROM-derived settings cursor anchors were not loaded`. | Missing semantic-pack binding at setup; no gameplay state executed and no product mutation occurred. | The freshly validated 86-entry pack was bound through `TECMO_ASSETPACK`. Direct flow, native-flow CLI boundaries, and Win32 developer flow all passed. |
| R2B-F04 | The first ignored native-shot proof run matched the accepted jump-make frame aggregate, then treated the historical MP4 hash difference as fatal. | Proof-harness policy was too strict for installed FFmpeg 8.1 container output. A partial ignored root `build/r2b-sol-native-shots-20260804T013200Z` was not accepted as evidence. | The ignored harness was revised to preserve historical-video mismatch as metadata while requiring accepted frame aggregates plus fresh pass-to-pass frame/video equality. Fresh `-v2` proof passed all four scenarios. |
| R2B-F05 | One `apply_patch` update to the ignored proof harness failed because its expected backslash context did not exactly match. | Patch verification failure before mutation; no file content changed in that call. | Smaller exact-context patches applied, PowerShell parser reported zero errors, and the v2 proof passed. |
| R2B-F06 | One read-only branch/main audit one-liner had a missing PowerShell closing parenthesis. | Parser error before any Git command in that invocation ran; no state change. | A simpler multiline audit ran successfully and confirmed clean branch, unchanged three main observations, and main ancestry. |
| R2B-F07 | `git verify-commit` writes the Good-signature line to stderr, which Windows PowerShell displayed as a `NativeCommandError` record despite successful verification. | PowerShell stream presentation only; `%G?` was `G` and `git verify-commit` succeeded. | Signature results are reported from Git status/exit and exact key fingerprint, not PowerShell's stderr formatting. |
| R2B-F08 | A first report-count probe treated report JSON roots as arrays and printed blank counts. | Read-only report-inspection shape assumption; report contents and files were unchanged. | The corrected probe used each root object's `.tests` collection: asset 55/55, intro 29/29 with one skip flag, native flow 1/1. |

## Encoder variance detail

The four accepted ordered frame aggregates match exactly in both R2B passes.
Fresh MP4s are byte-equal between pass 1 and pass 2 for every scenario. The
installed FFmpeg 8.1 hashes are:

| Scenario | Fresh video SHA-256 |
| --- | --- |
| jump make | `65CE0BD648DB1E458275B61E64A7E8D52F0472D842182B58BF50FB219A93E0B3` |
| jump miss | `DF3000FDD81BDFE68620B2773EE4C5DD6E2E56E0F6C07563385A664533CEB7AC` |
| jump rattle | `7F87E6C5E4200C8C966D4ECCF3A7EEBFFC27C3609330EE1FF8C75CD86FBA4426` |
| dunk | `F2B60D386E169B62484B9ED7B15BAA8641D936382D8FEFE6C79593586C4884E3` |

Because the 102 decoded source PNGs, contact sheets, scenario aggregates, and
fresh video pairs are equal, both Sol and independent Luna classified the
historical container-hash difference as toolchain variance, not a source,
state, rendering, or determinism defect.

## No unclosed product fault

There was no merge conflict, signature failure, candidate-path collision,
scope escape, source/ASM contradiction, warning-clean build failure, focused
gate failure, independent P0/P1/P2 finding, proprietary tracked artifact,
destructive command, main/staging mutation, or push.

The accepted partial/incomplete product semantics are cataloged in
`README.md` and `EVIDENCE.md`; they are not harness faults and were not silently
closed.
