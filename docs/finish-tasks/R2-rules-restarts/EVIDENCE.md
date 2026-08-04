# Evidence and implementation binding

## Canonical input

Read-only research/test ROM:

- `Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`
- length: 393,232 bytes
- SHA-256:
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

No ROM bytes or derived proprietary payloads are committed.

## Source-backed seam

TPNL-1 is a strict 768-byte semantic asset with FNV-1a32 `980DDC76`. Its eight
revision-locked source roles include Bank04 `$BA1F-$BA3E`, 32 bytes, FNV-1a32
`F56AD5D8`, identified neutrally as the shared screen-`$22` delayed SFX-6
request. TPNL-1 binds both foul and violation presentation records to
`presentation_sfx_id=6` and `presentation_sfx_delay_frames=16`.

Relevant bounded provenance is:

| Contract | CPU span | Classification and limit |
| --- | --- | --- |
| Foul commit neighborhood | Bank05 `$9571-$9649` | Exact bytes; commit boundary, not live subtype/contact proof. |
| Foul rules/presentation | Bank02 `$B0F8-$B398` | Strict TPNL rule/presentation input. |
| Foul presentation script | Fixed `$E95E-$EA11` | Presentation/wait source; not a cycle-exact C schedule claim. |
| Presentation release | Fixed `$EA14-$EA2F`, helper `$D2B9-$D2CE` | Four-frame lead-in and NES-A release semantics. |
| Violation presentation | Fixed `$EC5B-$ED14`; Bank03 `$BE87-$BFA8` | Strict violation dispatch/selector evidence. |
| Shared presentation cue | Bank04 `$BA1F-$BA3E` | Exact delayed shared SFX-6 request metadata. |
| Audio clear | Fixed `$EC06-$EC25` | Existing clear boundary; dispatch policy preserved. |
| TGBC | Bank05 `$970B-$9786` | Strict backcourt source; implemented semantic span `$971F-$9786`. |
| TGOR | Bank05 `$8FAD-$8FE7`, `$9042-$9053`, `$9054-$90AF`, `$BDEF-$BDF2` | Existing possession/orientation dependency, unchanged. |
| TGMO | Fixed `$F10C-$F1B0` | Existing holder/boundary latch dependency, unchanged. |

TGVR separately retains the screen, text, controller, gesture, and metasprite
sources. Those assets prove bounded referee data and cadence; nine-frame
blackout/fade alignment remains capture-bounded and outside this change.

## Native binding

`scene_process_phase_audio` now selects the presentation kind from the current
phase, uses selector 0 for foul presentation and the current numeric violation
selector for violation presentation, calls
`tecmo_gameplay_penalties_get_presentation`, and queues the returned SFX ID only
at equality with the returned delay.

This is classified native-faithful for the supported asset/scene boundary:
source-backed semantic metadata is consumed without embedding ROM bytes or
inventing a new semantic route. It is not a claim that the complete original
6502 caller order or presentation renderer is cycle-ported.

## Preserved audio distinction

Shot-clock expiry already requests SFX ID 3 when expiry creates the violation at
presentation frame 0. Phase entry also clears prior music, tonal SFX, and DMC
through the existing reset policy. Tests consume SFX 3, prove no pending SFX on
frames 1-15, observe SFX 6 at frame 16, consume it, and reject further requests.
The production dispatch order was not edited.

## Proof boundary

The deterministic scene proof is supporting native integration/render evidence,
not visual ROM-parity proof. The manifest remains `DRAFT` because the shared
runner's optional `-RequirePass` identity gate is pinned to a different R1 task;
the R2 suite itself reports `GAMEPLAY SCENE TEST PASS` and
`suites_complete=True`.
