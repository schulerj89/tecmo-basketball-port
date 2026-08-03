# R4 Audio Foundation terminal tests

All commands below use the local-private Rev1 ROM only through
`TECMO_ROM_PATH`. No private path is recorded in this document. The expected
ROM SHA-256 is
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

## Reproducible command sequence

From the project root:

```powershell
$env:TECMO_SKIP_SHORTCUT = "1"
$rom = $env:TECMO_ROM_PATH
if (!$rom) { throw "Set TECMO_ROM_PATH to the local Rev1 ROM before running R4 audio tests." }
if ((Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash -ne "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4") { throw "Unexpected Rev1 ROM SHA-256." }

& .\build.ps1
if ($LASTEXITCODE -ne 0) { throw "build.ps1 failed." }

& .\tools\Run-MusicTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $rom
& .\tools\Run-FrontendAudioTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $rom
& .\tools\Run-GameplayAudioTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $rom
```

The terminal results are:

```text
MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations
FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations
GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations
```

The gameplay suite builds the strict gameplay pack, exercises broad asset-pack
mutations, runs the isolated gameplay-only source importer gate, checks the
real-pack canonical-alias/distinct-container identity gate, and invokes
`--audio-proof PACK OUTPUT_DIR` twice. The proof gate requires:

- the exact single metadata header followed by exactly 23 fixed-field records;
- exact vector index/name/order, positive count, contiguous sample coverage, and
  final coverage equal to WAV samples;
- all emitted music/SFX/DMC state fields and the expected per-vector PCM FNV;
- checked FNV-1a32 recomputation over each exact WAV slice for both runs;
- exact 44-byte 44.1 kHz mono 16-bit little-endian RIFF layout;
- full-file golden SHA-256, byte-identical second run, independently generated
  byte-identical waveform CSV/SVG, and path-free ASCII/LF manifest;
- exact repository root, 40-hex HEAD, clean tracked/index state, and merge-base
  equal to `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` before writing the manifest.

The focused Revision-C script commit is
`96471e42f79669379df0a573e8e82be037e559fb`; its clean gameplay proof gate
passed before the documentation commit. The final terminal run is repeated
after the separate documentation commit so the ignored manifest records the
current committed branch HEAD rather than the pre-docs script HEAD.

## Terminal proof identities

The final ignored root manifest is
`build/proof/r4-audio-foundation/proof-manifest.txt`. It records the current
terminal `proof_generation_head`, base SHA, private ROM revision/SHA, pack SHA,
semantic pack fingerprints, vector count, sample format, and both-run
artifact/waveform hashes. The independently verified 22199fb terminal-candidate
checkpoint recorded:

```text
proof_generation_head=22199fb5a0b6a51641319ae5c61cc093e0a79444
root script-manifest SHA256=CA3CB4AE84297085FE415B989A6D2AD9DCD44BCC3BAA73AD887A688041D23996
```

This is the verified 22199fb terminal-candidate checkpoint. The current
docs-only Revision-D correction changes HEAD, so its post-commit root manifest
has a different SHA solely because `proof_generation_head` changes. Stable
WAV/events/CLI-manifest/CSV/SVG identities remain the same. The final exact
Revision-D HEAD and root-manifest SHA are recorded by the regenerated ignored
manifest and Sol handoff after commit; no self-reference is asserted here.

The stable artifact identities are:

- WAV, 8,663,340 bytes, 4,331,648 samples: `57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B`
- events, 8,656 bytes: `3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA`
- CLI manifest, 688 bytes: `47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298`
- waveform CSV: `76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2`
- waveform SVG: `6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E`

Both proof runs must retain those identities. Generated output is ignored and
must not be added to Git.

## Final repository audit

At the final committed tip, run:

```powershell
$base = "6d8f9c7a99a7ce188f1a523247d3a9b9093860fb"
git status --short --branch
git diff --check "$base..HEAD"
git merge-base $base HEAD
git diff --name-only $base HEAD
```

Expected conditions are branch `codex/r4-audio-foundation-luna`, a clean
tracked/index status, merge-base exactly equal to the base above, and changed
paths limited to the delegated audio implementation, owned test scripts, and
`docs/finish-tasks/R4-audio-foundation/`. The final docs commit SHA is supplied
by the post-commit ignored manifest and Sol handoff rather than embedded in its
own tracked text.

## QA harness notes

The initial QA header/field-map/FNV signedness assumptions were corrected. Two
combined provenance probes were rejected by the shell wrapper before execution;
split safe probes then succeeded, including the real invalid-index rejection.
The actual repository tracked/index state remained clean throughout the final
terminal run. These harness faults do not weaken the proof gate.
