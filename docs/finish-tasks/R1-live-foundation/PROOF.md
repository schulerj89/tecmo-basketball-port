# R1 LIVE foundation proof record

This is the actual worker proof record, not a fill-in template. The earlier
dirty-worktree capture is preserved as historical `DRAFT` precommit evidence;
the formal clean `-RequirePass` result below supersedes it for acceptance.

## Historical precommit draft root and manifest

- Proof root:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c`
- Manifest:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\PROOF-MANIFEST.json`
- Manifest SHA256:
  `E6E03E13C9396B334A32A09502DEBFBBFCF19CA05587573DDFD8EB7BF31EED82`
- Manifest status: `DRAFT`.
- Base SHA: `ad0f005673692b04772bce3c3b4d3ac4b2624731`.
- Current SHA: `ad0f005673692b04772bce3c3b4d3ac4b2624731`.
- Final SHA: `PENDING_CLEAN_COMMIT`.
- Branch: `codex/r1-live-foundation-luna`; clean=`false`.
- The manifest was self-validated after all suites, negatives, contact sheets,
  native videos, and artifact inventory were complete.
- The final draft was produced with `-Build`; the captured build log was
  warning-clean and the manifest records `build_warning_clean=true`.
- UTC timestamps: proof start `2026-08-03T15:55:43.6908686Z`, draft manifest
  `2026-08-03T15:55:53.9010715Z`, final draft record
  `2026-08-03T15:56:04.5344981Z`.

## Historical Sol precommit gate

Sol independently reran and accepted the implementation/proof gate before this
first worker commit:

- `build.ps1` warning-clean: PASS.
- CPU wrapper: PASS, exactly `680` commands, `24` handlers, and `17`
  mutations.
- Movement wrapper: PASS.
- Production flow: PASS, exact output
  `FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit`.
- Full `Run-GameplaySceneTests.ps1 -Build`: PASS, all four suites. Sol's
  personal proof root was
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-sol-acceptance-20260803-d`;
  manifest SHA256
  `FAC2826E262E0EF5A88A8B8063D48D0D07A47CC72CF7B5F0E22BFF1954DC133D`;
  status `DRAFT`, base/current
  `ad0f005673692b04772bce3c3b4d3ac4b2624731`, final
  `PENDING_CLEAN_COMMIT`, `build_warning_clean=true`, original reference
  validated, `189/189` logs, `254/254` inventory entries, zero path/byte/hash
  mismatches, `14/14` frames, equal contact hash
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`, equal
  native-video hash
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`,
  `7/7` stored/decoded frames per video, and exact cadence/timebase.
- `Run-Win32LaunchSmoke.ps1 -Build`: PASS for GUI/console subsystems,
  project-root arguments, working directory/icon, roster-independent startup,
  lifetime, and clean shutdown. This script and platform code are excluded
  from this slice.

This was an independent precommit gate, not a clean `-RequirePass` result. The
worker's own draft manifest remains historical `DRAFT` evidence with its final
SHA pending.

## Formal Sol PASS

Sol ran the formal proof command:

```text
.\tools\Run-GameplaySceneTests.ps1 -Build -RequirePass -ProjectRoot . -RomPath [canonical Rev1 ROM] -OriginalReferenceManifestPath [accepted CPU 20260803-053244 proof-manifest.json] -ProofRootPath build\live-proof-formal-20260803-e
```

The command exited `0`. All four scene suites passed and the LIVE proof
reported `PASS`.

- Proof root:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-formal-20260803-e`
- Manifest:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-formal-20260803-e\PROOF-MANIFEST.json`
- Manifest SHA256:
  `C465080ECD2D00D5FF905A63537AEBDC62DC5ABE7524B8E08492132417BC546F`
- Generated UTC start: `2026-08-03T16:16:40.0145168Z`.
- Finished UTC: `2026-08-03T16:17:00.2029423Z`.
- Schema: `TGLP-1`; status: `PASS`.
- Base SHA:
  `ad0f005673692b04772bce3c3b4d3ac4b2624731`.
- Current and final SHA:
  `e2333db8fd0ad21c036d0016574c1551929fbb5c`.
- Branch: `codex/r1-live-foundation-luna`; clean=`true`; require_pass=`true`;
  build_requested=`true`; build_warning_clean=`true`; suites_complete=`true`.
- Canonical Rev1 ROM: length `393232`, SHA256
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
- Preserved pack SHA256:
  `695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`.
- TGAI: `7616` bytes, FNV-1a32 `D6C4DB35`; TGMO: `1664` bytes, FNV-1a32
  `6C82A137`.
- The accepted original CPU formal manifest was validated.
- Required logs: `189/189` nonempty. Artifact inventory:
  `254/254` paths, byte counts, and SHA256 values independently checked, with
  zero mismatch.
- Frames: `14/14` paths and SHA256 values correct.
- Contact sheets: two `1920x1440` images, independently hash-equal at
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`.
- Native videos: two `640x480` MP4s, independently hash-equal at
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`.
  Independent ffprobe on each reported
  `r_frame_rate=avg_frame_rate=39375000/655171`,
  `time_base=1/39375000`, and `nb_frames=nb_read_frames=7`.
- The build log contained zero warning matches, and the formal worktree
  remained clean after proof completion.
- All 13 named negative regressions remained listed and passed:
  `dirty RequirePass rejects before PASS`; `wrong-branch RequirePass rejects
  before PASS`; `non-ancestral-base RequirePass rejects before PASS`; `stale
  ROM SHA rejects`; `stale asset-pack SHA rejects`; `stale contact-sheet
  IHDR/metadata rejects (1920x960 is invalid for seven frames)`; `stale video
  rate rejects`; `stale video frame-count rejects`; `stale video time-base
  rejects`; `missing/malformed/cross-pack proof inputs reject`;
  `artifact-inventory missing path rejects`; `artifact-inventory byte mismatch
  rejects`; and `artifact-inventory SHA mismatch rejects`.

Sol visually reinspected the formal contact sheet and shot frame: stable ten
actors, the expected PRESEASON/title presentation, coherent
handoff/movement/pass/switch/deferred/close-shot sequence, and no blank or
corrupt frame. The original-versus-native team, camera/view, resolution,
input-schedule, and frame-timing differences remain explicit; the comparison
is reference-only and makes no parity claim.

## Inputs and fingerprints

- ROM: Rev1, length `393232`, SHA256
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
- Preserved replay pack:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\asset-pack\gameplay-proof.assetpack`; SHA256
  `695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`.
- The scratch pack used during the run is recorded explicitly as ephemeral in
  the manifest; replay commands use the preserved proof-pack path.
- TGAI: `7616` bytes, FNV-1a32 `D6C4DB35`.
- TGMO: `1664` bytes, FNV-1a32 `6C82A137`.
- Bound launch roster, away: `[5,6,10,11,0]`.
- Bound launch roster, home: `[11,10,6,5,1]`.
- Production-style launch teams: away `0`, home `1`; initial routing is
  P1-away/P2-home. Force-possession, source-offset, CPU-only-routing, and
  close-position mutations are deterministic fixtures, not original or
  normal-policy evidence.

## Event order, inputs, and state assertions

The two deterministic repeats use this exact seven-event order:

1. `pretip-start` — real initial PRETIP presentation, no update; asserts
   presentation state, `live_handoff=false`, `first_sync_pending=true`,
   `sync_serial=0`, no holder, away initial possession, and static seed roles
   `4/9`.
2. `live-handoff` — neutral PRETIP updates until LIVE; asserts synchronized
   holder/possession/controller ownership.
3. `human-movement` — P1 holds RIGHT for two updates; asserts one-update TGMO
   latency, displacement, and bound roster/condition.
4. `offensive-pass` — P1 NES A; asserts holder/control transfer to the away
   receiver and selected roster/condition preservation.
5. `defensive-switch` — P1 NES A while home owns the ball; asserts the nearest
   same-team nonholder defender selection.
6. `cpu-target-deferred` — deterministic source-offset fixture; asserts at
   least one validated source target and one deferred actor.
7. `shot-path` — deterministic supported autonomous close-shot fixture; the
   first AI call launches exactly once, then one neutral production outer update
   advances the already-started close shot to `shot_frame=1` without another
   launch. The captured frame is visible playback, not the initial black
   cutaway.

Every event emits one strict JSON line containing selected starters,
phase/possession/holder/orientation/controllers/action/shot fields, live
validity/roles/serials/shot flags/counts, and all ten actors with slot/team,
roster/position/fixed link/CPU target and direction/source target/deferred
metadata. Accepted shot records have `action_serial=1`, `shot_frame=1`,
identical repeat hashes, and close shot active.

## Frames, contact sheets, and native video

- Resolution: `640x480` PNG frames.
- Stored frame count: `14` (`7` events × `2` repeats).
- Decoded native frame count: `14` (`7` per MP4).
- Contact sheets:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\contact-repeat-1.png` and
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\contact-repeat-2.png`;
  dimensions `1920x1440` (3 columns × `ceil(7/3)=3` rows); both SHA256
  `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`.
- Native MP4s:
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\native-repeat-1.mp4` and
  `C:\Users\joshs\Projects\tecmo-basketball-port-r1-live-foundation-luna\build\live-proof-edge-review-20260803-c\native-repeat-2.mp4`; both SHA256
  `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`.
  Each is `640x480`, `r_frame_rate=39375000/655171`,
  `avg_frame_rate=39375000/655171`, `time_base=1/39375000`, and has stored /
  decoded counts `7/7`.
- Decoded-frame-hash-list SHA256 for both videos:
  `6FA0AA43130E1EFF92986485EC6305ABE9A86781968FEC24371FA45616F13E9B`.
- All paired event PNG hashes, contact sheets, MP4 bytes, and decoded frame
  hashes are equal across repeats. The shot PNG SHA256 in both repeats is
  `C1A5507024E6309FD6B5E51570F53250F4EF2E5704CDCAA9F00183EACB128C15`.

## Original-reference comparison

The accepted immutable CPU formal manifest used by this draft is:

`C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna\temp-videos\gameplay-lab\cpu-lifecycle\20260803-053244\proof-manifest.json`

Accepted-reference manifest SHA256:
`E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`.

The wrapper validated its two runs, all 24 `256x224` PNG records and stored
hashes/dimensions, and both `768x896` contact sheets. The accepted
contact-sheet SHA256 is
`2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
Its separate video contract is the exact string
`256x240 original AVI/video contract (separate from PNG raster)`; no original
AVI file is required or claimed. This is immutable CPU formal-proof evidence,
not a native LIVE frame comparison.

### Sol visual inspection

Sol inspected the native proof and the accepted original reference separately.
The native proof is Hawks/Celtics at `640x480`, using a different native
camera/half-court event-fixture view. The accepted original is Bulls/Celtics
`256x224` PNG running-clock scrolling gameplay. Teams, view/camera, resolution,
input schedule, and frame timing differ. The comparison is reference-only and
incomplete for same-frame positional or visual fidelity; it does not claim
parity.

The native observation was stable ten-actor state with coherent PRETIP,
handoff, movement, pass, switch, and deferred events, the expected PRESEASON
title frame, visible close-shot playback at `shot_frame=1` and
`action_serial=1`, and no corrupted or blank LIVE frame. The original contact
sheet remains `768x896` with SHA256
`2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37` and was
visually coherent running-clock gameplay.

## Commands and tool record

Run from the worker root:

```text
.\build.ps1
.\tools\Run-GameplayCpuSteeringTests.ps1 -ProjectRoot . -RomPath [LOCAL_REV1_ROM]
.\tools\Run-GameplayMovementTests.ps1 -ProjectRoot . -RomPath [LOCAL_REV1_ROM]
.\build\tecmo_port.exe --root [DECOMP_ROOT] --flow-test
.\tools\Run-GameplaySceneTests.ps1 -Build -ProjectRoot . -RomPath [LOCAL_REV1_ROM] -OriginalReferenceManifestPath [ACCEPTED_CPU_PROOF_MANIFEST]
```

The wrapper also recorded every native proof event using the preserved
`asset-pack\gameplay-proof.assetpack`, ffmpeg encode commands, ffprobe stream
probes, framemd5 decode commands, sanitized input schedule, UTC timestamps,
PowerShell/ffmpeg/ffprobe versions, and all copied logs. The final draft
contains `189` required nonempty logs and `254` artifact-inventory entries;
each inventory path, byte count, and SHA256 was checked during final manifest
self-validation.

## Negative gates and known limits

The wrapper rejects dirty/wrong-branch/non-ancestral synthetic RequirePass
states, stale ROM/pack/contact/video/count/timebase metadata,
missing/malformed/cross-pack proof inputs, missing ffmpeg/ffprobe, malformed
PNG/video dimensions, mismatched repeat hashes/counts, invalid source maps,
and missing/empty inventory artifacts. It also retains the full scene negative
matrix and unchanged four-suite orchestration.

The draft is not final acceptance. Remaining limits are original Bank05
dynamic matchup/candidate semantics, original first running-clock RAM snapshot,
caller-derived shot workspaces/RNG, and unsupported jump/far/controller-
dependent shots, which remain explicit deferred/non-launch classifications.
The native adapter follows a referenced actor's current coordinate on every
immutable post-human snapshot/tick; only original Bank05 dynamic
retarget/matchup semantics remain incomplete/unproven.
