# Proof handoff

## Signed worker lineage and Sol integration

The writable Luna task is
`019fc912-a957-79f0-89a3-7e2e2d10db24`, titled
`Tecmo R2 Clocks Lineups Fatigue Implementation — Luna Max`. Its three
original lineage commits are:

- implementation/tests: `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`;
- first documentation commit: `540ae0ba47ef44d6096781ffd0c276012e683221`;
- documentation metadata correction: `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

The signed remediation commit is
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`.

All four commits report Good Git signatures for `jaystar524@gmail.com` with
RSA key fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.
Sol personally fast-forwarded the original signed lineage into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`, then reviewed and accepted the
remediation at
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` by ff-only integration. Sol's
source review accepted staged gameplay-state mutators and event outputs,
pre-write overlap rejection, bounded TGFT/TGFL destruction/replacement frees,
and the absence of an arbitrary invalid-pointer guarantee in portable C.

## Independent QA historical audit

The read-only independent QA task was:

- ID: `019fc957-a425-70f3-83b9-1e63dfdba40e`;
- exact title: `Tecmo R2 Clocks Lineups Fatigue Independent QA — Luna Max`;
- model/thinking: `gpt-5.6-luna/max`;
- projectless/null-Git, created_at epoch `1785789391`
  (`2026-08-03T20:36:31Z`);
- frozen audit candidate:
  `1536ae31e7016f6e9adbddb7868e2d40e51c1085`.

Its initial historical verdict was `FAIL` due to P2 findings only; it
explicitly reported no P0 or P1. Read-only integrity passed for the exact
branch/HEAD/base/merge-base, four linear Good-signed commits, 18 allowed
changed paths, clean status/diff-check, and proof inventory/hash validation.
The P2 remediation in signed commit
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` addresses the public state
transaction boundaries and TGFT/TGFL corrupt-destructor safety. The
independent QA task then performed the closure re-audit described below.

### Independent re-audit result

The same independent QA task
`019fc957-a425-70f3-83b9-1e63dfdba40e` — `Tecmo R2 Clocks Lineups Fatigue
Independent QA — Luna Max` — re-audited frozen candidate
`1567f284ff48a2334fb6a9bd82d00aadf0cdb373` with
`gpt-5.6-luna/max`, projectless/null-Git, created at
`2026-08-03T20:36:31Z`. Its verdict was `PASS`: no remaining actionable
findings and no P0/P1.

The historical P2 public-state mutator alias/rollback issue is resolved. The
historical P2 TGFT/TGFL destruction/replacement issue is resolved within the
documented bounded contract; portable C still does not claim arbitrary
invalid-pointer detection. P3 fixed-slot bridge wording, physical-frame and
render-mode wording, and proof-source/candidate identity are all resolved.

Read-only integrity passed for the exact branch/HEAD/base/merge-base, clean
status/diff-check, six linear first-parent commits with no merges, and all six
Good signatures for `jaystar524@gmail.com` with RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. Exactly 18 authorized
paths were changed; `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` through
`1567f284ff48a2334fb6a9bd82d00aadf0cdb373` changed six task docs only.

The proof inventory matched all 97 manifest artifacts, including paths, sizes,
and hashes, with no missing or extra files. The audit confirmed the 81-frame
contracts, deterministic rerender hashes, distinct free-throw orientations,
video/ffprobe/decoded-frame contracts, no audio stream, and the draft LIVE
manifest SHA-256
`4C522B29A0D82D5313F01D2C4436A46EF87635E4406DAD93527D18DD894A745E`.
The independent auditor did not rerun product tests and did not personally
visually accept frames; Sol's v2 execution and visual acceptance remain the
authoritative records in this document.

The remaining bounded boundary is dynamic substitutions and production
active-lineup ownership, which remain `incomplete`. No exact visual or audio
semantics claim is added. The QA task remains pinned only until Sol performs a
closure-doc consistency recheck and durably captures that result.

## Sol v1 personal QA and proof-source

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
- 81 production render frames are stored at
  `shot-clock-frames/frame-0000.png` through
  `shot-clock-frames/frame-0080.png`, each 640x480. The render-mode
  identifiers remain `gameplay-shot-clock-violation-frame0..80`;
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

The v1 manifest above is bound to proof-source HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`, not to the frozen independent-QA
candidate or the remediation. The v2 proof below is a separate proof bound to
the remediation HEAD.

## Sol v2 personal QA and proof-source

Sol's v2 QA ran at exact product/proof-source HEAD
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`. The build used
`TECMO_SKIP_SHORTCUT=1` and was warning-free. Sol recorded:

| Command/result | Sol-owned v2 result |
|---|---|
| `build.ps1` with `TECMO_SKIP_SHORTCUT=1` | PASS; warning-free `tecmo_port.exe` and `tecmo_port_game.exe` built. |
| `build\tecmo_port.exe --gameplay-state-test` | PASS; `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`. |
| `build\tecmo_port.exe --assetpack-test` | PASS; `Asset pack self-test passed.` |
| Exact Rev1 TGFL focused runner | PASS; 12 selected source mutations. |
| Exact Rev1 TGFT focused runner | PASS. |
| Exact Rev1 TGCP-2 camera runner | PASS; 21 mutations. |
| Exact Rev1 TPNL-1 penalty runner | PASS; 24 mutations. |
| Exact Rev1 TGVR-1 violation/referee runner | PASS. |
| Full AssetPackTests | PASS; 86 entries, pack size `1,401,618` bytes, SHA-256 `695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`. |
| NativeFlowTests with the exact validated pack | PASS; all CLI boundaries, with no initial asset-pack precondition failure in v2. |
| GameplaySceneTests without `-RequirePass` | PASS; R1 branch/base gates remained intentionally inapplicable. |

The v2 scene proof draft root is
`build/live-proof-20260803T212356578Z`; it contains 255 files and has manifest
SHA-256
`4C522B29A0D82D5313F01D2C4436A46EF87635E4406DAD93527D18DD894A745E`.
Tracked status and diff-check were clean.

## Task-specific deterministic v2 production proof

The Sol-owned v2 proof root is:

`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-v2-20260803T212500Z`

Its exact proof facts are:

- schema: `tecmo.r2-clock-lineups-fatigue.proof/v2`;
- proof-source HEAD: `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`;
- manifest: `PROOF-MANIFEST.json`, 33,453 bytes, SHA-256
  `1FA074FB90D87AF48A3FB78DB50E8B96A78C7F653EC9EFA76BF581B8FC0F51C3`;
- inventory: 97 hashed artifacts plus the manifest, 98 files total,
  `110,863,737` bytes;
- `COMMANDS.txt` SHA-256:
  `F4A61AFB876319DB37ED3A25992A0DDF62FFE6131A93D7304E0DC7C6890B66E4`;
- 81 frames at `shot-clock-frames/frame-0000.png` through
  `shot-clock-frames/frame-0080.png`; render-mode identifiers remain
  `gameplay-shot-clock-violation-frame0..80`;
- native MP4 `shot-clock-violation-native.mp4`, 15,769 bytes, SHA-256
  `AD682F67F0EF43C2BDD08D1FE80E4F2146A83E211FEA9B2459AAB9E005683FFE`;
- ffprobe: `640x480`, `r_frame_rate=avg_frame_rate=39375000/655171`,
  `time_base=1/39375000`, `nb_frames=nb_read_frames=81`;
- decoded framemd5 contains exactly 81 frames;
- all-frame contact sheet SHA-256
  `3D09097FD2AFA88CFD6F2026CB349EFD6C63B5749C9E12EAA659A5DEFE5BEF43`;
- numbered key-frame sheet for frames 0, 9, 23, 27, and 80 SHA-256
  `D8411BB3FBA473434D4825DB29332CB741A14BBA74E145ED460C29FDAD218450`;
- labeled free-throw contact sheet SHA-256
  `A91ABB866BB81A6384C680707E8CFA9C6118CD6B9E01279968456111B7931BFF`.

All 81 v2 shot-clock PNGs and both v2 free-throw PNGs are byte-identical to
v1. The MP4 and all three contact/key sheets are also byte-identical to v1.
Selected v2 shot frames 0, 9, 23, 27, and 80 and both free-throw sides were
rerendered independently and matched byte-for-byte; left/right free-throw
hashes remain distinct.

Sol personally re-inspected the v2 keyframes and free-throw contact sheet and
confirmed the same bounded clean observations recorded in
`OBSERVATIONS.md`: no clipping, HUD overlap, corrupt pixels, missing actors,
or orientation collapse. Audio is N/A. No period, halftime, or final render
ownership claim is made.

The v2 manifest is bound to product/proof-source HEAD
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`. The independent re-audit
candidate is the docs-only descendant
`1567f284ff48a2334fb6a9bd82d00aadf0cdb373`; this does not constitute an
artifact-integrity mismatch or change the v2 proof binding.

## Closure state

Independent re-audit of
`019fc957-a425-70f3-83b9-1e63dfdba40e` passed with no remaining actionable
findings and no P0/P1. The QA task remains pinned only until Sol completes the
closure-doc consistency recheck and durably captures it. The final accepted
SHA is the Good-signed closure descendant reported externally by its git
object/handoff; this document does not make an impossible self-referential
SHA claim.

Ignored build outputs, raw ROM/decomp/capture material, and proof-only payloads
remain outside the committed product. The exact visual observations are
recorded in `OBSERVATIONS.md`.
