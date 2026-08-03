# Proof handoff

## Signed worker lineage and Sol integration

The writable Luna task is
`019fc912-a957-79f0-89a3-7e2e2d10db24`, titled
`Tecmo R2 Clocks Lineups Fatigue Implementation — Luna Max`. Its three
lineage commits are:

- implementation/tests: `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`;
- first documentation commit: `540ae0ba47ef44d6096781ffd0c276012e683221`;
- documentation metadata correction: `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

All three report Good Git signatures for `jaystar524@gmail.com` with RSA key
fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. Sol personally
fast-forwarded this signed lineage into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

## Sol personal QA

The authoritative Sol task is
`019fc8ff-4ec4-7b20-86c6-9c9614f9194c`, titled
`Tecmo R2 Clocks Lineups Fatigue Domain Orchestrator — Sol Max`. Sol's
personal terminal QA completed the following:

| Command/result | Sol-owned result |
|---|---|
| `./build.ps1` | PASS; warning-free `tecmo_port.exe` and `tecmo_port_game.exe` built. |
| `build\\tecmo_port.exe --gameplay-state-test` | PASS; `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`. |
| `build\\tecmo_port.exe --assetpack-test` | PASS; `Asset pack self-test passed.` |
| `Run-GameplayFreeThrowLineupTests.ps1` with exact Rev1 | PASS; both orientations, 10 actors, 4 policies, indices 517..520, strict source/dependency rejection, 12 selected source mutations. |
| `Run-GameplayFatigueTests.ps1` with exact Rev1 | PASS. |
| `Run-GameplayCameraProjectionTests.ps1` with exact Rev1 | PASS; 21 source mutations. |
| `Run-GameplayPenaltyTests.ps1` with exact Rev1 | PASS; 24 source mutations. |
| `Run-GameplayViolationRefereeTests.ps1` with exact Rev1 | PASS. |
| `Run-AssetPackTests.ps1` with exact Rev1 | PASS; 86 entries, validated pack size `1,401,618`, SHA-256 `695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`. |
| `Run-GameplaySceneTests.ps1` without `-RequirePass` | PASS; R1 branch/base gates were intentionally inapplicable. Draft LIVE proof root and manifest are recorded below. |

`Run-NativeFlowTests.ps1` initially stopped at `numeric-render-suffix` because
the runtime asset-pack precondition was absent. Direct isolation proved the
render succeeded with a valid pack. The rerun with `TECMO_ASSETPACK` set to the
exact validated 86-entry pack passed all flow and CLI boundary cases. This is
recorded as an environment-precondition diagnosis, not a product failure or
an unqualified first-attempt pass.

The draft LIVE proof from the scene runner is:

- root: `build/live-proof-20260803T202541324Z`;
- manifest SHA-256: `BC13A03D759EBE5003A3E56C7256D4EE40791DA2E7CC0D2E2379B57DCC3C321B`;
- inventory: 255 files, two native videos, 14 frames, and a 1920x1440 contact
  sheet.

## Task-specific deterministic production proof

The Sol-owned proof root is:

`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-20260803T203000Z`

Its exact proof facts are:

- manifest: `PROOF-MANIFEST.json`, SHA-256
  `12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1`;
- inventory: 97 hashed artifacts plus the manifest, 98 files total,
  `110,862,262` bytes;
- commands file SHA-256:
  `3DE73EA26802C39B7354362D2D580929D5372028AF42A4B8776BE42E223A2F3B`;
- 81 production render frames named
  `gameplay-shot-clock-violation-frame0..80`, each 640x480;
- native MP4 `shot-clock-violation-native.mp4`, 15,769 bytes, SHA-256
  `AD682F67F0EF43C2BDD08D1FE80E4F2146A83E211FEA9B2459AAB9E005683FFE`;
- ffprobe: `width=640`, `height=480`,
  `r_frame_rate=avg_frame_rate=39375000/655171`,
  `time_base=1/39375000`, `nb_frames=nb_read_frames=81`;
- decoded framemd5 contains exactly 81 frames;
- all-81 numbered contact sheet SHA-256
  `3D09097FD2AFA88CFD6F2026CB349EFD6C63B5749C9E12EAA659A5DEFE5BEF43`;
- numbered key-frame sheet for frames 0, 9, 23, 27, and 80 SHA-256
  `D8411BB3FBA473434D4825DB29332CB741A14BBA74E145ED460C29FDAD218450`;
- free-throw left/right labeled contact sheet SHA-256
  `A91ABB866BB81A6384C680707E8CFA9C6118CD6B9E01279968456111B7931BFF`;
- selected shot-clock frames and both free-throw modes were re-rendered
  separately, with every SHA matching byte-for-byte; left/right free-throw
  orientation hashes are distinct.

This proof changes no audio semantics, so audio is N/A. It also does not claim
period, halftime, or final render-mode ownership; those mechanics remain
state/event-only in this scope.

## Acceptance still pending

Independent QA task ID/title and outcome are not supplied and remain pending.
The terminal accepted SHA also remains pending until independent QA and the
final revision. No independent QA identity or result is inferred here.

Ignored build outputs, raw ROM/decomp/capture material, and proof-only payloads
remain outside the committed product. The exact visual observations are
recorded in `OBSERVATIONS.md`.
