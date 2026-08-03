# Adopted CPU Movement and Tip-Input Work

This is a master-authored adoption record of the final report from Sol Max
thread `019fc012-6da0-7410-8600-a6b89b402424`. It does not replace the Sol's
personal QA report and does not claim the broader CPU/tip fidelity criteria are
complete.

## Scope and non-goals

Accepted scope: restore sustained meaningful ordinary CPU movement and move
team-routed held NES-B sampling into the visible 30-update jump contest.

Non-goals: original high-level play selection, exact formation/marking policy,
exact original winner settlement, or exact jump timing/trajectory.

## Root causes and implementation

- Non-holder defenders targeted exact linked-opponent coordinates, converged,
  and then legitimately hit TGAI's ROM-grounded zero-vector/no-write gate.
- Held B was sampled during the earlier close-up rather than the visible
  `JUMP_CONTEST`, and the public winner query accepted premature phases.
- Native formation points, goal-side defender offsets, depth splits, shaped
  court reflection, visible-window sampling, team routing, and fail-closed
  winner gates were added/hardened.

Principal changed functions live in `tecmo_gameplay_cpu_steering.c`,
`tecmo_gameplay_scene_actors.c`, `tecmo_gameplay_pretip.c`, and
`tecmo_gameplay_scene.c`, with importer/source-map and focused regressions.

## Original-game and ASM evidence

Supported Rev 1 ROM SHA-256:
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

High-confidence CPU kernel/direction evidence, not high-level policy:

- Bank06 `$81F7-$82D3` FNV32 `23BB7271`
- `$87AE-$88AF` `F866B06C`; `$88DA-$8A95` `9616E586`
- `$8B90-$8BE0` `9AD2BA91`; `$8BE1-$9237` `344298FE`
- `$9280-$9329` `C82E6853`; `$938B-$9620` `47818A62`
- Fixed `$C006-$C008` `14B2472E`; `$CBE0-$CBF6` `41C5B5C8`
- Bank04 `$9F2E-$AC75` `71331A96`

High-confidence tip input evidence, not exact winner policy:

- Bank05 `$985B-$988E`: FNV32 `F372E57C`, FNV64 `096F86BBD7A42ABC`
- Exact held-B subspan `$985E-$986A`: FNV32 `423816F1`, FNV64 `032F8A7A4F4439D1`
- Update `$98E1-$9A5F`: FNV32 `0A2F945A`, FNV64 `6CB90FF6825E2A5A`
- Bank06 `$A0F4-$A124`: FNV32 `3E73FFEC`, FNV64 `1FE3F2EA43FEDF8C`

The runtime remains native C plus validated semantic assets; ROM/ASM and live
captures were research/test evidence only.

## Luna lineage and review history

Four rounds used these eight reported Luna Max top-level sessions:

- `019fc01c-730d-71c2-8a73-529db70b6163`
- `019fc01c-7881-7e11-8132-ae9fdb3b9a0f`
- `019fc028-f47e-7193-9766-8286894dcff7`
- `019fc028-f829-7270-b147-f25620ec9cfb`
- `019fc041-15d4-7e32-a3c7-911d225f3744`
- `019fc041-1b0e-7370-88ed-6c0186e16176`
- `019fc04b-106b-7fe0-8ba5-56928f288f0e`
- `019fc04e-e8c6-7de1-8892-c5b675afb117`

The Sol reported evidence audit, isolated implementation, independent review,
and final winner-gate/court-boundary correction rounds. Transient duplicate task
shells were stopped before edits or commits.

## Tests and proof

Sol sign-off reports targeted CPU, pre-tip, and scene tests; warning-clean GUI
and console builds; all 27 top-level test runners; Win32 launch smoke; and no
skipped private gate.

Sol personally exercised the production Win32 game and inspected:

- `build/visual-qa/native-tip-contest-held-x-desktop.png`
- `build/visual-qa/native-live-desktop-1.jpg`
- `build/visual-qa/native-live-desktop-2.jpg`

Normal launch command: `build/tecmo_port_game.exe --root . --play` after a
validated asset-pack build. This historical task predates the finish contract
and has no complete video/contact-sheet manifest; future R1 CPU/tip fidelity
work must supply that missing end-to-end reference proof before accepting the
broader criteria.

## Commits and merge instructions

- Base: `1885d17f9dbc9773159f5d967cc8e5208f296f9f`
- Sol branch/head: `codex/cpu-tipoff-behavior` /
  `11ed80cd5fd8733ed1c176ed897dfe9b1ae28402`
- Integrated into the visual branch by merge `2ea4a952`; subsequently present
  in main through `1caa6453`, `9979b136`, `63b29b04`, and current main
- Do not re-merge worker branches; their patches are already represented in main

Known approximation: high-level formation and goal-side marking are robust
native policy, not the original Bank04/Bank06 play-command lifecycle.

