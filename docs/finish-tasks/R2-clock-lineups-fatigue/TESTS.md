# Tests

All commands below were run from
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna`.

## Canonical input

Focused runners accepted only this test-only input:

`C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`

SHA-256:
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

The ROM was not copied into the worktree, linked at runtime, or committed.

## Final results

| Command | Result |
|---|---|
| `.\build.ps1` | Exit 0; warning-free MSVC build produced `build\tecmo_port.exe` and `build\tecmo_port_game.exe`. |
| `.\build\tecmo_port.exe --assetpack-test` | Exit 0; `Asset pack self-test passed.` |
| `.\build\tecmo_port.exe --gameplay-state-test` | Exit 0; `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`. |
| `.\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna -RomPath C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes` | Exit 0; `TGFT-1 fatigue tests passed.` |
| `.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna -RomPath C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes` | Exit 0; `TGFL-1 focused tests passed` with 12 source mutations. |

The TGFT runner covers canonical 512-byte payload/fingerprint, source-map
spans, malformed dependency/entry sizes, active/bench/recovery vectors,
cadence modes, `0 -> 255`, state/input/assets/storage aliases, and transactional
replacement. The TGFL runner covers canonical 1216-byte payload/fingerprint,
strict source map, 12 selected source mutations plus the listed
dependency/size/strict-object cases, both orientations, all base placements,
and the self-test’s 720 policy
vectors plus `0xFF` predicate check.

## Sol v1 personal QA and production proof

After fast-forwarding the signed lineage at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`, Sol reran the broader exact-Rev1
QA set. Build, gameplay-state, asset-pack, TGFL, TGFT, camera projection,
penalty, violation-referee, and scene tests passed. The asset-pack run passed
86 entries with validated pack size `1,401,618` and SHA-256
`695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07`.
Camera projection reported 21 source mutations; penalty reported 24; TGFL
reported 12 selected source mutations.

The first `Run-NativeFlowTests.ps1` invocation stopped at
`numeric-render-suffix` because `TECMO_ASSETPACK` was not set. Direct
isolation passed with a valid pack, and the rerun with `TECMO_ASSETPACK` set to
the exact validated 86-entry pack passed all flow and CLI boundary cases. This
was an environment-precondition diagnosis, not a product failure and not an
unqualified first-attempt pass.

`Run-GameplaySceneTests.ps1` without `-RequirePass` passed; its R1 branch/base
gates were intentionally inapplicable. Draft LIVE proof root:
`build/live-proof-20260803T202541324Z`; manifest SHA-256
`BC13A03D759EBE5003A3E56C7256D4EE40791DA2E7CC0D2E2379B57DCC3C321B`; 255
files, two native videos, 14 frames, and a 1920x1440 contact sheet.

The task-specific deterministic proof is rooted at
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-20260803T203000Z`.
Its manifest SHA-256 is
`12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1`;
the inventory is 97 hashed artifacts plus the manifest (98 files,
110,862,262 bytes); and the commands SHA-256 is
`3DE73EA26802C39B7354362D2D580929D5372028AF42A4B8776BE42E223A2F3B`.
The proof contains 81 numbered 640x480 shot-clock/violation frames and the
native MP4 `shot-clock-violation-native.mp4` (15,769 bytes, SHA-256
`AD682F67F0EF43C2BDD08D1FE80E4F2146A83E211FEA9B2459AAB9E005683FFE`).
ffprobe reported width 640, height 480, both frame rates
`39375000/655171`, time base `1/39375000`, and 81 frames; decoded framemd5
also contained exactly 81 frames. The all-81 sheet SHA-256 is
`3D09097FD2AFA88CFD6F2026CB349EFD6C63B5749C9E12EAA659A5DEFE5BEF43`, the
key-frame sheet SHA-256 is
`D8411BB3FBA473434D4825DB29332CB741A14BBA74E145ED460C29FDAD218450`, and
the labeled left/right free-throw sheet SHA-256 is
`A91ABB866BB81A6384C680707E8CFA9C6118CD6B9E01279968456111B7931BFF`.
Separate re-renders matched every selected SHA byte-for-byte, and the two
free-throw orientation hashes were distinct.

The manifest and render proof above are bound to proof-source HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`; they are not v2 proof for the
frozen independent-QA candidate or this remediation. No v2 Sol QA/proof was
regenerated. Audio is N/A; the bounded visual proof does not establish
period/halftime/final render semantics.

## Independent QA historical result

Read-only independent QA task
`019fc957-a425-70f3-83b9-1e63dfdba40e`,
`Tecmo R2 Clocks Lineups Fatigue Independent QA — Luna Max`, created at
`2026-08-03T20:36:31Z` with `gpt-5.6-luna/max`, froze candidate
`1536ae31e7016f6e9adbddb7868e2d40e51c1085`. Its initial verdict was `FAIL`
due to P2 only, with explicitly no P0/P1. Exact branch/HEAD/base/merge-base,
four linear Good-signed commits, 18 allowed changed paths, clean/diff-check,
and proof inventory/hash validation passed. The QA task remains pinned for
re-audit.

## Remediation verification

This revision adds no new production proof. The owned remediation verified:

| Command | Result |
|---|---|
| `.\build.ps1` | Exit 0; warning-free MSVC build produced both executables. |
| `.\build\tecmo_port.exe --gameplay-state-test` | Exit 0; `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`. |
| `.\build\tecmo_port.exe --assetpack-test` | Exit 0; `Asset pack self-test passed.` |
| `.\tools\Run-GameplayFatigueTests.ps1` with exact Rev1 | Exit 0; `TGFT-1 fatigue tests passed.` |
| `.\tools\Run-GameplayFreeThrowLineupTests.ps1` with exact Rev1 | Exit 0; `TGFL-1 focused tests passed` with 12 selected source mutations. |

Independent re-audit, v2 Sol QA/proof, and the terminal accepted SHA remain
pending. Do not interpret these remediation tests as regenerated production
render proof.

## Review corrections represented in the final run

An earlier TGFL focused attempt exposed that the builder’s newly required
internal full-ROM SHA gate changed mutated-ROM diagnostics from source-span
messages to the full-ROM SHA message. The owned runner was updated to assert
that stronger contract; the final run passed. This was a test-contract
correction, not a product failure.

The worker did not run or own Sol's v1 production proof. Sol-owned visual output
is limited to the bounded proof cases above; it does not alter the API/state
behavior classifications or establish audio semantics.
