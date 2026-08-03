# Builds and tests

All worker results are provisional; Sol owns the independent clean build,
Win32 smoke, proof rerun, visual review, and QA.

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

The preserved draft root is:

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

## Sol precommit gate (independent)

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

The worker did not run clean `-RequirePass` in this turn, and independent QA
closure remains pending. The complete raw fault/revision history is in
[LINEAGE.md](LINEAGE.md), and the actual worker draft record is in
[PROOF.md](PROOF.md).
