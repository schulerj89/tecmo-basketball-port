# R4 audio route evidence ledger

This document separates foundation capability from production-route proof.
The canonical Rev1 ROM is private test evidence only; its SHA-256 is
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
No ROM byte, generated asset pack, WAV, event record, waveform, capture, or
private path is tracked.

## Frozen foundation identities

The wrapper refuses to run if any accepted suite drifts:

| Frozen suite | SHA-256 |
| --- | --- |
| `tools/Run-MusicTests.ps1` | `C69CDC7747DCDF3E73C677BFEC0F6B2DB1F905BC222E43B7496CE7EF7117C2A5` |
| `tools/Run-FrontendAudioTests.ps1` | `9DB10A03EAA592D518938FAD419F84595FF804773F95AA1EFDDDF39B8061E86A` |
| `tools/Run-GameplayAudioTests.ps1` | `24427E142A22A8A32657880B97F064958057ADC65B4216A50849B3944F16FCE2` |

After GameplayAudio regenerates
`build/proof/r4-audio-foundation/proof-manifest.txt`, the wrapper requires the
current expected-parent container and stable audio identities:

| Evidence | Accepted identity |
| --- | --- |
| Current expected-parent full shared pack SHA-256 | `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29` |
| TMUS-1 | 36,784 bytes / FNV-1a32 `05C00ECB` |
| TFSX-1 | 1,792 bytes / FNV-1a32 `985DC7ED` |
| TSFX-1 | 2,824 bytes / FNV-1a32 `968A5DE6` |
| TDMC-1 | 2,515 bytes / FNV-1a32 `AD70E6E8` |
| WAV SHA-256 | `57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B` |
| Events SHA-256 | `3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA` |
| CLI manifest SHA-256 | `47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298` |
| Waveform CSV SHA-256, both runs | `76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2` |
| Waveform SVG SHA-256, both runs | `6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E` |

The earlier accepted isolated/combined R4A full-pack checkpoint remains
`8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`.
That value is preserved as historical integration-tip evidence; it is not the
full-pack golden for expected parent `f1b04193405d1c87f21e80ee51d3790499ea0cf8`.
Later accepted integration added non-audio asset-pack content, so the complete
container identity is tip-specific. This proof makes no narrower claim about
which later entry or byte accounts for the delta. The four audio payload
sizes/FNVs and all audio WAV/events/CLI/waveform identities remain unchanged.

## Clean authoritative checkpoint

The authoritative full wrapper passed from a clean worktree at signed commit
`e68671f0087276b8374fee5144a716a7dfa57905`. Its `-Build` phase produced the CLI
and game executable; Music, FrontendAudio, and GameplayAudio all passed. The
FrontendAudio result used a valid canonical decompilation root and therefore
included the real native `--flow-test` route execution.

| Checkpoint evidence | SHA-256 / value |
| --- | --- |
| Route classification | 5 proven / 13 source-present-only / 7 unproven |
| Route ledger | `4A9664BD56CDEF6EE9B994F5834900367B13A6BD80A17A6F30A82FD281AD7DEB` |
| Ignored route manifest | `6F3D0D40AB7946B3A1DA695808EC8615BC2881E9F113D1F427537FC52095DFD6` |
| Ignored foundation root manifest | `A681164E7C37864AEC6CD1DD88047DF2F374C308C7CAE1692B8B4E036A5E018E` |
| Proof generation HEAD | `e68671f0087276b8374fee5144a716a7dfa57905` |
| Expected parent | `f1b04193405d1c87f21e80ee51d3790499ea0cf8` |

The route ledger and both manifests carry the HEAD binding; the route ledger
and route manifest also carry the expected-parent binding. The worktree was
clean after the run and only ignored proof artifacts existed. No private path
is evidence.

## Final committed-HEAD evidence

The complete wrapper was rerun after the documentation commit at exact signed
HEAD `8e58aa40f669e9f54155593b49b1e22638394111`. Music, FrontendAudio with the
real native `--flow-test`, and GameplayAudio passed again. The final outputs
are:

| Final evidence | SHA-256 / value |
| --- | --- |
| Route classification | 5 proven / 13 source-present-only / 7 unproven |
| Route ledger | `C3D15BB68D5A9ACF0AE4A58E8E962AB21EB1217BEA6DC70E1F824C0AFBE0BCEC` |
| Ignored route manifest | `D6ABBABD69CBAB5CDF5F5727F04EB3AD8847DC36CA5216049B378FEB390311B9` |
| Ignored foundation root manifest | `A87CF0D8C234B43FD2C28FC4C9ABFA77FE57595D272BE10A10F1B26331414F2D` |
| Proof generation HEAD | `8e58aa40f669e9f54155593b49b1e22638394111` |
| Expected parent | `f1b04193405d1c87f21e80ee51d3790499ea0cf8` |

The final route ledger and both manifests were independently rehashed from the
ignored proof root, and Git remained clean. Independent in-session terminal QA
`/root/audio_terminal_qa` accepted the exact five-path candidate with
P0/P1/P2/P3 all zero. The thread-backed Luna service attempts timed out before
creating any task, so no missing, duplicate, or replacement task is implied.

The wrapper also requires the foundation proof to name its current committed
HEAD. That proof establishes native programs, PCM/state behavior, mailbox
ordering, mixed override, DMC independence/held-DAC continuity, and portable
output transactions. It does not by itself show that every production scene
caller fires.

## Proven routes

These five routes are executed by the native `--flow-test` because the wrapper
requires and passes `-DecompRoot` to the frozen FrontendAudio suite:

1. Normal license-to-arena handoff queues opening music ID 7.
2. Title setup preserves opening state through frame 4 and hard-stops it on
   frame 5.
3. A fresh second START queues frontend SFX 10 exactly once on confirmation
   frame 1.
4. Confirmation remains in title through frame 126; frame 127 queues
   presentation music ID 6 and enters the blue menu.
5. An accepted Player 1 A release queues frontend SFX 8 exactly once. The flow
   negatives cover reveal, START, directions, B/cancel, held input, suppressed
   release, and relevant chord paths.

The frozen Music and FrontendAudio gates additionally validate the requested
programs, timing metadata, deterministic PCM, strict same-pack dependencies,
and source mutations. The classification still stops at the production route
that the flow command actually exercises.

## Source-present-only routes

The following read-only native callers exist at the pinned source identities,
and their target APIs/programs pass the foundation suites. The allowed route
proof commands do not execute these callers:

| Route | Frozen request | Honest boundary |
| --- | --- | --- |
| Gameplay-scene entry | music ID 8 | pregame-matchup caller exists; no scene-entry execution in this runner |
| Pre-tip to live handoff | gated music ID 5 | caller and future-ID-5 gate exist; no production handoff execution |
| Qualifying violation/foul/period/free-throw restarts | music ID 5 | caller conditions exist; free-throw entry and live return stay distinct |
| Halftime/final event | music ID 6 | MUSIC_REQUEST caller exists; no completed-game execution/capture |
| Shot/period expiry | SFX 3 | event caller and event-to-ID mapping exist; no production event execution |
| Late-clock boundary | SFX 14 | event caller and mapping exist; no full-game countdown execution |
| Violation/foul presentation | SFX 6 | numeric presentation caller exists; meaning is bounded/incomplete/unknown |
| Scoring result | SFX 11, then clock-gated 12/13 | transactional callers and last-write-wins engine exist; no universal scoring claim |
| Qualifying restart | SFX 5 | caller exists, but effect 5 stays neutral |
| Held-ball/dribble trigger | DMC 4 | TGBD-triggered caller and mapping exist; complete caller scheduling is not claimed |
| State-15 diagnostic repeats | DMC 0 | bounded diagnostic caller exists; ID 0 remains address-bound |
| Current dunk frame-87 seam | DMC 2 | A9C5 caller exists; ID 2 remains address-bound with no inferred meaning |
| Win32 output loop | music plus selected frontend/gameplay source | init/select/service/shutdown source exists; no device capture is produced |

Pinned read-only source SHA-256 values used by the gate are:

| File | SHA-256 |
| --- | --- |
| `src/tecmo_flow_test.c` | `39AF8AB830B0D3546A9D168BECE38E6F39DAC09E593C6BF3A9A2DA5E465AB8FA` |
| `src/tecmo_game.c` | `67F9E8F8C31DBD95BD75E0F197E527A8ADEBBA0A6C8945525D15CA9D8D063AA2` |
| `src/tecmo_gameplay_scene.c` | `67F4B71A85274D8C2B4668F1141C71B817F667A63BE263A218838705D8AC01F6` |
| `src/tecmo_gameplay_scene_shots.c` | `5E1E835A852E16E3BF489287A1599D8A0505A305E0AC38572C3DB0F379270673` |
| `src/win32_platform.c` | `10043B4E2D28F5114B717643216FD748F2DB0AE88EB35D14FB3690A005F8AB45` |
| `include/tecmo_gameplay_audio.h` | `7301838A8293F86C9AC5CE98108632DFFAFCEEEC20D143F09C4BD019BBE36815` |

These hashes prevent a later source change from inheriting this ledger
silently. Hash agreement is source identity, not runtime execution.

## Unproven boundaries

The ledger preserves these explicit gaps:

- DMC ID 1 / A8D6-long has no production queue in the frozen gameplay-scene
  sources.
- DMC ID 3 / ABF5 is imported with bounded sequence-level correlation but has
  no production queue in those sources. It is not an impact/rim claim.
- DMC IDs 0, 1, and 2 remain address-bound and unresolved. Numeric identity,
  PCM, and bounded callers do not prove semantic meaning or exclusivity.
- Effect 5 has no proven semantic name and remains `BANK05_9FEC_CUE`.
- Effect 6 has only numeric/bounded presentation correlation. Its complete
  meaning, exclusivity, and original caller ordering remain incomplete and
  unknown.
- Nonlinear/cycle-exact APU mixing, DMC bit/cycle reader phase, IRQ behavior,
  and real-device output parity are not proved.
- There is no complete opening/menu/gameplay/halftime/final production capture
  and no proof that every current source-present caller fires end to end.

The generated `route-ledger.json` is the machine-readable form of this exact
classification. A later signed production task must provide concrete
source/capture mapping before any unproven or source-present-only row can be
promoted.
