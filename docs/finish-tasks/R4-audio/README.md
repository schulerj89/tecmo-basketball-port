# Tecmo R4 audio cue-routing proof

Status: terminal proof-first slice accepted at signed commit
`8e58aa40f669e9f54155593b49b1e22638394111`; the final committed-HEAD rerun
and independent terminal QA both passed with P0/P1/P2/P3 all zero.

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
5. parses the regenerated ignored foundation proof manifest and requires the
   current expected-parent full-pack hash plus every stable audio payload, WAV,
   event, CLI-manifest, and waveform hash;
6. writes a deterministic ignored route ledger and short manifest below
   `build/proof/r4-audio-route/`.

## Authoritative terminal checkpoint

The authoritative Sol personally reviewed signed commit
`e68671f0087276b8374fee5144a716a7dfa57905` and ran the complete wrapper from a
clean worktree with `-Build` and a valid canonical decompilation root. The CLI
and game executable built, Music passed, FrontendAudio passed with the real
native `--flow-test`, and GameplayAudio passed. The exact wrapper result was:

```text
AUDIO ROUTE PROOF PASS: proven=5 source-present-only=13 unproven=7 ledger=4A9664BD56CDEF6EE9B994F5834900367B13A6BD80A17A6F30A82FD281AD7DEB
```

The ignored route manifest SHA-256 was
`6F3D0D40AB7946B3A1DA695808EC8615BC2881E9F113D1F427537FC52095DFD6`; the
ignored foundation root manifest SHA-256 was
`A681164E7C37864AEC6CD1DD88047DF2F374C308C7CAE1692B8B4E036A5E018E`.
The route ledger and both manifests bind `proof_generation_head` to that
checkpoint. The route ledger and route manifest additionally bind
`expected_parent` to `f1b04193405d1c87f21e80ee51d3790499ea0cf8`.

Those identities describe the precursor `e68671f` checkpoint. The required
final committed-HEAD rerun subsequently passed at signed commit `8e58aa4` with
route-ledger SHA-256
`C3D15BB68D5A9ACF0AE4A58E8E962AB21EB1217BEA6DC70E1F824C0AFBE0BCEC`,
route-manifest SHA-256
`D6ABBABD69CBAB5CDF5F5727F04EB3AD8847DC36CA5216049B378FEB390311B9`,
and foundation-manifest SHA-256
`A87CF0D8C234B43FD2C28FC4C9ABFA77FE57595D272BE10A10F1B26331414F2D`.
All three generated records bind `proof_generation_head` to exact `8e58aa4`;
the route records retain expected parent `f1b0419` and the current full-pack
SHA-256 `27D4CEB4...E1CA6B29`.

Thread-backed Luna task creation timed out without creating a task. One
independent in-session terminal reviewer, `/root/audio_terminal_qa`, therefore
performed the terminal static, proof, classification, and clean-scope audit and
returned PASS with P0/P1/P2/P3 all zero. No product, runner, or proof claim was
widened by this service fallback.

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

The current expected-parent full shared pack is pinned at SHA-256
`27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`.
The earlier accepted R4A checkpoint
`8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`
remains historical evidence, not the current full-pack golden. Later accepted
integration added non-audio pack content; this proof does not attribute the
container delta more narrowly.

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
