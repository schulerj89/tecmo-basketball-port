# R4 Audio Foundation lineage

Round/task: `R4-AUDIO-FOUNDATION` / `OWN-R4-AUDIO-FOUNDATION`.

Acceptance status: **Sol acceptance pending**. This is the terminal Revision-C
lineage record; the isolated R4 foundation is ready for Sol review, while the
broader ACC-AUDIO claim remains incomplete.

## Task identities

| Task | Exact title/role | Model and thinking | State |
| --- | --- | --- | --- |
| `019fc822-bdfa-7ab1-8b35-e7d9aa58969d` | “Tecmo R4 Audio Foundation — Domain Orchestrator — Sol Max” — authoritative Sol | `gpt-5.6-sol/max` | acceptance owner; pending terminal acceptance |
| `019fc839-7677-7d93-abff-4aa427e7c6b3` | “Tecmo R4 Audio Foundation — Implementation and Revisions — Luna Max” — writable implementation/revision worker | `gpt-5.6-luna/max` | writer lineage; terminal docs and proof prepared |
| `019fc825-e59c-7002-8139-e3c6e2818318` | “Source Evidence Audit” — read-only audit | `gpt-5.6-luna/max` | accepted, unpinned |
| `019fc825-ec41-7993-8629-9c0948e08b4b` | “Native C and QA Audit” — read-only audit | `gpt-5.6-luna/max` | accepted, unpinned; Sol correction recorded |
| `019fc86b-1cc0-7f01-9abb-fa3e23598703` | “Tecmo R4 Audio Foundation — Independent QA — Luna Max” — detached QA worktree | `gpt-5.6-luna/max` | revised conditional pass; findings resolved by Revision C |

The audit correction is important: accepted TDMC serialization already requires
`pool_offset==0`; subtraction-form relational and queue-time checks are
defense-in-depth and do not describe a currently reachable mutated-offset
exploit.

## Workspace and durable checkpoints

- Worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r4-audio-foundation-luna`
- Branch: `codex/r4-audio-foundation-luna`
- Exact base/parent: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- Control-plane durable checkpoint: `3364e8b`; the reconciled audits and this
  writable lineage were durably registered there.
- Listening disposition checkpoint: `232e57e`; external human signoff resolved
  `BLOCK-R4-AUDIO-LISTENING-001` and completed/unpinned `S-BLOCKERS-001`.
- Earlier ruling/limitation checkpoint: `94acc9f`; it is retained as the
  honest pre-signoff limitation record, not as a claim of Sol auditory review.

No ROM, payload, capture, trace, WAV, proof event file, or other proprietary
artifact was created in tracked history. No merge, rebase, push, fork, or
additional worker task was performed by this lineage.

## Ordered commit lineage

The complete ordered history before Revision C is:

1. `29611607babe31415ab063520d832631ab3c2e4c` — R4 audio implementation,
   native tests, proof exporter, and initial finish material.
2. `51790b832eb4bb23db07ac7965d6c2b1da877a1e` — documentation and proof
   contract revision; the independent QA initial review points here.
3. `c8eb88a8155b1d00471c964646a7f56f89cc6540` — Sol documentation/review
   correction.
4. `4ddb1bf3f5fda1e207e14ed443367afb5796a644` — restored the exact public
   `>=8` declared-PRG-bank compatibility contract and direct undersized tests.
5. `f9499e43503e69cbbe2f16774d73a4964a34adfc` — deterministic proof gate:
   two-run identity, state/order/coverage, exact WAV, waveform, provenance,
   and golden assertions.
6. `ad82eb9ba34316b568b3ca33c80949657a973893` — corrected the TMUS7 tail
   accumulator expectation after canonical post-commit QA.

Revision C then adds:

7. `96471e42f79669379df0a573e8e82be037e559fb` — exact event-header/fixed-line
   enforcement and checked per-vector WAV PCM FNV-1a32 validation.
8. The terminal documentation record is the branch HEAD after this separate
   seven-document commit. Its exact SHA is deliberately not written into its
   own commit; after commit, the ignored proof manifest’s
   `proof_generation_head` and the Sol handoff record the exact SHA.

The Revision-C script commit was made while the worktree and tracked/index
state were clean. The documentation commit is likewise required to be clean
and linear; no self-referential SHA is fabricated in these files.

## QA lineage and recovery notes

Independent QA’s initial review was anchored at
`51790b832eb4bb23db07ac7965d6c2b1da877a1e`; its revised conditional pass was
anchored at `ad82eb9ba34316b568b3ca33c80949657a973893`. It found no runtime or
product defect. At that point all three domain suites, proof determinism,
provenance, ownership, and cleanliness were green; the remaining findings were
documentation terminal quality and proof header/FNV completeness. Revision C
resolves those findings.

The QA harness history is recorded honestly:

- Initial header, field-map, and FNV signedness assumptions were corrected
  during review.
- Two combined provenance probes were rejected by the shell wrapper before
  execution. They were split into safe probes; the real tracked/index state
  remained clean and the direct invalid-index gate subsequently rejected dirty
  provenance as required.
- The first post-commit proof run caught one expected-table typo for the emitted
  TMUS7 tail accumulator. The correction is the separate `ad82eb9` commit; the
  canonical artifacts did not change.

These are harness/review recoveries, not runtime defects. The final terminal
commands and results are in [TESTS.md](TESTS.md), and the ignored proof facts
and observations are in [PROOF.md](PROOF.md).

## Acceptance handoff

Sol must independently confirm the terminal branch, the ignored manifest
`proof_generation_head`, the exact hashes, the owned-path audit, and the
explicit approximation/deferred list. Until that is recorded, this lineage
must continue to say **Sol acceptance pending**. Acceptance of this isolated
foundation does not imply completion of cross-domain cue routing or full
ACC-AUDIO integration.
