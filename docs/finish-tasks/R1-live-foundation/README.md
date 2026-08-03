# R1 LIVE foundation

Status: independent terminal QA completed review; this is the docs-only QA
correction on top of `6a16422b02e6354bfaf67f731e7a0e5b05906a17`. QA reported
`P0=0`, `P1=0`, and `P2=2` docs-only. Final QA docs verification and ff-only
merge remain pending. The slice is implemented on
`codex/r1-live-foundation-luna` from parent
`ad0f005673692b04772bce3c3b4d3ac4b2624731`.

The slice binds selected TeamManagement starters by value, preserves the accepted direct-scene compatibility path, installs the Bank04 static startup values as the native-faithful/inferred post-tip LIVE start layout, synchronizes holder/possession/controller state, and integrates the accepted TGAI formation/play-state/one-step/shot-request contract through a transactional native adapter. The Bank04 values are exact table evidence; their reuse is not a proven first-running-clock snapshot. TIP, rendering, and `src/tecmo_gameplay_scene_shots.c` remain outside the change.

The implementation/proof gate passed Sol's independent precommit review. The
earlier worker capture at `build/live-proof-edge-review-20260803-c` remains
historical `DRAFT` evidence and is superseded for acceptance by the formal
`-RequirePass` record at `build/live-proof-formal-20260803-e`. Its manifest is
`PASS`, clean, and finalized at
`e2333db8fd0ad21c036d0016574c1551929fbb5c`. The formal proof passed all four
scene suites and all named negative regressions; independent QA has not yet
accepted the terminal chain. Reused and repinned QA Luna
`019fc765-8d36-7be2-b273-d5e617520061` independently verified the exact
three-commit worker chain and recorded proof at
`build/live-proof-independent-qa-20260803-f`, manifest SHA256
`A7E1D408FA273481B58B605A3D189E52BD5F2A8ADD99FF59900601B9ABD6CE00`.

Read [SCOPE.md](SCOPE.md) for boundaries, [EVIDENCE.md](EVIDENCE.md) for source classification, [IMPLEMENTATION.md](IMPLEMENTATION.md) for changed seams, and [MERGE.md](MERGE.md) for the eventual ff-only handoff.
