# Builds and tests

The worker's precommit results remain historical evidence. Sol's formal proof
is now PASS; independent QA and ff-only merge are still pending.

## Warning-clean build

From `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna`:

```text
.\build.ps1
```

PASS. `tecmo_port.exe` and `tecmo_port_game.exe` built, including the LIVE
foundation/proof units. No MSVC warning lines were emitted. The initial two
`C4701` diagnostics remain recorded in [LINEAGE.md](LINEAGE.md); they were
fixed before this build.

## Focused and production regressions

```text
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot . -RomPath [LOCAL_REV1_ROM]
.\tools\Run-GameplayMovementTests.ps1 -ProjectRoot . -RomPath [LOCAL_REV1_ROM]
.\build\tecmo_port.exe --root [DECOMP_ROOT] --flow-test
```

PASS results:

- CPU wrapper: exact Rev1 importer, ten source spans plus seven lifecycle
  spans, 680 aligned commands, 24 handlers, eight directions, transactional
  TGMO direction/movement composition, 17 ROM mutations, and immutable
  source-map compatibility.
- Movement wrapper: exact Rev1 importer, seven source spans, strict payload /
  provenance/dependencies, transactional rejection, normal/fast/slow/
  diagonal/boundary/Y-gate vectors, live integration, and 7 ROM mutations.
- Flow test: `FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit`.
- Direct scene test and the full wrapper both invoke all four accepted suites:
  PRETIP, render contract, shot clock, and state flow; no suite was suppressed.

## Full scene/proof wrapper

```text
.\tools\Run-GameplaySceneTests.ps1 -Build -ProjectRoot . -RomPath [LOCAL_REV1_ROM] -OriginalReferenceManifestPath [ACCEPTED_CPU_PROOF_MANIFEST]
```

PASS. The accepted original manifest was:

```text
C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna\temp-videos\gameplay-lab\cpu-lifecycle\20260803-053244\proof-manifest.json
```

The historical precommit draft root is:

```text
C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c
```

It records 14 PNG frames, two deterministic `1920x1440` contact sheets,
two deterministic native `640x480` MP4s at `39375000/655171` with `7/7`
stored/decoded frames each, the original CPU reference validation, 189
required logs, and 254 artifact entries. The shot event is visibly rendered
at `shot_frame=1` and retains exact-once `action_serial=1`.

The wrapper's stale metadata, missing/malformed/cross-pack input, synthetic
RequirePass, source-map, and full scene negative regressions passed. Its final
manifest validator checks every inventory path, byte count, and SHA256, not
only the inventory count. The LIVE state-flow regression also covers the
scene-level edge/corner inert `scene_update_ai` transaction and 120 neutral
bound running-clock updates with roster, condition, ownership, and exact-once
shot-launch checks.

## Historical Sol precommit gate (independent)

Sol's personal precommit rerun passed the bounded implementation/proof gate:

- Warning-clean `.\build.ps1`: PASS.
- CPU wrapper: PASS, exactly `680` commands, `24` handlers, and `17`
  mutations.
- Movement wrapper: PASS.
- Production flow: PASS, exact output
  `FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit`.
- Full `Run-GameplaySceneTests.ps1 -Build`: PASS, all four suites. Root:
  `build/live-proof-sol-acceptance-20260803-d`; manifest SHA256
  `FAC2826E262E0EF5A88A8B8063D48D0D07A47CC72CF7B5F0E22BFF1954DC133D`;
  `DRAFT`, base/current
  `ad0f005673692b04772bce3c3b4d3ac4b2624731`, final
  `PENDING_CLEAN_COMMIT`; `build_warning_clean=true`; original validated;
  `189/189` logs and `254/254` inventory entries; zero path/byte/hash
  mismatches; `14/14` frames; equal contacts hash
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`; equal
  native-video hash
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`;
  `7/7` stored/decoded frames per video; exact cadence/timebase.
- `Run-Win32LaunchSmokeTest.ps1 -Build`: PASS for GUI/console subsystems,
  project-root arguments, working directory/icon, roster-independent startup,
  lifetime, and clean shutdown.

## Formal proof PASS

Sol ran the exact formal command:

```text
.\tools\Run-GameplaySceneTests.ps1 -Build -RequirePass -ProjectRoot . -RomPath [canonical Rev1 ROM] -OriginalReferenceManifestPath [accepted CPU 20260803-053244 proof-manifest.json] -ProofRootPath build\live-proof-formal-20260803-e
```

It exited `0`; all four scene suites passed and LIVE PROOF reported `PASS`.
The formal root is
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-formal-20260803-e`;
the manifest SHA256 is
`C465080ECD2D00D5FF905A63537AEBDC62DC5ABE7524B8E08492132417BC546F`.
Schema/status are `TGLP-1`/`PASS`; base is
`ad0f005673692b04772bce3c3b4d3ac4b2624731`; current/final is
`e2333db8fd0ad21c036d0016574c1551929fbb5c`; branch is correct; clean,
require_pass, build_requested, build_warning_clean, and suites_complete are
all true. Generated UTC start/finish were
`2026-08-03T16:16:40.0145168Z` and `2026-08-03T16:17:00.2029423Z`.

The canonical ROM was length `393232`, SHA256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`; the
pack SHA256 was
`695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`;
TGAI was `7616/D6C4DB35` and TGMO was `1664/6C82A137`; the accepted original
manifest validated. Logs were `189/189` nonempty and inventory was `254/254`
with independently checked paths/bytes/SHA and zero mismatch. Frames were
`14/14`; two `1920x1440` contacts matched at
`F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`; two
`640x480` MP4s matched at
`B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595` and each
ffprobe reported `r_frame_rate=avg_frame_rate=39375000/655171`,
`time_base=1/39375000`, `nb_frames=nb_read_frames=7`. The build log had zero
warning matches and the formal worktree remained clean. All 13 named negative
regressions remained listed and passed.

Sol's formal visual reinspection found stable ten-actor state, expected
PRESEASON/title presentation, coherent handoff/movement/pass/switch/deferred/
close-shot sequence, and no blank or corrupt frame. The documented original
versus native team/view/resolution/input/timing differences remain; this is
reference-only and makes no parity claim.

Independent QA has not yet accepted the two-commit chain. The complete raw
fault/revision history is in [LINEAGE.md](LINEAGE.md), and the historical
precommit plus formal records are in [PROOF.md](PROOF.md).
