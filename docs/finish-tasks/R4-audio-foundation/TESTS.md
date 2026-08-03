# Tests and final results

All final domain gates were run from the implementation worktree with
`TECMO_SKIP_SHORTCUT=1` and the canonical local-private Rev1 ROM. The ROM path
is intentionally omitted from committed documentation.

## Build

```powershell
$env:TECMO_SKIP_SHORTCUT = '1'
.\build.ps1
```

Result: build succeeded and produced `build/tecmo_port.exe`.

## Owned suites

```powershell
.\tools\Run-MusicTests.ps1 -RomPath <REV1_ROM>
.\tools\Run-GameplayAudioTests.ps1 -RomPath <REV1_ROM>
.\tools\Run-FrontendAudioTests.ps1 -RomPath <REV1_ROM>
```

Final results:

- `MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations`
- `GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations`
- `FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations`

The gameplay suite also runs the canonical isolated source gate, every
gameplay-owned bounded source mutation, broad asset-pack mutation integration,
the canonical-alias/distinct-container identity gate, the hidden proof command
twice, artifact byte/SHA equality, required vector coverage, and waveform
evidence generation. The music and frontend suites retain their source
mutation gates and direct malformed/NULL/oversized coverage.

## Direct checks

The final run also exercised:

- `--music-source-test <REV1_ROM>` → strict TMUS-1 source success.
- `--gameplay-audio-source-test <REV1_ROM>` → strict TSFX-1/TDMC-1 source
  success and gameplay-local mutation rejection.
- `--frontend-audio-source-test <REV1_ROM>` → strict TFSX-1 source success.
- `--audio-pack-identity-test <PACK_A> <CANONICAL_ALIAS> accept` → pass.
- `--audio-pack-identity-test <PACK_A> <BYTE_IDENTICAL_COPY> reject` →
  `reject-preserve` pass.

No real audio device is required by the portable output transaction seam or
the hidden proof command.
