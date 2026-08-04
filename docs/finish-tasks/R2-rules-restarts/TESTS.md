# Test and proof record

All commands ran in
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-rules-restarts-sol` with
the canonical ROM used read-only. Generated files are ignored under `build/` or
`build/proof/`.

## Sol personal integrated gates

The frozen implementation tip `1dde1ef` passed:

```powershell
$rom = 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
$env:TECMO_SKIP_SHORTCUT='1'
.\build.ps1

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -S . -B build\sol-r2-rules-cmake-001 -G 'Visual Studio 17 2022' -A x64
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  --build build\sol-r2-rules-cmake-001 --config Release

.\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath $rom `
  -ProofRootPath build\sol-r2-rules-personal-scene-001
.\tools\Run-GameplayPenaltyTests.ps1 -ProjectRoot . -RomPath $rom
.\tools\Run-GameplayViolationRefereeTests.ps1 -ProjectRoot . -RomPath $rom
.\tools\Run-GameplayBackcourtTests.ps1 -ProjectRoot . -RomPath $rom
.\tools\Run-GameplayCourtOrientationTests.ps1 -ProjectRoot . -RomPath $rom
.\tools\Run-GameplayCameraProjectionTests.ps1 -ProjectRoot . -RomPath $rom
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot . -RomPath $rom
.\build\tecmo_port.exe --root . --gameplay-scene-test `
  build\sol-r2-rules-personal-scene-001\asset-pack\gameplay-proof.assetpack
```

Canonical and CMake console/GUI builds were warning-clean. The full scene suite,
all six focused gates, direct console scene route, and hidden GUI scene route
exited 0. Personal proof produced 14 stored frames and two seven-frame native
videos. Paired contact-sheet hash:
`F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C`;
paired video hash:
`B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595`.
The 1920x1440 contact sheet was inspected at full resolution.

## Independent terminal QA

Task `019fcb44-0f91-7632-9b25-88e51b505ce3` independently ran:

- canonical `build.ps1` console and GUI build, exit 0, diagnostic count 0;
- fresh Visual Studio CMake console and GUI targets, exit 0, diagnostic count 0;
- full scene runner with `-Build` into
  `build/qa-gameplay-scene-proof-20260804`, exit 0;
- TPNL, TGVR, TGBC, TGOR, TGCP, and audio runners with `-Build`, all exit 0;
- direct console and hidden GUI scene-test routes, both exit 0;
- two direct `--gameplay-state-test` runs, both exit 0 with
  `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C` and identical output
  SHA-256
  `4DE45F77465A59AF851133C8861DF3895B0C4227FB67E9BFF64F7435726AB87D`.

Independent proof identities:

| Artifact | SHA-256 |
| --- | --- |
| `PROOF-MANIFEST.json` | `7B36A54396F5FAE1D3F3416B6DCA81FD900E6C97329E90754785BF667F6553A1` |
| proof asset pack | `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29` |
| each contact sheet | `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C` |
| each native MP4 | `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595` |
| each decoded-frame hash list | `6FA0AA43130E1EFF92986485EC6305ABE9A86781968FEC24371FA45616F13E9B` |
| scene self-test log | `F303F1464A700A0073DB9CF9AD171032310CCF693ABC94A14D6820B5038592E4` |

The two audio proof WAVs were byte-identical with SHA-256
`57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B`;
the two event logs were byte-identical with SHA-256
`3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA`.

The independent verdict was `ACCEPT`, with P0/P1/P2/P3 all `none`.
