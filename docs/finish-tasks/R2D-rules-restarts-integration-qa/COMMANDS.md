# Command and fault ledger

Local proprietary paths are represented as `<LOCAL_REV1_ROM.nes>` and `<LOCAL_DECOMP_ROOT>`. No ROM bytes or derived proprietary dump are tracked.

## Read-only gates

The takeover and closing gates used `git status`, `git worktree list --porcelain`, `git branch -vv`, `git rev-parse`, `git cat-file`, `git merge-base --is-ancestor`, `git diff --numstat`, `git diff --check`, `git verify-commit --raw`, `git ls-remote origin refs/heads/main`, registry JSON parsing, filesystem ownership checks, ROM hashing, process checks, and ROM-extension scans. All corrected terminal gates exited 0 and matched the identities in `LINEAGE.md`.

Canonical ROM identity:

- size: `393232`
- SHA-256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

## Builds

All of these exited 0. Console output contained no compiler warning, compiler error, fatal error, or linker error diagnostic.

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1

& '<VS_CMAKE>' -S . -B .\build\R2D-Sol-cmake-001 -G 'Visual Studio 17 2022' -A x64
& '<VS_CMAKE>' --build .\build\R2D-Sol-cmake-001 --config Release --target tecmo_port
& '<VS_CMAKE>' --build .\build\R2D-Sol-cmake-001 --config Release --target tecmo_port_game
```

The canonical build produced `build/tecmo_port.exe` and `build/tecmo_port_game.exe`. The fresh CMake tree produced both Release targets. The full scene manifest also recorded `build_warning_clean=true`.

## Focused and audio suites

Every corrected command below exited 0:

```powershell
.\tools\Run-GameplayPenaltyTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayViolationRefereeTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayBackcourtTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayCourtOrientationTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayCameraProjectionTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -Build
.\tools\Run-MusicTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayAssetTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-AssetPackTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -AssetPackPath build\R2D-Sol-assetpack-001\tecmo.assetpack -ReportPath build\R2D-Sol-assetpack-001\report.json
```

Pass labels were TPNL-1, TGVR-1, TGBC-1, TGOR-1, TGCP-2, TSFX-1/TDMC-1, TFSX-1, TMUS-1, TGPL-1, and the complete 86-entry asset-pack gate.

## GameplayScene, state, flow, and current-main smokes

Every corrected command or process below exited 0:

```powershell
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -ProofRootPath build\R2D-Sol-scene-proof-001 -Build
.\build\tecmo_port.exe --root . --gameplay-scene-test build\R2D-Sol-scene-proof-001\asset-pack\gameplay-proof.assetpack
Start-Process .\build\tecmo_port_game.exe -ArgumentList @('--root','.', '--gameplay-scene-test','<ABSOLUTE_PROOF_PACK>') -WorkingDirectory . -WindowStyle Hidden -PassThru -Wait

$env:TECMO_ASSETPACK = '<ABSOLUTE_ROM_ONLY_PACK>'
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot . -DecompRoot <LOCAL_DECOMP_ROOT> -ReportPath build\R2D-Sol-native-flow-002.json
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot <LOCAL_DECOMP_ROOT>

.\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -AssetPackPath '<ABSOLUTE_ROM_ONLY_PACK>' -SkipBuild
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -SkipBuild
.\tools\Run-TeamManagementTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -SkipBuild
```

The direct executable matrix also exited 0 for `--assetpack-test`, `--music-test`, `--frontend-audio-test`, `--gameplay-audio-test`, `--team-management-test`, `--season-test`, `--controls-test`, and `--video-test` with the verified ROM-only pack in `TECMO_ASSETPACK`.

Two corrected `--gameplay-state-test` runs both exited 0, printed `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`, were byte-for-byte equal as captured text, and each hashed to `5C5EE00C87080AF61B6A5803756514168752BFC4AD9419D9266FB38080966857` as UTF-8.

The direct console scene printed `GAMEPLAY SCENE SELF TEST PASS`. The hidden GUI scene process exited 0. NativeFlow passed the active flow and all 15 CLI boundary cases. Win32 launch passed hidden shortcut/GUI and console subsystem, root, working-directory, icon, roster-independent startup, lifetime, and clean-shutdown checks.

## Determinism and visual commands

The manifest and artifact audit used `ConvertFrom-Json`, `Get-FileHash -Algorithm SHA256`, a normalized repeat-1/repeat-2 relative-name map, and original-detail image inspection. It found 25 normalized repeat pairs, no missing pair, and zero mismatch.

Nine direct named render modes exited 0 and were preserved under `build/R2D-Sol-visual-proof-001`:

- `gameplay-live-start`
- `gameplay-out-of-bounds-frame23`
- `gameplay-out-of-bounds-frame31`
- `gameplay-out-of-bounds-frame39`
- `gameplay-shot-clock-violation-frame0`
- `gameplay-shot-clock-violation-frame16`
- `gameplay-shot-clock-violation-frame80`
- `gameplay-backcourt-frame27`
- `gameplay-backcourt-frame167`

## Fault and diagnostic ledger

No fault mutated source, tracked paths, refs, staging, main, or the remote.

1. The first Sol registry wrapper had a JavaScript parse error before PowerShell execution. The corrected wrapper parsed the signed registry and passed.
2. A read-only PowerShell guard left `HEAD^{tree}` unquoted; PowerShell transformed the revspec and `git rev-parse` exited 1 before any mutation. Quoting the revspec passed.
3. The first Sol NativeFlow wrapper shell did not inherit `TECMO_ASSETPACK`; the `numeric-render-suffix` child exited 1. Re-running with the verified ROM-only pack in the same process passed the active flow and all 15 CLI boundaries.
4. The first gameplay-state comparison used unavailable `[Convert]::ToHexString`; both child tests had already exited 0 and matched, but the helper hash fields were null. The compatible `BitConverter` rerun passed with equal hashes.
5. The first visual copy helper expected `build/test.png`; the successful renderer reported and wrote `build/play_test.png`, so the helper exited 1 after creating only an empty unique proof directory. The corrected helper reused that directory without cleanup and preserved all nine images.
6. An overbroad proprietary-output scan classified the two standard CMake compiler-ABI `.bin` probes as ROM-like. The refined actual-ROM-extension scan found zero ROM file beneath `build`.
7. Luna's first frontend-audio call used the parent disassembly directory and exited 1 because lifted files are below the decompilation subroot; the corrected strict subroot passed.
8. Luna's direct flow call without `TECMO_ASSETPACK` exited 1 on the expected missing ROM-derived cursor anchors; the generated full ROM-only pack passed.
9. Luna had one outer PowerShell parser typo before child execution; the corrected wrapper passed. Unsupported render checkpoints at frames 168 and later exited 1 as the intentional bounded mode contract, not a product failure.
10. Luna's second contact-sheet viewer preview was anomalous, but byte identity, SHA-256, decoded dimensions, and the first full-resolution inspection proved a display-layer preview issue.

All corrected acceptance commands passed. These diagnostics produce no P0/P1/P2/P3 finding.
