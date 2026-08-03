# R4 audio foundation

Status: implementation and proof gates complete; Sol acceptance is pending.

This task delivers the native C audio foundation for music, gameplay SFX/DMC,
frontend SFX, and the Win32 waveOut fallback transaction policy. Runtime code
consumes validated semantic asset-pack entries; it does not read a ROM.

## Scope

- Transactional waveOut prefill/refill state handling with frozen fallback.
- Checked direct render counts, opening-queue retry behavior, DMC relational
  bounds, and exhaustive gameplay event mapping.
- Canonical same-pack selection for music/gameplay/frontend borrowed players,
  including real-pack alias and distinct-container coverage.
- Checked ROM importer offsets and exact Rev1 serialized postconditions for
  TMUS-1, TSFX-1, TDMC-1, and TFSX-1.
- A hidden developer proof command and deterministic PowerShell evidence gate.
- The seven-document finish contract in this directory.

Fidelity classification is recorded in [EVIDENCE.md](EVIDENCE.md). The
isolated R4 foundation is exact/high-confidence where its native semantic
contracts are fingerprinted; broader ACC-AUDIO remains incomplete because
cross-domain cue routing and full game integration are outside this task.

## Non-goals

Cross-domain cue call-site routing, shared source-map/import-layout changes,
build/platform boundary changes, nonlinear or cycle-exact NES APU mixing, and a
cycle-exact DMC reader phase are deferred. DMC clip IDs 0/1/2 remain
address-bound/unresolved; effect 5 remains neutral/unresolved and effect 6 is
only a bounded-correlation result. These are documented approximations, not
claims of original-hardware equivalence.

## Ownership and base

The writable ownership is exactly the audio headers/sources, the two owned
importers, the three owned PowerShell suites, and this documentation subtree.
No ROM, decoded payload, WAV, capture, trace, save state, or other proprietary
artifact is tracked. The expected parent/base is
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.

The authoritative orchestrator is task
`019fc822-bdfa-7ab1-8b35-e7d9aa58969d`, gpt-5.6-sol/max. The writable worker is
task `019fc839-7677-7d93-abff-4aa427e7c6b3`, titled
“Tecmo R4 Audio Foundation — Implementation and Revisions — Luna Max”,
gpt-5.6-luna/max. It used worktree
`C:\Users\joshs\Projects\tecmo-basketball-port-r4-audio-foundation-luna`
and branch `codex/r4-audio-foundation-luna`.

## Contract map

- [EVIDENCE.md](EVIDENCE.md) — source ranges, fingerprints, and confidence.
- [IMPLEMENTATION.md](IMPLEMENTATION.md) — native functions and behavior.
- [LINEAGE.md](LINEAGE.md) — task, audit, checkpoint, and commit lineage.
- [TESTS.md](TESTS.md) — commands and exact final results.
- [PROOF.md](PROOF.md) — ignored deterministic proof manifest and hashes.
- [MERGE.md](MERGE.md) — ordered commits and handoff instructions.
