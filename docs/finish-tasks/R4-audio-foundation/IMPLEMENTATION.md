# R4 Audio Foundation implementation contract

Revision C is a terminal proof/documentation revision on top of the native
audio implementation. The worktree is
`C:\Users\joshs\Projects\tecmo-basketball-port-r4-audio-foundation-luna`,
branch `codex/r4-audio-foundation-luna`, based exactly on
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`. Sol acceptance is pending.

## Owned runtime APIs

| Area | Owned implementation and contract |
| --- | --- |
| Output | `tecmo_audio_output_init`, `tecmo_audio_output_render_samples`, service/refill helpers, backend lifecycle, routing validation, and portable transaction self-tests in `src/tecmo_audio_output.c` / `include/tecmo_audio_output.h`. |
| Music | `tecmo_music_player_*`, `tecmo_music_queue_track`, `tecmo_music_queue_opening_once`, direct renderer guards, cadence/loop/termination state in `src/tecmo_music.c` / `include/tecmo_music.h`. |
| Frontend audio | `tecmo_frontend_audio_*`, TFSX-1 parser/player, direct renderer guards, source and same-pack dependency checks in `src/tecmo_frontend_audio.c` / `include/tecmo_frontend_audio.h`. |
| Gameplay audio | `tecmo_gameplay_audio_*`, TSFX-1/TDMC-1 parser/player, DMC relational/queue checks, direct renderer guards, and source validation in `src/tecmo_gameplay_audio.c` / `include/tecmo_gameplay_audio.h`. |
| Pack import | `tecmo_asset_pack_build_music`, `tecmo_asset_pack_build_gameplay_audio`, and `tecmo_asset_pack_build_frontend_audio`, with checked public offsets, exact Rev1 postconditions, and the `>=8` PRG-bank compatibility contract. |
| CLI/proof | Hidden developer-only audio identity/proof commands in `src/tecmo_cli_audio.c`; no shared CLI help or call-site changes. |

The importer arithmetic validates declared PRG size and bank count before fixed
bank computation, checks addition/multiplication in public-offset paths, and
fails without output on malformed, zero-bank, undersized-bank, or UINT64_MAX
builder inputs. Canonical postconditions are TMUS 36,784 bytes / `05C00ECB` /
2,251 instructions / 37 voices; TFSX 1,792 / `985DC7ED` / 87 instructions / 3
voices; TSFX 2,824 / `968A5DE6` / 131 instructions / 14 voices; and TDMC 2,515
/ `AD70E6E8` with five clips and three pools.

## Output transactions and player behavior

- Initialization snapshots the initial music-player state before the eight
  prefill renders. A prepare/write failure resets the backend and restores the
  complete initial state.
- Each accepted service refill commits its snapshot of every advancing borrowed
  source: music plus the selected gameplay/frontend source, including the
  frontend embedded-SFX alias. A rejected refill restores player state while
  leaving previously accepted refills queued and drainable.
- Validation can detach invalid borrowed pointers; rollback restores mutable
  player state, not the routing object or invalid pointers. A test explicitly
  invalidates a previously selected gameplay source and verifies rollback plus
  detached routing.
- The portable private seam covers failed prepare, initial write, and refill
  equivalents without a real device; successful submission advances and failed
  submission freezes.
- Every public direct renderer guards `sample_count` against
  `SIZE_MAX/sizeof(int16_t)` before touching a destination or advancing state,
  including NULL-sink tests.
- `tecmo_music_queue_opening_once` latches only after a successful queue; a
  genuinely missing/null asset can fail and then retry with the valid asset.
- Gameplay and frontend selections against music require canonical same-pack
  identity. The hidden real-pack identity gate accepts a canonical alias and
  rejects a byte-identical distinct canonical container while preserving
  selection state.

The native model preserves TMUS IDs 5–8, TFSX 8/10, TSFX 3/5/6/11/12/13/14,
last-write-wins mailboxes, matching-channel SFX override while music advances,
future-track-5 game-music gating, clean track 7/8 termination, and DAC/output
continuity through retrigger/end/clear. It does not claim nonlinear/cycle-exact
NES APU mixing or DMC reader phase.

## Importer and proof tools

`--gameplay-audio-source-test ROM` requires the exact Rev1 layout and full SHA,
then calls only the gameplay-audio importer and validates TSFX/TDMC outputs.
Gameplay-owned mutations fail through gameplay revision/source validation before
the final full-ROM SHA path; the Bank06 `$A145` music-owned mutation is kept in
broad asset-pack coverage because it is intentionally outside that isolated
gate.

The hidden `--audio-pack-identity-test MUSIC_PACK GAMEPLAY_PACK EXPECTATION`
command initializes both players and a device-free output object, then tests
canonical alias acceptance and distinct-container rejection. The hidden
`--audio-proof PACK OUTPUT_DIR` command emits deterministic private evidence:
44.1 kHz mono 16-bit little-endian WAV, fixed event/state records, and a
semantic manifest. `Run-GameplayAudioTests.ps1` runs it twice, requires golden
full-file identities, exact event header/field order, vector state/order/coverage,
per-vector WAV-slice FNV-1a32, exact RIFF layout, two-run waveform identity,
repository provenance, and path-free ASCII/LF manifest output.

Generated pack, WAV, event, waveform, and manifest files remain ignored under
`build`; no generated evidence is part of the runtime or tracked history.

## Explicit boundaries

No cue call sites, shared source-map/import-layout, CMake/build, Win32/device
platform, root README, PORTING, or AGENTS file was modified. Cross-domain cue
routing and full ACC-AUDIO integration are deferred. The implementation is a
native semantic port with exact-high declared contracts, not an emulator wrapper.
