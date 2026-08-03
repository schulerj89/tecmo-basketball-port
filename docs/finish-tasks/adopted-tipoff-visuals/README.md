# Adopted Tip-Off Visual, HUD, Facing, and Input Proof Work

This master-authored adoption record is grounded in Sol Max thread
`019fc081-44bd-7422-baac-3030f5356dba` and its signed reports.

## Scope and non-goals

Accepted scope: readable two-player tip jump; clean screen edges; correct
pre-tip/live HUD; position-derived inward contest facing and TGOR restoration;
fast Win32 X pulse preservation; deterministic unassigned-team tip sampling;
and production-path proof.

Non-goals: ROM-exact trajectory, exact CPU automatic-tip timing, or original
winner/claim settlement.

## Evidence and implementation summary

- TPTI selectors identify the competing actors; Bank05/Bank04 spans pin input,
  update, actor, pose, and source-anchor contracts.
- The original 30-update presentation was too short to read in real time. Input
  remains 30 updates; presentation uses an explicitly native 60-update arc.
- Invalid/incorrect live-band team selectors produced unrelated right-edge CHR.
  Valid `$40-$5A` selectors and render preflight remove corruption.
- Pre-tip HUD gating was corrected to show real team/player data during the
  ball-descent/contest phases.
- Contest facing derives from actual left/right anchors; live landing restores
  goal-derived TGOR orientation.
- Win32 fast down/up events now create one bounded update pulse. Physical X is
  NES B; literal B remains unmapped.

Pinned source evidence includes Rev 1 SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`,
Bank05 `$985B-$988E`, `$985E-$986A`, `$98E1-$9A5F`, Bank04
`$AC76-$AD81` and `$AD82-$ADDF`, and the 5,888-byte TPTI payload FNV32
`99ADFE3D`. Exact addresses prove contracts, not the native presentation curve.

Principal commits changed pre-tip asset import/source maps, state/update and
scene adapters, actor rendering/facing, HUD gating, Win32 key state, render
checkpoint/proof tooling, tests, and fidelity documentation.

## Luna lineage and revision history

Initial visual work:

- `019fc08f-ed6c-7ea0-970a-5f201afa68b3`
- `019fc08f-f31e-7e51-beac-761780fc2bca`
- `019fc08f-f833-7893-86c6-8b8176dc0ca9`
- `019fc08f-fdb0-7d00-9088-e95f8c0e79e7`

Real-time HUD/timing/facing hardening:

- `019fc3da-1baf-76a3-9e27-b69ba3d443e8`
- `019fc3da-271f-7fb0-89dd-c55a42d6dadd`
- `019fc3da-271f-7fb0-89dd-c53d25270b7b`
- `019fc3da-dd56-70e3-a1eb-79573d2fe83e`
- `019fc42f-80cf-72a2-aa2b-c670f2ea33dc`

Fast input/CPU/proof follow-up:

- `019fc471-8e41-7ea3-bd33-6486e0d52dff`
- `019fc471-d299-7710-8490-93b3d50ef2cd`
- `019fc48f-badc-7611-b3ed-21a826d38876`
- `019fc4b6-9c4f-71e0-80c1-6081cc3f1e60`

Four full duplicate/raced sessions are recorded in `sessions.json` as replaced;
partial IDs reported by the Sol are retained as raw recovery notes rather than
invented thread IDs. No duplicate code was integrated.

## Tests, proof, and Sol inspection

The Sol signed 27/27 top-level suites, focused controls/pre-tip/scene/camera/
orientation/render checks, warning-clean rebuilds, and Win32 launch smoke. It
personally inspected all full-resolution frames, both edge sheets, contact
sheets, decoded MP4, and the deployed GUI.

Canonical accepted proof:

- `build/proof/tipoff-main-9979b13/proof-manifest.json`
- `build/proof/tipoff-main-9979b13/tipoff-stage-contact-sheet.png`
- `build/proof/tipoff-main-9979b13/tipoff-sequence-0661-0725.mp4`

Reproduce with `tools/New-TipoffVisualProof.ps1` using the supported local Rev 1
ROM and a clean validated asset pack; the exact invocation, base/final SHA,
asset fingerprint, frame numbers, inputs, diagnostics, and artifact hashes are
inside the proof manifest. Frames 661-725 include input, CPU sample, apex,
landing, and live handoff.

## Commits and merge record

- Visual feature head: `1caa64531ec53c657fed6a87a047631f76d93e64`
- Real-time HUD/facing head: `f6e1f3a8a56d58f2331377b2658166b3654347c7`
- CPU sample: `6e5d6492a500fe3e42967156c6ba81f8dbb34b55`
- Fast pulse: `494428a941918b5b948ef8b798f4cb782ca66b56`
- Production proof: `8ebf564d03b6bf9780dfc0df55907d5843624734`
- Main merge: `9979b136ef62530be78808101feb7f0d561b2dbd`

All are already represented in current main. Do not re-merge historical worker
branches. Remaining approximation: jump trajectory and automatic CPU sample
frame 8 are evidence-informed native policy, not original timing.

