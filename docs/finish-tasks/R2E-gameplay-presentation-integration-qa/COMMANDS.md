# Command and fault ledger

Local proprietary paths are represented as `<LOCAL_REV1_ROM.nes>` and
`<LOCAL_DECOMP_ROOT>`. No ROM, decompilation, capture, or other proprietary
payload is tracked.

## Read-only control and takeover gates

The takeover, rescope, corrective, and closing gates used `git status`,
`git worktree list --porcelain`, `git branch`, `git rev-parse`, `git cat-file`,
`git merge-base --is-ancestor`, `git diff --name-status`, `git diff --numstat`,
`git diff --check`, `git verify-commit`, `git show --show-signature`,
`git ls-remote origin refs/heads/main`, registry inspection, path-ownership
checks, PowerShell AST parsing, ROM hashing, proof-inventory hashing, and
proprietary-extension scans. Corrected gates exited 0 and matched
`LINEAGE.md`.

Canonical private Rev1 identity:

- bytes: `393232`
- SHA-256: `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

The only prescribed integration action was:

```powershell
git merge --ff-only d8d811918932c19bbe1741d2392ec1ad942ebd79
```

It ran on the assigned clean branch only and produced no merge commit. The
runner correction was then inspected with `git diff`, `git diff --check`, and
PowerShell AST parsing before its signed commit.

## Builds

All corrected build commands exited 0. Compiler/linker logs had zero warning,
error, fatal-error, or linker-error diagnostic matches.

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1

& '<VS_CMAKE>' -S . -B .\build\cmake-r2e-corrective-a4c1286 -G 'Visual Studio 17 2022' -A x64
& '<VS_CMAKE>' --build .\build\cmake-r2e-corrective-a4c1286 --config Release --target tecmo_port
& '<VS_CMAKE>' --build .\build\cmake-r2e-corrective-a4c1286 --config Release --target tecmo_port_game
```

Canonical executable SHA-256 values after the correction:

- `build/tecmo_port.exe`: `1C9C16904BD76BBA16AED7CA5CB0A36679EA2FAD44B02FE0EA029F891EAAD78F`
- `build/tecmo_port_game.exe`: `E78A6BB95394EB6725CE3F6C49C4DA6420EF146619CA0B30EDCB3FDB9FFBC177`

Fresh CMake Release executable SHA-256 values:

- console: `94F4A9B25B132138C057C25F8AE9AC8248DF4332EAE8EEC9491FCFD6F874C7B2`
- GUI: `B3386238CEAF235CEC4BEDDE6074DD108A32AED484801165A4E28AC282FF20FA`

## Focused proof and parser negatives

The corrected runner owned a unique GUID-suffixed scratch directory under
`build`, created it without `-Force`, and removed only that exact invocation's
directory after revalidating resolved containment.

```powershell
.\tools\Run-GameplayPresentationTests.ps1 `
  -ProjectRoot . `
  -RomPath <LOCAL_REV1_ROM.nes> `
  -ProofRootPath build\gameplay-layup-proof-r2e-corrective-a4c1286 `
  -Build
```

The command exited 0 and covered pass-one/pass-two frames 1-17 plus these six
transactional negatives: frame 0, frame 18 with a preseeded sentinel, missing
suffix, `+1`, trailing `1x`, and leading-zero `01`. Pre/post inventory found no
fixed or GUID scratch residue.

## Full scene and affected suites

Every corrected runner below exited 0:

```powershell
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -ProofRootPath build\gameplay-scene-proof-r2e-corrective-build-a4c1286 -Build
.\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayDunkCutawayTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayCameraProjectionTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayCourtTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayCourtOrientationTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayViolationRefereeTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayPenaltyTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-GameplayAssetTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
```

The direct console scene printed `GAMEPLAY SCENE SELF TEST PASS`; deterministic
state replay printed `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`.
Jump-shot, court-viewport, and HUD coverage used their existing direct
executable interfaces and passed; this repository has no separate runner with
those three names.

## Cross-domain and current-main smokes

Every corrected command or child process below exited 0:

```powershell
.\tools\Run-AssetPackTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -AssetPackPath build\asset_pack_test_a4c1286.assetpack -ReportPath build\asset_pack_test_report_a4c1286.json
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-MusicTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -Build
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -Build
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot . -DecompRoot <LOCAL_DECOMP_ROOT> -ReportPath build\native_flow_test_report_a4c1286.json
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot <LOCAL_DECOMP_ROOT>
.\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -SkipBuild
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -SkipBuild
.\tools\Run-TeamManagementTests.ps1 -ProjectRoot . -RomPath <LOCAL_REV1_ROM.nes> -DecompRoot <LOCAL_DECOMP_ROOT> -SkipBuild

$env:TECMO_ASSETPACK = '<ABSOLUTE_ROM_ONLY_PACK>'
.\build\tecmo_port.exe --root <LOCAL_DECOMP_ROOT> --flow-test
```

Direct current-main executable routes also exited 0 for asset-pack, gameplay
assets, close shots, dunk cutaway, jump shots, shot resolution, camera, court,
viewport, orientation, HUD, violation/referee, penalties, gameplay state,
gameplay scene, gameplay/frontend audio, music, team management, season,
controls, bank07, and video. The video route reported FCEUX FNV `9F872B25`.

Report hashes:

- asset-pack report: `7A3CA10FC99F529C3B136DF5953B3F98EF27D00B584072897A66213F97D34176`
- NativeFlow report: `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9`

## Proof, media, and visual commands

Proof audits used `ConvertFrom-Json`, `Get-FileHash -Algorithm SHA256`, exact
relative-path inventory maps, decoded-frame records, `ffprobe`, and
original-detail image inspection. Sol viewed all 17 corrective pass-one frames
at native 640x480 through individual files and the 2560x2400 sheet, plus the
fresh 1920x1440 scene sheet and relevant accepted historical sheets/media.
No derived visual or proprietary payload is tracked.

## Fault and limitation ledger

No listed fault mutated an unauthorized tracked path, index, protected ref,
main, staging, origin, or remote state.

1. Bare `cmake` was unavailable on `PATH`; bundled Visual Studio CMake passed
   the same fresh configure/build gate.
2. Two early `rg` queries used an unsupported flag and then an invalid regular
   expression. Corrected read-only queries passed.
3. The first direct-scene wrapper did not surface its background session ID;
   the same child was rerun/polled and passed.
4. A GameplayAsset wrapper inspected stale `$LASTEXITCODE` after an intentional
   negative case and reported a false negative. A clean subprocess rerun passed.
5. The accepted focused proof was absent from the new Sol worktree; the exact
   report-bound proof was located in the retained implementation-worker
   worktree and rehashed read-only.
6. A scene query first requested `manifest.json`; the actual file is
   `PROOF-MANIFEST.json`. The corrected query passed.
7. Several read-only PowerShell `foreach`/pipeline, nested link-check, or
   empty-pipe expressions used the wrong pipeline object or had parser errors.
   Corrected grouping/captured-parent/query forms passed.
8. One contact query requested `contact-sheet.png`; the proof-owned name is
   `gameplay-layup-contact-sheet.png`. The corrected file matched its hash.
9. One orchestration wrapper had a JavaScript parse error caused by an embedded
   PowerShell backtick; the corrected wrapper ran without mutation.
10. `list_threads` rejected limit 200 because the maximum is 50; limit 50
    returned the registry.
11. Task creation returned JSON-encoded text, so the same-script pin extractor
    did not execute. The one already-created Luna ID was recovered and pinned
    immediately in a separate call; no second task, retry, or replacement was
    created.
12. Luna's initial direct flow calls without a decomp root and with the target
    repository root exited 1. The isolated projectless task lacks the external
    decomp/baseline root. Sol's exact-root direct flow and NativeFlow passed.
13. The first Option-A signature gate falsely threw because PowerShell applied
    `-notmatch` to an output array. Joining the output produced the correct Good
    signature result.
14. A corrective proof-hash query contained an empty pipeline and failed to
    parse. The corrected read-only hash query passed.
15. The first fresh corrective scene proof intentionally omitted `-Build`, but
    its audit helper incorrectly expected `build_warning_clean=true`. That
    valid proof was retained; a second fresh proof with `-Build` passed and is
    the primary evidence.
16. A multi-image viewer response showed display-layer deltas for alternating
    frames, and one first lookup omitted zero padding. Single-file original
    views plus the full-resolution sheet confirmed complete, coherent images.
17. Git range/diff/revspec display queries used ambiguous PowerShell
    range/path syntax or an unquoted `^{tree}` suffix and printed usage, omitted
    the patch, or transformed the revspec. Explicit range strings, quoted
    revspecs, and revision-before-`--` syntax returned the exact ledger.
18. Good SSH diagnostics written on stderr appeared as PowerShell
    `NativeCommandError` presentation records although `git verify-commit`
    exited 0 and reported the expected Good signature.
19. Luna's first corrective-report message transport rejected a formatting
    delimiter. The same pinned task resent identical content as plain text;
    repository and task state were unchanged.

The full scene manifest remains `DRAFT` solely because the original-reference
manifest is not locally available and `-RequirePass` was not asserted. That is
an evidence limitation, not a candidate failure. No broad parity or performance
claim is inferred.
