# Ordinary shot animation mapping

The native ordinary-shot renderer uses a Bank05 sequence base plus the visible
phase in the low nibble of the animation byte. It does not execute the ROM.

## ASM boundary

- Before Bank05 `$842C`, actor state `$1E` (`$83E9-$842B`) owns the gather and
  turn. Its first `30/20/10/00` cycle preserves the actor's entry pose. Later
  cycles select facing-indexed records from `$8469-$847A`; consequently the
  bounded `325`, `1060`, and `1061` poses are legitimate only in this setup
  context and are not universal committed-shot frames.
- At `$842C`, Bank05 computes
  `family * 16 + profile * 8 + direction`, loads `$8D3D/$8D5D[index]` into
  `$0442/$044D`, writes `$0458=$31`, changes the actor to state `$0B`, and
  starts the shot-owned ball state. The 32 imported TGJS bases are this exact
  selector matrix.
- Bank07 `$F1E8-$F204` (the listing around line 6988) adds `$A5B9` to the
  selected byte offset, masks `$0458` to its low nibble, doubles that phase,
  and reads the selected sprite pointer. TGJS stores the byte-offset base as a
  TGPL pointer index, so native C resolves `final_pose = sequence_base + phase`.

## Committed sequence

The imported constants and Bank05 `$8999`/states `$0B-$0E` retain the animation
byte schedule. The renderer-visible low nibbles are sequence phases 1 through
3 during preparation, phase 4 while held, phase 5 for release/airborne, and
phase 6 for landing/recovery before the actor returns to its neutral movement
sequence. High-nibble countdown changes do not change the visible pose. The
port therefore updates the final TGPL pose whenever a committed transition
changes the low nibble, while leaving shot outcome and ball physics untouched.

## Deterministic proof

Build a local asset pack and run:

```powershell
.\build\tecmo_port.exe --gameplay-jump-shots-test build\tecmo.assetpack
.\build\tecmo_port.exe --gameplay-scene-test build\tecmo.assetpack
```

The first command emits machine-readable `TGJS_PHASE_TRACE` records containing
family, profile, direction, selected base, phase, and final pose. It also
checks every phase `0..7` for all 32 family/profile/direction bases. The scene
test exercises hold/release, airborne, landing/recovery, makes, misses, both
baskets, the full 32-selector matrix, and unchanged close-shot/tip-off tests.

Reproduce the visual checkpoints with:

```powershell
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame5  build\test-artifacts\shooting-animation\ordinary-shot-gather-frame05.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame9  build\test-artifacts\shooting-animation\ordinary-shot-release-frame09.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame20 build\test-artifacts\shooting-animation\ordinary-shot-airborne-frame20.png
.\build\tecmo_port.exe --render-test-mode gameplay-jump-make-frame58 build\test-artifacts\shooting-animation\ordinary-shot-recovery-frame58.png
```

Frame 5 is the bounded pre-commit turn. Frames 9, 20, and 58 show the selected
ordinary-shot sequence at release/preparation, airborne phase 5, and recovery
phase 6 respectively.
