# Tecmo R4 audio cue-routing proof

Status: proof-first candidate prepared; terminal execution and independent QA
remain pending.

Task/session/claim: `R4-AUDIO` /
`S-SOL-R4-AUDIO-CUE-ROUTING-001` / `OWN-R4-AUDIO-PROOF-FIRST`.

This slice adds a proof gate and an honest route ledger. It does not change a
production caller, gameplay module, importer, build file, source map, audio
engine, or device path. Consequently it does not close `ACC-AUDIO` and does
not manufacture a route that the accepted evidence does not execute.

The entry point is `tools/Run-AudioRouteProofTests.ps1`. It:

1. requires the exact Rev1 ROM and a valid decompilation root so the frozen
   FrontendAudio script really runs its native `--flow-test` checks;
2. requires a clean candidate descended from the exact task parent, with an
   exact five-path proof-only diff;
3. pins the three accepted Music/FrontendAudio/GameplayAudio script SHA-256
   identities plus the read-only route/evidence source identities;
4. runs the three scripts without replacing or weakening any suite;
5. parses the regenerated ignored R4A foundation proof manifest and requires
   every accepted pack, payload, WAV, event, CLI-manifest, and waveform hash;
6. writes a deterministic ignored route ledger and short manifest below
   `build/proof/r4-audio-route/`.

## Classification rule

The ledger uses exactly three labels:

- `proven`: an allowed frozen terminal command executes the production route
  and asserts its result.
- `source-present-only`: the frozen native caller exists and its target
  program/cue API is validated, but these allowed commands do not execute that
  caller.
- `unproven`: the production queue is absent or the semantic, cycle, device,
  or end-to-end evidence is missing.

This yields 5 proven routes, 13 source-present-only routes, and 7 unproven
boundaries. The five proven routes are limited to the normal frontend flow:

| Route | Request | Why proven |
| --- | --- | --- |
| License-to-arena handoff | music ID 7 | native flow observes pending opening track 7 at the normal handoff |
| Title setup frame 5 | music stop | native flow proves unchanged state through frame 4 and hard stop at frame 5 |
| Fresh title confirmation frame 1 | frontend SFX 10 | native flow observes exactly one request |
| Title confirmation frame 127 | music ID 6 | native flow proves the frame-126 hold and frame-127 blue-menu handoff |
| Accepted Player 1 A release in the blue menu | frontend SFX 8 | native flow proves accepted requests plus rejected input cases |

Gameplay, halftime/final, and Win32 callers remain source-present-only under
this runner. Their presence is not promoted to execution proof. DMC IDs 1 and
3 have no production queue in the frozen gameplay-scene sources, so those
routes remain unproven.

## Non-negotiable limits

- No nonlinear or cycle-exact NES APU parity is claimed.
- DMC reader bit/cycle phase and IRQ behavior are not claimed.
- DMC IDs 0, 1, and 2 retain their exact numeric identities and remain
  address-bound/unresolved.
- Effect 5 remains the neutral `BANK05_9FEC_CUE`; it is not renamed.
- Effect 6 remains numeric/bounded, incomplete, and semantically unknown
  beyond the accepted correlation.
- No real-device capture or complete opening-to-final gameplay audio capture
  is produced.

A later production rescope requires a concrete source/capture mapping and a
separate signed assignment granting the affected production paths. See
[EVIDENCE.md](EVIDENCE.md), [COMMANDS.md](COMMANDS.md), and
[LINEAGE.md](LINEAGE.md).
