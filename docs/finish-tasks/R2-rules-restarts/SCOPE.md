# Scope and semantic boundary

## Authorized behavior

The production change is confined to `scene_process_phase_audio` in
`src/tecmo_gameplay_scene.c`. While the scene remains in foul or violation
presentation, it retrieves the strict TPNL-1 presentation record and requests
that record's `presentation_sfx_id` only when `phase_frame` equals
`presentation_sfx_delay_frames`.

For the accepted Rev1 TPNL-1 asset, both presentation kinds bind SFX ID 6 at
delay frame 16. Equality and once-per-scene-frame dispatch make the request
exact-once without adding a new latch or changing phase-update order.

The runtime continues to consume validated semantic assets. ROM/decompilation is
research/test evidence only and is never a normal runtime dependency.

## Test-only surface

The focused translation unit covers four timing cases and eight restart cases:

| Dimension | Values |
| --- | --- |
| Timing kind | direct violation fixture; direct foul-presentation fixture |
| Initial possession/orientation | Away; Home |
| Restart detector | TGMO out-of-bounds; TGBC backcourt |
| GAME MUSIC | disabled; enabled |

The state-flow corrections retain the accepted shot-clock expiry SFX ID 3 at
frame 0, prove silence for presentation frames 1-15, consume SFX ID 6 at frame
16, and reject repeats through the existing release lead-in. The music-disabled
case also preserves the no-restart-SFX/no-track policy.

## Preserved dependencies and contracts

Accepted R2-SHOTS-OUTCOMES and R2-DEFENSE-CONTACT dependencies remain unchanged.
The implementation does not alter TIP/pre-tip, shot evaluation or settlement,
clocks/periods, defense/contact foundations, TGMO/TGBC/TGOR/TGVR/TGFL assets,
audio assets or mixer behavior, frontend behavior, or existing build logic beyond
registering the new test translation unit.

## Explicit exclusions and incompletes

This lane does not add or claim:

- live foul/contact inference, collision ownership, or new foul detectors;
- invented TPNL caller context or semantic labels;
- foul-divider selection or parity for the two observed divider outcomes;
- free-throw aim, release-quality, outcome, rebound, inbound, or final-possession
  changes;
- five-second, ten-second, traveling, goaltending, or other missing live
  violation detectors;
- rebound, claimant, recovery, block, or steal semantics;
- shot make/miss, contact, contest, score, or settlement changes;
- original blackout/fade/cycle parity or complete referee presentation parity;
- asset, importer, source-map, scene-art, tool, or proof-format changes;
- main, staging, merge-order, or push action.

The foul fixture calls the existing state request API directly. It is
presentation/audio proof, not detector proof.

Root `AGENTS.md`, root `PORTING.md`, and `docs/gameplay-state-foundation.md`
previously described the immediate SFX 6 implementation status. Because the
first two are active repository contracts, Sol stopped before signing and
obtained Good-signed control `6028f997`. Only those three exact passages are
corrected to the frame-16 native-faithful boundary; no other shared text is
changed.
