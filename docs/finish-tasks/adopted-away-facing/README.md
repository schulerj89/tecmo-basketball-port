# Adopted Goal-Relative Away-Left Facing Work

This master-authored adoption record is grounded in Sol Max thread
`019fc4e9-55c8-7d62-904f-30d0974d9c6f` and its signed report.

## Scope and non-goals

Accepted scope: correct the production renderer's extra mirror when an actor's
authored pose already carries the goal-baseline polarity. Preserve action,
pre-tip, mixed-pose, failed-lookup, and explicit movement overrides.

Non-goals: CPU logic, tip timing, camera, rules, shots, menus, or animation
families beyond the reported goal-left polarity defect.

## Root cause and implementation

The renderer applied `!actor->facing_right` as another horizontal mirror to
every non-encoded pose. Away's left-goal movement pose already had OAM flip bit
`$40`; the second mirror cancelled it. The focused change in
`tecmo_gameplay_scene_render.c` reconciles authored polarity only while the
actor matches its assigned TGOR goal baseline. Rendered-pixel regressions cover
Away left/right, Home right, held-left movement override, and encoded actions.

## Original-game/ASM evidence

High-confidence evidence:

- Fixed compositor `$D413/$D498`; `$D503 AND #$41` preserves flip bit `$40`
- Bank05 `$8F47/$8F57`: Away orientation-0 raw `$012A` resolves through pose
  149 and `$A6E3/$884E` to four `$41` attributes
- Home raw `$016A` resolves through pose 181 and `$A723/$8702` to `$03`
- Original frame 4100 shows three white Away metasprites at OAM sprites 34-51,
  all with `$41`

The Sol explicitly did not claim original runtime pose `$0070` equals native
pose 149. ROM/OAM/captures remain private research evidence only.

## Luna lineage and review

- Luna Max thread: `019fc4f0-7a8e-7113-b682-ff1c0055f91a`
- Actual title: `Tecmo Away Facing Left Sprite Fix - Luna Max`
- Luna commit: `b96f9cb0437e63a4e613f6afedadb62848c1320b`
- Sol integration commit: `f26da5889f16bba5aa7b62eabf25932ff732c437`
- Final documentation/main commit: `63b29b04b1ab4745b7b8d5dd0499942d1bf8ba4e`

The Sol personally reviewed the patch, original OAM/frame evidence, before and
after full-resolution frames, magnified crops, contact sheet, MP4, and live
shortcut build. A later requested walking-cycle check found no additional
defect; the current eight-frame Away-left walking/dribble cycle already matches
the original, so no extra mirror was committed.

## Tests and proof manifest

Sol sign-off: warning-clean build; focused orientation/scene/pre-tip/movement
checks; all 27 runners; Win32 launch smoke; supported private reference gate.

Accepted proof is preserved in main's ignored local proof tree:

- `build/proof/away-facing-left-only/manifest.json`
- `build/proof/away-facing-left-only/reproduce-native-proof.ps1`
- `build/proof/away-facing-left-only/after/native-after-away-left.png`
- `build/proof/away-facing-left-only/after/native-after-sequence-contact.png`
- `build/proof/away-facing-left-only/after/native-after-away-left-live-0721-0725.mp4`
- `build/proof/away-facing-left-luna/manifest.json`

The manifest SHA-256 reported by the Sol is
`0BD6308217A578F7D822EA8CA7F69F1EC9F41CFFCB5620B44E98CFF1A5A491E1`.
The reproduction script records the supported ROM, asset pack, Bulls Away vs
Pacers Home scenario, real Win32 input bridge, resolution, frames 721-725, and
artifact hashes.

## Merge instructions and limits

The Luna and Sol product commits are already ancestors of current main. The
historical integration worktree/branch were removed only after proof was
preserved. Do not re-merge `b96f9cb` or `f26da588`.

This task accepts the goal-left rendering defect only. It does not certify all
directional/action animation families; those remain in the Round 2 presentation
matrix.

