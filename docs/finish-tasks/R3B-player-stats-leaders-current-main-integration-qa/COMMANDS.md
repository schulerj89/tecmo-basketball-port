# R3B Current-Main Integration QA — Commands

Commands were run from the repository root on the corrected code checkpoint
`20dcf9a4d30f8d4e557ab61df5af8fc34458c82c`, a signed descendant of the
reconciled merge. `<ROM>` denotes the validated ignored private Rev1 ROM and
`<DECOMP>` the local decompilation root. Outputs were kept under:

`build/integration-qa-r3-20260804T184202450Z/`

## Build and direct checks

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
.\build.ps1
.\build\tecmo_port.exe --assetpack-test
.\build\tecmo_port.exe --season-test
.\build\tecmo_port.exe --gameplay-state-test
```

Final corrected result: build exit 0, console and GUI produced, warning lines
0; all three direct checks exit 0. Logs are
`build-warning-clean-final-corrected.log`, `assetpack-self-test-corrected.log`,
`season-self-test-corrected.log`, and
`gameplay-state-self-test-corrected.log`.

## Domain suites

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath <ROM> -DecompRoot <DECOMP> -SkipBuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath <ROM> -DecompRoot <DECOMP> -SkipBuild
.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath <ROM> -Build -ProofRootPath <QA>\gameplay-scene-proof-final-corrected
```

The final corrected Season and Team Data commands exited 0; Season passed
strict TSNS/TSAV provenance, migration/rejection, rollback, native handoff,
malformed-pack guards, and 19 pixel checkpoints; Team Data passed parser,
mapping, input/state, malformed, and 15-pixel regression. The full Scene
command exited 0 and passed the scene/shot/camera/audio/determinism/malformed
matrix. Its proof remains explicitly `DRAFT`.

The direct scene self-test was also run:

```powershell
.\build\tecmo_port.exe --root . --gameplay-scene-test <QA>\gameplay-scene-proof-final-corrected\asset-pack\gameplay-proof.assetpack
```

It exited 0 with `GAMEPLAY SCENE SELF TEST PASS`.

## R2E and platform/flow smoke

The R2E runner was run authoritatively in a child process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplayPresentationTests.ps1 -ProjectRoot . -RomPath <ROM> -Build -ProofRootPath <QA>\gameplay-presentation-proof-final-corrected
```

Result: exit 0; positive two-pass hashes and all six negative framing cases
passed. The manifest was generated at code head `20dcf9a...` while the pending
current/historical `PROOF.md` repair was the only tracked edit, so it records
`clean=false`; no product source was dirty. In-process invocation has the
known stale-exit-code behavior after expected negative native modes, hence the
child process is authoritative.

With the generated season pack bound in the child process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot <DECOMP> -Build
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-NativeFlowTests.ps1 -ProjectRoot . -DecompRoot <DECOMP> -Build -ReportPath <QA>\native-flow-report-corrected.json
```

Both corrected runs exited 0. Win32 verified GUI/console startup, project-root
arguments, working directory, icon, roster-independent startup, window
lifetime, and clean shutdown. Native flow passed all CLI/flow boundaries and
wrote `native-flow-report-corrected.json`.

## Read-only repository checks

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\finish-orchestration\Test-ControlPlane.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\finish-orchestration\Verify-Lineage.ps1
```

Both exited 0 with zero warnings. No control-plane files were changed.
