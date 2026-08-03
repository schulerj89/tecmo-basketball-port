# Implementation contract

## Transaction and routing behavior

`src/tecmo_audio_output.c` now owns a portable checkpoint seam for the same
borrowed mutable state used by Win32 waveOut:

- `checkpoint_capture()` snapshots music, valid selected gameplay, and valid
  selected frontend players, with pointer-alias protection.
- Initialization snapshots once before the eight-buffer prefill. A prepare or
  initial write failure restores that complete initial player checkpoint before
  entering silent fallback.
- Each service refill snapshots independently. Accepted writes commit; a
  rejected write restores only that refill. Previously accepted headers remain
  queued and drainable; shutdown owns reset/unprepare/close.
- Routing fields are never restored by a player checkpoint. If validation
  detaches an invalid gameplay/frontend selection, the pointers remain detached
  after a rejected refill while mutable music/player state rolls back.
- `gameplay_source_is_valid()` is rechecked at render time, so a path or asset
  mutation after selection cannot continue rendering through the gameplay path.

`tecmo_audio_output_select_gameplay_player()` and the frontend equivalent
require canonical asset-pack identity and preserve an existing selection on
rejection. The hidden `--audio-pack-identity-test` command independently loads
music from path A and gameplay from path B, uses a device-free initialized
`TecmoAudioOutput`, and proves canonical aliases accept while distinct
byte-identical containers reject and preserve the selected route.

## Direct render and queue contracts

- `tecmo_music_render_samples()`,
  `tecmo_gameplay_audio_render_samples()`,
  `tecmo_frontend_audio_render_samples()`, and the output renderer reject a
  count greater than `SIZE_MAX / sizeof(int16_t)` before touching the sink or
  advancing state. The self-tests cover sentinel and `NULL` sinks.
- `tecmo_music_queue_opening_once()` latches only after a successful queue.
  The self-test first uses a genuinely missing/null asset, then retries the
  valid asset.
- Gameplay event mapping is exhaustive for IDs 3, 5, 6, 11, 12, 13, and 14;
  unknown events reject. DMC queue/render relational checks use subtraction
  form for pool/data bounds and preserve the held DAC level across end,
  retrigger, and stop-all.

## Importers

`src/asset_pack/tecmo_asset_pack_music.c` uses a minimum declared PRG count of
7 for Bank06; the gameplay importer uses 6 for Bank05; frontend uses 5 for
Bank04. Checked add/multiply helpers and declared-PRG bank-range checks run
before source pointers. Direct source tests reject `UINT64_MAX`, zero-bank,
and importer-specific undersized-bank layouts. Enforced Rev1 builds assert the
exact TMUS/TSFX/TDMC/TFSX serialized sizes, FNV fingerprints, instruction
counts, and voice counts listed in [EVIDENCE.md](EVIDENCE.md).

`tecmo_asset_pack_gameplay_audio_source_test()` is deliberately gameplay-only:
it verifies exact Rev1 layout, runs only the gameplay importer, checks TSFX and
TDMC postconditions, then checks the full ROM SHA for canonical identity. The
owned gameplay suite distinguishes gameplay-local revision failures from
full-ROM-SHA-only failures.

## Proof exporter

`src/tecmo_cli_audio.c` contains the hidden developer-only
`--audio-proof PACK OUTPUT_DIR` command. It loads validated semantic pack
entries, initializes players without opening a device, and writes only to the
explicit output directory. It emits a deterministic 44.1 kHz mono signed
16-bit little-endian WAV, fixed-format event/state records, and a path-free
semantic manifest. The gameplay suite runs it twice, compares bytes and
SHA-256, and creates ignored CSV/SVG waveform evidence.
