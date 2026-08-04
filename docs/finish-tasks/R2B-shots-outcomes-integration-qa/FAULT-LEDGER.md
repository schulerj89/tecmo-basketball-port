# R2B fault and retry ledger

This ledger separates orchestration, shell, proof-tool, inspection, and
configuration diagnostics from accepted product results. None changed a
candidate/product path, main, staging, `origin/main`, live remote main, or a
remote ref.

## Ledger

| ID | Observation | Classification and effect | Corrective action / terminal result |
| --- | --- | --- | --- |
| R2B-F01 | The first projectless reviewer creation request included unsupported top-level field `type` and was rejected. | Pre-creation schema validation; no task, output, pin, registry entry, or Git change existed. | Collision checks still found zero reviewer tasks. One corrected request created sole pinned task `019fca5b-3b84-7a82-b2ab-588c50a4b7fd`, which is reused. |
| R2B-F02 | An original clock/lineup/fatigue wrapper saw `$LASTEXITCODE=1` after TGFT printed PASS. | An expected negative native subprocess left stale process state; the PowerShell test did not throw. No product failure. | Isolated `powershell.exe -File` rerun printed PASS and returned exit 0. |
| R2B-F03 | Original direct flow without `TECMO_ASSETPACK` stopped because settings cursor anchors were unavailable. | Missing validated semantic-pack binding at setup; no gameplay state or product mutation. | Binding the 86-entry pack made direct flow, native flow, and Win32 developer flow pass. |
| R2B-F04 | The first ignored shot-proof run matched the accepted make aggregate, then treated historical MP4 hash variance as fatal. | Proof policy was too strict for FFmpeg 8.1 container output; the partial ignored root was not accepted. | Harness now records historical-video variance while requiring exact accepted PNG aggregates and fresh pass equality. The accepted v2 and reconciled proofs pass. |
| R2B-F05 | Two `apply_patch` calls against the ignored proof harness used backslash context that did not match exactly and failed verification. | Patch verification stopped before mutation in both calls. | Smaller exact-context patches applied. The ignored harness parsed and generated complete passing proofs. |
| R2B-F06 | One original read-only branch/main audit one-liner lacked a closing PowerShell parenthesis. | Parser error before Git commands in that invocation; no state change. | A simpler multiline audit confirmed the clean branch, main observations, and ancestry. |
| R2B-F07 | `git verify-commit` writes Good-signature text to stderr, which Windows PowerShell can display as `NativeCommandError`. | Stream presentation only; Git exit was successful and `%G?` was `G`. | Signatures are reported from Git status/exit and the exact key fingerprint. |
| R2B-F08 | Initial report-count probes treated JSON root objects as arrays and printed incomplete counts. | Read-only report-shape assumption; reports were unchanged. | Corrected probes used `.tests`: asset 55/55, intro 29/29 with one skip flag, native flow 1/1. |
| R2B-F09 | After reconciled native-flow printed every boundary PASS, its caller read `$LASTEXITCODE=1`. | The script intentionally runs negative CLI vectors and leaves their native exit code stale despite completing successfully. | A fresh child PowerShell process was used as the authoritative process gate. |
| R2B-F10 | The first isolated native-flow child did not inherit `TECMO_ASSETPACK` and failed its numeric-render boundary. | Harness setup omission; missing-pack behavior failed closed as designed. | The validated reconciled pack was bound before child creation. All boundaries passed and the child returned process exit 0. |
| R2B-F11 | The first reconciled Win32 run removed the pack binding before its explicit developer-flow subcheck. | Harness ordering mistake; GUI/shortcut setup began, then the expected missing-anchor failure stopped the subcheck. | Keeping the reconciled pack bound for the whole script produced a full Win32 PASS. |
| R2B-F12 | The first shot-proof inventory auditor compared forward-slash manifest paths with unnormalized Windows backslashes and falsely reported 118 unlisted/missing paths. | Read-only auditor normalization bug. All path-local hash, dimension, aggregate, head/tree, and pack checks in that same run had passed. | Corrected char-level separator normalization produced 119 listed files, zero unlisted/missing, and zero errors. |
| R2B-F13 | A first scene-manifest shape probe queried nonexistent key `.artifacts` instead of `.artifact_inventory`. | Read-only inspection-key error; no artifact or manifest changed. | The corrected audit used all 254 inventory records and found zero containment, duplicate, missing, byte-count, hash, or unlisted failures. |

## Encoder variance detail

All four accepted ordered frame aggregates match exactly in both reconciled
passes. Fresh MP4s are byte-equal between pass 1 and pass 2 and exactly match
the earlier FFmpeg 8.1 fresh hashes:

| Scenario | Fresh video SHA-256 |
| --- | --- |
| jump make | `65CE0BD648DB1E458275B61E64A7E8D52F0472D842182B58BF50FB219A93E0B3` |
| jump miss | `DF3000FDD81BDFE68620B2773EE4C5DD6E2E56E0F6C07563385A664533CEB7AC` |
| jump rattle | `7F87E6C5E4200C8C966D4ECCF3A7EEBFFC27C3609330EE1FF8C75CD86FBA4426` |
| dunk | `F2B60D386E169B62484B9ED7B15BAA8641D936382D8FEFE6C79593586C4884E3` |

The 102 selected PNGs, contact sheets, accepted aggregates, and fresh video
pairs are deterministic. Historical container-hash differences are toolchain
variance, not source, state, rendering, or determinism defects.

## No unclosed product fault

There was no merge conflict, signature failure, path collision, scope escape,
source/ASM contradiction, warning-clean build failure, focused or broad gate
failure, personal P0/P1/P2 finding, proprietary tracked artifact, destructive
command, main/staging/origin mutation, or push.

Accepted incomplete semantics are cataloged in `README.md` and `EVIDENCE.md`;
they are not harness faults and were not silently closed.
