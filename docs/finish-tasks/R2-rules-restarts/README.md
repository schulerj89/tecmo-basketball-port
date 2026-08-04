# Tecmo R2 Rules and Restarts

Status: the bounded implementation candidate is Good-SSH-signed and independently
accepted. Candidate `1dde1ef748658a11403ff4bc450af858d05f08c2`, tree
`a46d403f1537583b55e7607cdde541bf1bf98dc4`, has sole parent
`7fe2dd772af1d88f704a9272005b4ba557434cac`. Independent terminal QA task
`019fcb44-0f91-7632-9b25-88e51b505ce3` returned `ACCEPT` with no P0-P3
findings.

This lane closes one deliberately narrow source-backed seam: foul and violation
presentation now request the strict TPNL-1 shared SFX ID 6 when the presentation
counter reaches metadata delay frame 16, exactly once. Shot-clock expiry SFX ID 3
at entry frame 0 remains distinct. Existing phase-audio reset and dispatch order,
qualifying restart SFX/music policy, possession settlement, clocks, TGBC, TGOR,
camera, shots, defense, frontend, and audio contracts are preserved.

The new deterministic scene matrix proves:

- foul and violation timing for both initial possessions;
- no SFX 6 before frame 16, SFX 6 at frame 16, and no repeat;
- out-of-bounds and TGBC backcourt restarts for both possessions/orientations;
- enabled and disabled GAME MUSIC routes;
- frozen presentation clocks and camera;
- one restart handoff with coherent holder, TGBC reset, TGOR serial/direction,
  camera projection, ball attachment, and event mailbox;
- inert live action on the restart frame.

The explicit foul fixture proves presentation/audio behavior only. It does not
prove a live foul detector, contact inference, subtype selection, or original ROM
caller timing.

## Exact implementation ledger

Candidate `1dde1ef` changes exactly these seven paths relative to its sole parent:

- `CMakeLists.txt` — one additive test translation unit;
- `build.ps1` — the matching additive source-list entry;
- `src/tecmo_gameplay_scene.c` — `scene_process_phase_audio` only;
- `src/tecmo_gameplay_scene_test_internal.h` — one prototype;
- `src/tecmo_gameplay_scene_test_orchestrator.c` — one invocation;
- `src/tecmo_gameplay_scene_test_rules_restarts.c` — new focused scene matrix;
- `src/tecmo_gameplay_scene_test_state_flow.c` — three signed, line-bounded stale
  timing corrections.

Production dispatch remains
`scene_apply_phase_audio_reset -> scene_process_events ->
scene_process_phase_audio -> jump-miss result audio`.

The later Sol documentation descendant changes no product/test/build behavior.
Under Good-signed control `6028f997`, it also corrects only the stale cue-status
passages in root `AGENTS.md`, root `PORTING.md`, and
`docs/gameplay-state-foundation.md`, alongside the seven files in this task-doc
directory.

## Evidence classification

- **Exact/ROM-derived asset evidence:** canonical Rev1 identity; strict TPNL-1
  source spans and metadata, including shared cue 6 with delay 16; strict TGVR,
  TGBC, TGOR, TGCP, TSFX, and TDMC provenance gates.
- **Native-faithful bounded integration:** metadata-driven equality dispatch at
  scene presentation frame 16, exact-once mailbox behavior, and the tested native
  restart/clock/orientation/camera/music contracts.
- **Native-approximate or incomplete:** broader live foul/contact ownership,
  unported violation detectors, original intra-frame 6502 caller ordering,
  TGVR blackout/fade alignment, and other exclusions listed in `SCOPE.md`.

No ROM, capture, trace, save state, decoded payload, proprietary artifact, or
generated proof is committed. The Sol documentation commit and its subsequent
same-QA signed-tip disposition are reported in the authoritative Sol/master
handoff because a commit cannot contain its own object ID or a later audit result.

See `SCOPE.md`, `EVIDENCE.md`, `TESTS.md`, `LINEAGE.md`,
`INDEPENDENT-QA.md`, and `FAULT-LEDGER.md`.
