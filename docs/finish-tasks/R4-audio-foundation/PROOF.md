# R4 Audio Foundation proof contract

The proof is private, deterministic evidence for the native semantic audio
boundary. It is not a runtime asset and does not claim a cycle-exact NES APU.
The proof command is the hidden developer-only:

```text
--audio-proof PACK OUTPUT_DIR
```

`Run-GameplayAudioTests.ps1` invokes that command twice from an explicit
validated pack path and separate output directories. It requires the exact
44-byte RIFF/WAVE/fmt/data PCM layout, the exact metadata header and 23 fixed
records, vector order/state/positive contiguous coverage, per-vector WAV-slice
FNV-1a32, full-file golden identities, two-run artifact and waveform identity,
and clean repository provenance before writing the path-free script manifest.

The event vectors are, in order:

`TMUS7_START`, `TMUS7_TAIL_END`, `TMUS5_LOOP`, `TMUS6_LOOP`, `TMUS8_END`,
`TFSX8_DRY`, `TFSX10_DRY`, `TSFX3_DRY`, `TSFX5_DRY`, `TSFX6_DRY`, `TSFX11_DRY`,
`TSFX12_DRY`, `TSFX13_DRY`, `TSFX14_DRY`, `TMUS5_TSFX3_OVERRIDE`,
`TDMC0_CLIP`, `TDMC1_CLIP`, `TDMC2_CLIP`, `TDMC3_CLIP`, `TDMC4_CLIP`,
`TDMC_POST_END_HOLD`, `TDMC_RETRIGGER`, and `TDMC_STOP_HOLD`.

The exact state checks include TMUS7 start/tail ticks 5/2615 with playing
1/0, TMUS7 tail accumulator `4377523500`, TMUS5/6 ticks 1362 and accumulator
`22678021800` while playing, TMUS8 tick 396 stopped, frontend/gameplay dry cue
IDs active, mixed track 5 + SFX 3 ticks 11 with both active, DMC clip held
levels 80/60/64/64/46 inactive, post-end 80 inactive, retrigger 102 active,
and stop-hold 102 inactive.

## Terminal proof facts

The canonical semantic pack SHA-256 is
`8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`.
Semantic fingerprints are TMUS `05C00ECB`, TFSX `985DC7ED`, TSFX `968A5DE6`,
and TDMC `AD70E6E8`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| WAV, each run | 8,663,340 | `57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B` |
| events, each run | 8,656 | `3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA` |
| CLI manifest, each run | 688 | `47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298` |
| canonical waveform CSV | — | `76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2` |
| canonical waveform SVG | — | `6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E` |

The run-2 WAV/events/CLI-manifest identities equal run 1. Independently
generated run-2 waveform CSV/SVG identities equal the canonical pair above.
The WAV contains 4,331,648 samples, is 98.223310658 seconds at 44,100 Hz, and
the proof records 23 exact vectors. Across the PCM data, the observed facts are
minimum/maximum `-24079/20927`, mean `-1571.905066`, zero full-scale clipping,
and maximum adjacent delta `24865`.

The ignored root script manifest is
`build/proof/r4-audio-foundation/proof-manifest.txt`. The independently verified
22199fb terminal-candidate checkpoint recorded:

```text
proof_generation_head=22199fb5a0b6a51641319ae5c61cc093e0a79444
root script-manifest SHA256=CA3CB4AE84297085FE415B989A6D2AD9DCD44BCC3BAA73AD887A688041D23996
```

That is a verified 22199fb terminal-candidate checkpoint, not a claim about the
Revision-D commit. This docs-only correction changes HEAD, so its post-commit
root manifest necessarily has a different SHA solely because
`proof_generation_head` changes. The stable WAV/events/CLI-manifest/CSV/SVG
identities remain the same. The final exact Revision-D HEAD and root-manifest
SHA are supplied by the regenerated ignored manifest and Sol handoff after the
commit; no self-reference is fabricated here.

## Timestamp guide

| Vector segment | Time (seconds) |
| --- | --- |
| TMUS7 start | `0–0.092880` |
| TMUS7 tail | `0.092880–43.607075` |
| TMUS5 | `43.607075–66.282812` |
| TMUS6 | `66.282812–88.958549` |
| TMUS8 | `88.958549–95.553016` |
| dry cues | `95.553016–96.388934` |
| mixed override | `96.388934–96.574694` |
| DMC clips | `96.574694–98.014331` |
| post-end hold | `98.014331–98.107211` |
| retrigger | `98.107211–98.130431` |
| stop hold | `98.130431–98.223311` |

## Objective inspection and human signoff

Sol’s objective observation was a personal inspection of the source/ASM,
overall and cue-tail waveforms, and the tail spectrogram. It found dense
continuous music with vector-boundary transitions, discrete cue envelopes and
harmonic sections, broadband/low-frequency DMC texture, exact flat held-DAC
blocks, and constant blocks of 2,100 and 4,987 samples. This is an objective
source/waveform inspection record, not a claim of Sol subjective listening.

Human subjective listening was separately approved by durable checkpoint
`232e57e`: an external listener reported a clean opening/tail/end, correct loops
and stinger, separable cues, a working mixed override, and correct TDMC
held/retrigger/stop windows. This is external human signoff, not Sol or QA
auditory perception. It resolves `BLOCK-R4-AUDIO-LISTENING-001`; `S-BLOCKERS-001`
is completed and unpinned. The earlier honest limitation/ruling remains
checkpoint `94acc9f`.

## Honest limitations

- Native PCM synthesis and nonlinear/cycle-exact NES APU behavior can differ;
  the proof validates the declared native semantic model, not analog hardware.
- DMC reader bit/cycle phase and IRQ timing are not claimed exact.
- DMC IDs 0/1/2 remain unresolved/address-bound; no impact claim is made.
- Effect 5 remains neutral/unresolved.
- Effect 6 remains bounded-correlation only.
- Cross-domain cue call sites and full ACC-AUDIO integration are deferred and
  outside this owned R4 foundation.

No ROM, decoded payload, WAV, event record, waveform, capture, trace, or save
state is committed. All such evidence remains under ignored build output or a
private local test environment.
