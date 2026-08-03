# Deterministic private proof

Generated evidence is ignored and remains under:

`build/proof/r4-audio-foundation/`

The owned gameplay script removes and recreates that directory on each proof
run, invokes `--audio-proof PACK OUTPUT_DIR` twice, checks byte-identical WAV,
event, and C-manifest artifacts, computes SHA-256, and writes ignored
path-free CSV/SVG waveform evidence plus `proof-manifest.txt`.

## Coverage

The fixed event suite contains 23 unambiguous vectors:

`TMUS7_START`, `TMUS7_TAIL_END`, `TMUS5_LOOP`, `TMUS6_LOOP`, `TMUS8_END`,
`TFSX8_DRY`, `TFSX10_DRY`, `TSFX3_DRY`, `TSFX5_DRY`, `TSFX6_DRY`,
`TSFX11_DRY`, `TSFX12_DRY`, `TSFX13_DRY`, `TSFX14_DRY`,
`TMUS5_TSFX3_OVERRIDE`, `TDMC0_CLIP`, `TDMC1_CLIP`, `TDMC2_CLIP`,
`TDMC3_CLIP`, `TDMC4_CLIP`, `TDMC_POST_END_HOLD`, `TDMC_RETRIGGER`, and
`TDMC_STOP_HOLD`.

TMUS7 drains to clean termination under a checked 3,000,000-sample bound
(the bound is statically required to exceed 2,000,000 samples). TMUS5/6 each
run for 1,000,000 samples with exact cadence checks. DMC clips drain through
inactive state and held-DAC windows. Retrigger and stop-all compare the exact
pre-stop DAC level.

## Final path-free manifest facts

The proof-generation HEAD was
`29611607babe31415ab063520d832631ab3c2e4c`; base was
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. The manifest records local-private
Rev1 source identity (`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`),
pack SHA-256 `8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`,
semantic fingerprints TMUS `05C00ECB`, TFSX `985DC7ED`, TSFX `968A5DE6`, and
TDMC `AD70E6E8`, command/vector count, and `44100Hz_mono_s16le` format.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `run1/audio-proof.wav` (same as run2) | 8663340 | `57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B` |
| `run1/audio-proof.events` (same as run2) | 8656 | `3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA` |
| `run1/audio-proof.manifest` (same as run2) | 688 | `47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298` |
| `waveform/audio-proof-waveform.csv` | 139358 | `76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2` |
| `waveform/audio-proof-waveform.svg` | 47774 | `6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E` |

The generated artifacts are private/ignored build evidence only. No WAV,
event record, waveform, ROM, decoded payload, or trace is committed.

## Source, waveform, and listening notes

Source note: the run used the exact local-private Rev1 ROM and a validated
semantic asset pack. The script-level manifest records the base SHA, committed
proof-generation HEAD, ROM revision/SHA, pack SHA, and semantic pack
fingerprints without recording an absolute path. The C proof command itself
opened no audio device and declared no runtime ROM artifact.

Waveform note: both generated runs were byte-identical. The WAV is a fixed
44.1 kHz mono signed 16-bit little-endian capture; CSV/SVG are deterministic
inspection aids derived from the fixed event sample ranges. These artifacts
remain ignored under `build/proof/r4-audio-foundation/`.

Listening disposition: this worker records no personal subjective listening
acceptance. `[PENDING — independent QA Luna]` Add the exact QA task ID and
findings when supplied. `[PENDING — Sol personal listening]` Add Sol’s honest
listening observations and disposition when supplied. Until then, automated
proof is complete but Sol acceptance remains pending.

## Complete audible approximations and deferred differences

- Native C output is deterministic, but it is not a nonlinear or cycle-exact
  NES APU mixer.
- DMC timing/rate, reader bit order, IRQ behavior, and cycle phase are not
  claimed to be cycle-exact; held-DAC/retrigger/stop continuity is a native
  contract only.
- DMC clip IDs 0/1/2 remain address-bound and unresolved. ABF5 has
  sequence-level bounded correlation only; there is no impact, rim, or
  exclusivity claim.
- Gameplay effect 5 remains neutral/unresolved.
- Gameplay effect 6 remains bounded-correlation only.
- Cross-domain cue routing and full game integration are deferred because cue
  call sites and shared integration boundaries were explicitly excluded.
  Consequently broader ACC-AUDIO is incomplete even though the isolated R4
  foundation is exact/high-confidence where fingerprinted.
- No hardware listening session or claim of original device behavior is part of
  this automated proof; the later QA/Sol observations above remain required for
  final acceptance.
