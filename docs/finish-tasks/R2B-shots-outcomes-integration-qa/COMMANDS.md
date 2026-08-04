# R2B commands and results

Private research inputs are represented by variables. The ROM and
decompilation tree are test inputs only and are not runtime dependencies or
tracked artifacts.

```powershell
$ProjectRoot = '<R2B_WORKTREE>'
$Rom = '<CANONICAL_REV1_ROM>'
$DecompRoot = '<LOCAL_DECOMP_ROOT>'
$env:TECMO_SKIP_SHORTCUT = '1'
```

The canonical Rev1 iNES ROM was 393,232 bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

## Clean takeover and immutable inputs

Representative read-only commands were:

```powershell
git status --porcelain=v2 --untracked-files=all
git branch --show-current
git rev-parse HEAD main origin/main
git ls-remote origin refs/heads/main
git rev-parse codex/round-2b-shots-outcomes-staging
git merge-base 8a5b9928544a430efa34cbf98a248d6a8cbe7b14 `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git rev-list --reverse `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git diff --name-only `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git diff --name-only `
  222d75cfafa9153db1eb44492bf557f11b1a9091..8a5b9928544a430efa34cbf98a248d6a8cbe7b14
git diff --check `
  222d75cfafa9153db1eb44492bf557f11b1a9091..7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
git merge-tree --write-tree `
  8a5b9928544a430efa34cbf98a248d6a8cbe7b14 `
  7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a
```

Results:

- exact clean branch `codex/r2b-shots-outcomes-integration-qa-sol` at base
  `8a5b9928544a430efa34cbf98a248d6a8cbe7b14`;
- local main, remote-tracking main, and live remote main all exact at that SHA;
- immutable staging exact at `7b9287a...`;
- candidate base exact `222d75cf...` and exactly three candidate commits;
- 17 candidate paths, 80 current-main paths, normalized overlap 0;
- conflict-free reproduced tree `2d918e8d672f991c87c293096e315a8bde5685da`;
- candidate and predicted merge diff checks clean;
- both excluded blobs exact.

Signatures were checked with `git verify-commit` for control, main, all three
candidate commits, and the integration merge. Each applicable commit returned
a Good SSH signature with RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Authorized branch-only merge

After the durable clean-takeover checkpoint, the sole product-bearing Git
mutation in R2B was:

```powershell
git merge --no-ff -S 7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a `
  -m 'merge: reconcile R2 shots outcomes with current main'
git show -s --format='%H %P %T' HEAD
git verify-commit HEAD
```

Result: Good-signed merge
`26e6aaf19b639972cb9043f29fc55daa1efce835`, ordered parents
`8a5b992... 7b9287a...`, tree `2d918e8d...`, and clean branch status.

No rebase, cherry-pick, force update, main mutation, staging mutation, or push
was performed.

## Warning-clean build and focused shot gates

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1
.\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath $Rom
```

Results:

- full `/W4` build exit 0; `tecmo_port.exe` and `tecmo_port_game.exe` built;
  warning/error diagnostic scan count 0;
- TGCS-1 pass: canonical/provenance/reload, 208 poses,
  malformed/missing/cross-pack rejection, and 43 Rev1 mutations;
- TGSR-3 pass: four primary plus four lookup source spans, point values 1/2/3,
  terminal polarity, raw routes A708/A7A9/A8E9/A708, rattle passes 1..4, and
  claimant settlement.

The built executables used for terminal smokes were:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/tecmo_port.exe` | 2,150,912 | `6B08AFB9CC6AFF0F542EC0035A3B3F0C09404A65989E55168EA88FA53CEA1987` |
| `build/tecmo_port_game.exe` | 2,150,912 | `C0FD78A80018834BA721B1E37D1EB6BA89FC3B9E36963847FB8F6B26E3887FBA` |

## Complete gameplay scene and direct state gates

```powershell
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath $Rom `
  -ProofRootPath 'build\r2b-sol-scene-20260804T012159Z'
.\build\tecmo_port.exe --root $ProjectRoot --gameplay-scene-test `
  'build\r2b-sol-scene-20260804T012159Z\asset-pack\gameplay-proof.assetpack'
.\build\tecmo_port.exe --gameplay-state-test
```

Results:

- `GAMEPLAY SCENE TEST PASS` across pack/provenance, HUD, movement, dribble,
  fatigue, penalties, out-of-bounds, backcourt, referee, CPU steering, camera,
  free-throw lineup, orientation, TGDK/TGJS/TGSR jumps/make/miss/rattle/expiry,
  dunk, deterministic/malformed/missing/oversize/cross-pack, and CHR cases;
- direct `GAMEPLAY SCENE SELF TEST PASS`;
- `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`.

The scene proof manifest has SHA-256
`111788F30FC2E834C158E6EDCE38A4CCE3F395194A9FD548E03297921BE6B0EA`.
A separate read-only audit checked all 254 declared artifact paths, byte counts,
and hashes with zero failure. Its `DRAFT` and generic-task metadata are retained
as described in `EVIDENCE.md`.

## Current-main clock, lineup, fatigue, and cross-domain gates

```powershell
.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot . -RomPath $Rom
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-AssetPackTests.ps1 -ProjectRoot . -RomPath $Rom
$env:TECMO_ASSETPACK =
  (Resolve-Path '.\build\asset_pack_test\tecmo_test.assetpack').Path
.\build\tecmo_port.exe --assetpack-test
.\tools\Run-IntroSequenceTests.ps1 -ProjectRoot . -RomPath $Rom
.\build\tecmo_port.exe --root $DecompRoot --flow-test
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot . `
  -DecompRoot $DecompRoot `
  -ReportPath 'build\r2b-sol-native-flow-report.json'
.\tools\Run-MusicTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot . `
  -RomPath $Rom -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath $Rom
.\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-TeamManagementTests.ps1 -ProjectRoot . -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . `
  -DecompRoot $DecompRoot
Remove-Item Env:TECMO_ASSETPACK
```

Results:

- TGFL-1 pass for both orientations, 10 actors, four policies, indices
  517..520, strict dependencies, and 12 source mutations;
- TGFT-1 pass;
- 55/55 broad asset tests pass; built pack has 86 entries, 1,401,618 bytes,
  SHA-256 `CC9A522A1EC5025193FD525419096D5A5AA15AF2F63A9E18E68DB8D81E87AC6F`;
- direct asset-pack self-test pass;
- 29/29 intro report entries pass, with one accepted skipped bounded pixel-mask
  subcase because reference PNGs were unavailable;
- bound direct flow pass: `menu play-intro title start-game-menu preseason
  season quit`;
- native-flow CLI boundary matrix pass;
- music, frontend audio, and gameplay audio pass;
- season, team data, and team management pass;
- Win32 GUI/console subsystem, shortcut, icon, project-root argument, developer
  flow, window lifetime, clean shutdown pass.

Report hashes:

| Report | SHA-256 |
| --- | --- |
| `build/asset_pack_test_report.json` | `41B8584D034E5466058D757A6CFA19A665E4660553C0B8C7A736FE51B7DB3E8B` |
| `build/intro_sequence_test_report.json` | `0687BF25D767A3CAD5AA8D37B5C4BD75976D1929C907C75A13C7740F694F33D3` |
| `build/r2b-sol-native-flow-report.json` | `932334D5AAB9CBA662A63218EB160C1B0A46C637D41B3090744595A362CD96B9` |

## Fresh deterministic shot proof and visual review

An ignored PowerShell proof harness rendered the accepted ordered scenario map
twice:

```powershell
$Cases = [ordered]@{
  'jump-make' = @{ prefix='gameplay-jump-make-frame';
    frames=@(1,5,9,19,20,39,57,63,85,110,111) }
  'jump-miss' = @{ prefix='gameplay-jump-frame';
    frames=@(1,2,4,21,22,39,40,45,46,72,73,74,75,86,87) }
  'jump-rattle' = @{ prefix='gameplay-jump-rattle-frame';
    frames=@(1,4,21,40,72,73,74,77,81,85,88,89,90,91,102,103) }
  'dunk' = @{ prefix='gameplay-dunk-frame';
    frames=@(1,16,32,48,64,75,80,87,132) }
}
.\build\r2b-sol-generate-native-shot-proof.ps1 `
  -OutputRoot 'build\r2b-sol-native-shots-20260804T013200Z-v2'
```

The accepted aggregate formula is SHA-256 over the UTF-8 bytes of ordered
uppercase per-frame SHA-256 strings joined with CRLF and no trailing separator.

| Case | Frames | Aggregate SHA-256 | Fresh FFmpeg 8.1 video SHA-256 |
| --- | ---: | --- | --- |
| jump make | 11 | `47D5B332BCFEFEA472C5CA4FDFCDC9A646FC5CE9E3FD208C6686807B2F74BB99` | `65CE0BD648DB1E458275B61E64A7E8D52F0472D842182B58BF50FB219A93E0B3` |
| jump miss | 15 | `B1E87ECF18121FE57348C46326C1C54A5657FB6633362A36AEAB852E030B9AEF` | `DF3000FDD81BDFE68620B2773EE4C5DD6E2E56E0F6C07563385A664533CEB7AC` |
| jump rattle | 16 | `9447A2693D285FE702B3C7D67CF7D554C4962CB3B7122F0845FE633C8230AF5A` | `7F87E6C5E4200C8C966D4ECCF3A7EEBFFC27C3609330EE1FF8C75CD86FBA4426` |
| dunk | 9 | `F2C128049E5E28BC9750F6A55011FE2B7065D97D9CFE18B329C43DCEA588CEFD` | `F2B60D386E169B62484B9ED7B15BAA8641D936382D8FEFE6C79593586C4884E3` |

Every aggregate matches accepted R2. Each fresh MP4 is byte-equal across the
two passes. The historical MP4 hashes differ because the installed FFmpeg 8.1
container output differs; frame content does not.

The manifest SHA-256 is
`96C260D23B27559C9B0907264F80EDCEE56E75A2526BF87ABC356FE82CF884CB`.
An independent second inventory audit found exactly 119 files total:
102 frames, eight sheets, eight MP4s, and the manifest; escaped 0, missing 0,
unlisted 0, bad hash 0, bad dimension 0.

Sol opened at original detail:

- `pass1/jump-make/jump-make-contact-sheet.png`;
- `pass1/jump-miss/jump-miss-contact-sheet.png`;
- `pass1/jump-rattle/jump-rattle-contact-sheet.png`;
- `pass1/dunk/dunk-contact-sheet.png`;
- `pass1/dunk/frame-0005.png`.

Visual result: P0=0, P1=0, P2=0. Make, miss, rattle, and dunk progression is
coherent; HUD and sprites are readable; the dunk black frame is intentional.

## Harness and retry record

The complete non-product diagnostic ledger is in `FAULT-LEDGER.md`. The two
runtime-facing retries were:

1. an unbound direct `--flow-test` stopped at settings-cursor setup; the same
   command passed after binding the validated pack through `TECMO_ASSETPACK`;
2. a wrapper inspected stale `$LASTEXITCODE=1` after TGFT had printed PASS; an
   isolated `powershell.exe -File` run returned process exit 0.

Neither retry exposed a product regression.

## Signed-document and terminal checks

The scoped documentation sequence uses:

```powershell
git diff --check
git status --short --branch
git add -- docs/finish-tasks/R2B-shots-outcomes-integration-qa
git diff --cached --check
git diff --cached --name-only
git commit -S -m 'docs: record R2B shots outcomes integration QA'
git verify-commit HEAD
git diff --check 26e6aaf19b639972cb9043f29fc55daa1efce835..HEAD
git merge-base --is-ancestor 8a5b9928544a430efa34cbf98a248d6a8cbe7b14 HEAD
git merge-base --is-ancestor 7b9287a91fcd9d4725d5d05c37b1d667d9bfb57a HEAD
git status --porcelain=v2 --untracked-files=all
git rev-parse main origin/main
git ls-remote origin refs/heads/main
```

The first signed docs candidate is then sent to the same pinned Luna for a
read-only signed-tip review. Any documentation revision stays in this folder
and is Good-signed. The exact terminal SHA and final guarded handoff are sent
to master because a commit cannot contain its own object ID.
