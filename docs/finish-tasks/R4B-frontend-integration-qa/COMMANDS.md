# R4B commands and results

Private local inputs are represented by placeholders:

```powershell
$Rom = '<CANONICAL_REV1_ROM>'
$DecompRoot = '<LOCAL_DECOMP_ROOT>'
$env:TECMO_SKIP_SHORTCUT = '1'
```

The canonical ROM was 393,232 bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
No ROM byte or private input path is committed.

## Pre-mutation and merge checks

Representative exact checks:

```powershell
git status --short --branch
git rev-parse HEAD
git rev-parse main
git rev-parse origin/main
git ls-remote origin refs/heads/main
git rev-parse codex/round-4b-frontend-intro-title-staging
git merge-base 819b0e5eabca11683786e45474ca60329dff7f5f `
  757283edba5f87c2998b16e06bd1831e54ba04b5
git merge-tree --write-tree --messages `
  819b0e5eabca11683786e45474ca60329dff7f5f `
  757283edba5f87c2998b16e06bd1831e54ba04b5
git diff --check 6d8f9c7a99a7ce188f1a523247d3a9b9093860fb..819b0e5
git diff --check 6d8f9c7a99a7ce188f1a523247d3a9b9093860fb..757283e
git verify-commit 819b0e5eabca11683786e45474ca60329dff7f5f
git verify-commit 757283edba5f87c2998b16e06bd1831e54ba04b5
```

Results: clean exact branch/base; local/origin/live main all `819b0e5...`;
staging exact `757283e...`; merge-base `6d8f9c7...`; predicted tree
`4b9879f...`; both `diff --check` calls pass. Base verifies. Candidate
verification returns `1` because it is unsigned, as explicitly accepted by
master.

The deliberate merge and verification were:

```powershell
git merge --no-ff -S 757283edba5f87c2998b16e06bd1831e54ba04b5 `
  -m 'Merge accepted R4 frontend into R4B integration QA'
git verify-commit 6b5d43546128408de8ab246d22f1b48322714183
git merge-base --is-ancestor 819b0e5eabca11683786e45474ca60329dff7f5f `
  6b5d43546128408de8ab246d22f1b48322714183
git merge-base --is-ancestor 757283edba5f87c2998b16e06bd1831e54ba04b5 `
  6b5d43546128408de8ab246d22f1b48322714183
```

Results: signed non-ff merge `6b5d435...`; ordered parents `819b0e5...` and
`757283e...`; tree `4b9879f...`; verification and both ancestry checks exit
`0`; worktree clean.

## Warning-clean build and frontend suite

```powershell
.\build.ps1
.\tools\Run-IntroSequenceTests.ps1 -ProjectRoot '.' -RomPath $Rom -Build
```

Results:

- MSVC C11 `/W4` build: exit `0`, warnings `0`, both executables produced.
- Canonical suite: exit `0`, `29` tests, `1` intentional optional bounded
  reference skip, `0` failures.
- Sanitized report:
  `build/intro_sequence_test_report.json`, 272,405 bytes, SHA-256
  `161C3005206167A5C5D83799B640D01CCA5A3195159508D27109E3C3E73E0584`.
- Combined ROM-only pack:
  `build/intro_sequence/tecmo_intro_sequence_test.assetpack`, 1,401,618
  bytes, SHA-256
  `CC9A522A1EC5025193FD525419096D5A5AA15AF2F63A9E18E68DB8D81E87AC6F`.

## Direct frontend/flow checks

```powershell
$env:TECMO_ASSETPACK =
  (Resolve-Path 'build\intro_sequence\tecmo_intro_sequence_test.assetpack').Path
.\build\tecmo_port.exe --assetpack-test
.\build\tecmo_port.exe --arena-scene-test
.\build\tecmo_port.exe --root $DecompRoot --flow-test
```

All exit `0`:

```text
Asset pack self-test passed.
ARENA INTRO SCENE SELF TEST PASS
FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit
```

## Cross-domain gates

The combined pack remained configured for flow/launch tests that exercise the
frontend. Each script ran in a child PowerShell process so expected negative
native-command exit codes could not leak through `$LASTEXITCODE`.

```powershell
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot '.' -RomPath $Rom
.\tools\Run-GameplayMovementTests.ps1 -ProjectRoot '.' -RomPath $Rom
.\tools\Run-NativeFlowTests.ps1 -ProjectRoot '.' `
  -DecompRoot $DecompRoot -ReportPath 'build\native_flow_r4b_report.json'
.\tools\Run-SeasonTests.ps1 -ProjectRoot '.' -RomPath $Rom `
  -DecompRoot $DecompRoot -SkipBuild
.\tools\Run-MusicTests.ps1 -ProjectRoot '.' -RomPath $Rom
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot '.' -RomPath $Rom `
  -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot '.' -RomPath $Rom
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot '.' -RomPath $Rom `
  -ProofRootPath 'build\proof\r4b-live-smoke-6b5d435'
.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot '.' `
  -DecompRoot $DecompRoot -StartupTimeoutSeconds 10 -AliveMilliseconds 1000
```

Results: all exit `0`. TGAI-1 CPU steering, TGMO-1 movement, native flow and
all CLI boundaries, TSNS/TSAV season, TMUS, TFSX, TSFX/TDMC, gameplay-scene
LIVE integration, PE/shortcut/GUI lifetime, and clean shutdown pass. The LIVE
proof manifest truthfully remains `DRAFT`: this branch is not the dedicated R1
proof branch, `-RequirePass` was not claimed, and no original-reference
manifest was supplied. It is an integration smoke gate only.

## Malformed/corrupt negatives

The canonical suite personally exercised:

- `40/40` finale missing/malformed cases, including exact corrupted
  `chr/all` fingerprint rejection with exit `1`, no output PNG, and diagnostic
  `intro-finale-render-source finale=0 chr=1 schema=TFIN-1`;
- `8/8` TFIN semantic-header round trips without screen-payload mutation;
- `7/7` TASG structural negatives;
- `1/1` TCLP CHR-offset negative;
- `2/2` BUCKS/PASS CHR-offset negatives;
- strict malformed local clean-mode suffix rejection;
- `9/9` production parser/bound cases, including N=`4096` accepted and
  N=`4097`, missing/alpha/trailing/signed/overflow suffixes rejected without
  output.

No negative generated a committed artifact.

## Complete production replay

The same combined pack drove a fresh process for every N in `0..3151`:

```powershell
$env:TECMO_ASSETPACK =
  (Resolve-Path 'build\intro_sequence\tecmo_intro_sequence_test.assetpack').Path
0..3151 | ForEach-Object {
    $n = $_
    $out = 'build\proof\r4b-frontend-integration-6b5d435\frames\frame-{0:D4}.png' -f $n
    .\build\tecmo_port.exe --root $DecompRoot `
      --render-test-mode ('intro-production-clean-frame' + $n) $out
}
```

The harness required one exact sanitized `intro-production-state` line per
process, checked `global=N`, hashed every PNG, and validated contiguous names,
counts, headers, dimensions, bit depth, color type, and uniform byte length.
It then reran 16 representative processes twice, including N=`4096`.

The two video encodes used the accepted deterministic command shape:

```powershell
ffmpeg -framerate 60 -start_number 0 `
  -i build/proof/r4b-frontend-integration-6b5d435/frames/frame-%04d.png `
  -frames:v 3152 -c:v libx264 -preset veryslow -qp 0 -pix_fmt yuv444p `
  -threads 1 -x264-params threads=1:lookahead-threads=1:sliced-threads=0 `
  -fflags +bitexact -flags:v +bitexact -map_metadata -1 -map_chapters -1 `
  -metadata 'title=Tecmo intro production replay' `
  -metadata 'comment=Native deterministic 60fps proof' -an <OUTPUT_MP4>
```

Both outputs are byte-identical and match the accepted R4 video hash. Fresh
`ffprobe -count_frames` validated the stream. The encoded sequence was compared
to the PNG sequence with:

```text
[0:v]format=yuv444p,settb=1/60[src];
[1:v]format=yuv444p,settb=1/60[enc];
[src][enc]psnr=stats_file=<IGNORED_STATS>
```

Result: `PSNR y:inf u:inf v:inf average:inf min:inf max:inf` over 3,152 rows.

## Harness corrections

The following were orchestration configuration errors, not product findings:

1. An initial PowerShell wrapper passed argument arrays incorrectly and failed
   before four scripts executed. The recorded canonical child-process runs are
   the successful reruns above.
2. First NativeFlow and Win32 attempts omitted the combined pack inherited by
   the documented R1A command sequence. Their explicit arena/flow checks
   therefore failed. Repeating with the same combined ROM-only pack made every
   boundary/launch case pass.
3. A few proof-inspection one-liners required syntax/native-stderr wrapper
   correction. The finished frame set, manifests, repeat encodes, ffprobe, and
   PSNR were independently rehashed by the Luna; no partial artifact is cited
   as accepted evidence.

## Final documentation/cleanliness commands

The terminal sequence is:

```powershell
git diff --check
git status --short --branch
git add -- docs/finish-tasks/R4B-frontend-integration-qa
git diff --cached --check
git diff --cached --name-only
git commit -S -m 'docs: accept R4B frontend integration QA'
git verify-commit <SIGNED_TERMINAL_REPORT_SHA>
git merge-base --is-ancestor 6b5d43546128408de8ab246d22f1b48322714183 `
  <SIGNED_TERMINAL_REPORT_SHA>
git status --porcelain=v1 --untracked-files=all
git rev-parse main
git rev-parse origin/main
git ls-remote origin refs/heads/main
```

Exact terminal SHA/signature, clean status, live refs, and Luna unpin metadata
are supplied after these commands complete.
