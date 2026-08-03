# R1-CPU-PLAY-LIFECYCLE task contract

Status: Sol-accepted `DRAFT_PASS` evidence is recorded; the CPU-only worker
commit and formal clean `-RequirePass`/final proof remain separate gates.

| Field | Value |
| --- | --- |
| Task | `R1-CPU-PLAY-LIFECYCLE` |
| Round | R1 CPU isolated lifecycle; LIVE integration is a later boundary |
| Authoritative Sol | `019fc61e-0f2a-7fb0-a76e-e4676808c959` |
| Worker branch | `codex/r1-cpu-play-lifecycle-luna` |
| Worker worktree | `C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna` |
| Expected base | `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` |
| Sol-recorded control-plane checkpoint (not a product commit) | `dea1fd7c2c2761fe08a6a27ab13a5e661e2b7094` |
| Current draft final SHA | `PENDING_FINAL_SHA_UNTIL_COMMIT` |
| Model/pin | `gpt-5.6-luna`, max thinking; no subagents or child tasks |

## Scope

This task ports the original CPU play lifecycle as an isolated native-C semantic
engine behind the existing TGAI-1 CPU-steering boundary. It covers validated
Rev1 command/formation/route evidence, five-byte fetch and stream transport,
bounded handler effects, fixed startup links and seeds, target fields, the
caller-supplied gates that are proven, and the CPU-side shot-request predicate.
The runtime consumes only native C and the validated semantic asset pack.

The tracked implementation is limited to the CPU-owned families named in the
delegation: CPU headers/sources, CPU asset-pack files, the CPU focused runner,
`tools/gameplay-lab/**`, and this task folder. TGAI-1 remains 7616 bytes with
FNV1a32 `D6C4DB35`; the source-map and import-layout contracts are untouched.

## Non-goals

- No normal production-scene consumption of the isolated play-stream engine;
  that is `R1-LIVE`.
- No complete CPU basketball policy, play-intent naming, pass/steal choice,
  shot outcome/release/make/miss parity, collision/contact ownership, or
  dynamic `$037F` reconstruction.
- No runtime ROM, ASM, decompilation, FCEUX capture, savestate, cheat, or loose
  asset-pack dependency.
- No edits to LIVE/TIP/scene paths, CLI/source-map/import-layout files,
  `PORTING.md`, `AGENTS.md`, or orchestration state.

## Contract files

- [EVIDENCE.md](EVIDENCE.md) -- exact source/ROM anchors and confidence boundary.
- [IMPLEMENTATION.md](IMPLEMENTATION.md) -- APIs, functions, handler matrix, and
  deferred later boundaries.
- [LINEAGE.md](LINEAGE.md) -- Sol/Luna history, revisions, review corrections,
  pins, and fault register.
- [TESTS.md](TESTS.md) -- exact commands, expected outputs, and draft results.
- [PROOF.md](PROOF.md) -- private original/native proof protocol and limitations.
- [proof-manifest.template.json](proof-manifest.template.json) -- generated-proof
  manifest shape with explicit pending fields.
- [MERGE.md](MERGE.md) -- final commit and Sol-owned merge placeholders.

## Acceptance boundary

The isolated lifecycle acceptance evidence is the strict importer/semantic
contract plus the accepted eleventh `DRAFT_PASS` source-pinned original trace.
Native PNGs, contact sheets, and video are continuity/regression evidence only.
The current production scene continues to use the previously accepted native
harness/formation approximation and does not consume these command offsets or
the isolated lifecycle state. Sol's source, trace, and visual inspection of
the draft evidence is recorded in `EVIDENCE.md`; formal clean `-RequirePass`
and final manifest acceptance remain pending.

## Legal and provenance boundary

The canonical Rev1 ROM and the read-only decompilation are research/import/test
inputs only. Their identity is checked before import; only validated semantic
constants and native data are retained in the pack/runtime. Local ROM,
decompilation, ASM, FCEUX, PNG, MP4, FM2, and generated pack outputs remain
outside the commit and under ignored/private paths. No private absolute path is
part of the runtime or proof manifest template.
