# Tecmo R4 Audio Foundation

Status: terminal Revision C prepared; Sol acceptance remains pending.

Round/task: `R4-AUDIO-FOUNDATION` / `OWN-R4-AUDIO-FOUNDATION`.

This handoff records the isolated audio-foundation boundary: validated native
C players, semantic asset-pack import, deterministic output, and private proof
of the Rev1-derived music, frontend SFX, gameplay SFX, and DMC contracts. Normal
runtime consumes the validated pack. It does not read a ROM, decompilation,
emulator capture, trace, WAV, or proof event file.

## Scope and claim

The owned implementation covers:

- TMUS-1 music import, parser, sequencer, native synthesis, queueing, looping,
  clean termination, and audio-output transaction behavior.
- TFSX-1 frontend sound effects and TSFX-1 gameplay sound effects, including
  dry cue mapping, mailbox behavior, matching-channel override, and same-pack
  dependency identity.
- TDMC-1 DMC metadata, bounded sample playback, held-DAC behavior through end,
  retrigger, and clear-all.
- Strict Rev1 source gates, checked public-offset arithmetic, exact serialized
  postconditions, deterministic CLI proof, and ignored waveform evidence.

The isolated R4 foundation is `exact-high` for its declared boundaries where
the tests below say exact or strict. It is not a claim of nonlinear or
cycle-exact NES APU emulation. Broader `ACC-AUDIO` remains incomplete because
cross-domain cue call sites, full game integration, and the remaining native
runtime audio routing are outside this task's ownership.

| Criterion | Terminal classification |
| --- | --- |
| TMUS/TFSX/TSFX/TDMC serialized pack identity and Rev1 source validation | exact-high; strict postconditions and mutation gates pass |
| Direct renderer bounds, queue latch, output transactions, rollback, and frozen fallback | exact-high within the native API contract |
| Music cadence, track 7/8 termination, loop/no-drift checks, and mixed override | exact-high for the tested native model; not cycle-exact NES APU mixing |
| Frontend/gameplay cue mapping and DMC held-DAC state | exact-high at the declared boundary; DMC reader phase/IRQ is not claimed |
| Full ACC-AUDIO call-site routing and product-wide integration | incomplete/deferred; excluded cross-domain call sites were not changed |

Known audible approximations and deferred differences are listed in
[PROOF.md](PROOF.md) and are part of the acceptance contract: native PCM and
nonlinear/cycle-exact differences, DMC reader bit/cycle phase, unresolved DMC
IDs 0/1/2, neutral effect 5, bounded-correlation effect 6, and deferred
cross-domain cue routing/full ACC-AUDIO integration.

## Ownership and base

- Worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r4-audio-foundation-luna`
- Branch: `codex/r4-audio-foundation-luna`
- Exact base/expected parent: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- Terminal documentation record: branch HEAD after the separate Revision-C
  documentation commit. Its SHA is intentionally not written into its own
  commit; the post-commit ignored proof manifest and Sol handoff record it.
- Writable scope was limited to the audio headers/sources, owned asset-pack
  importers, three owned test scripts, `src/tecmo_cli_audio.c`, and this
  finish-task directory. No shared build/platform, call-site, README root,
  PORTING, or AGENTS file was changed.

No ROM, decoded payload, WAV, capture, trace, save state, event record, or other
proprietary artifact is tracked. The canonical Rev1 ROM is private test
evidence only; see [EVIDENCE.md](EVIDENCE.md).

## Lineage and acceptance

The authoritative Sol is task `019fc822-bdfa-7ab1-8b35-e7d9aa58969d`,
“Tecmo R4 Audio Foundation — Domain Orchestrator — Sol Max”,
`gpt-5.6-sol/max`. The writable worker is task
`019fc839-7677-7d93-abff-4aa427e7c6b3`,
“Tecmo R4 Audio Foundation — Implementation and Revisions — Luna Max”,
`gpt-5.6-luna/max`, on the branch/worktree above. Read-only source and native
audits, independent QA, control-plane checkpoints, ordered commits, and the
honest QA harness history are in [LINEAGE.md](LINEAGE.md).

Acceptance state is explicitly **Sol acceptance pending** until the Sol task
records acceptance. The isolated R4 foundation can be accepted independently;
that does not silently close the broader ACC-AUDIO work or authorize changes
outside the owned paths.

## Verification entry points

Use the local-private ROM through `TECMO_ROM_PATH` and run the commands in
[TESTS.md](TESTS.md). The terminal gameplay suite runs the hidden proof command
twice, validates exact event metadata and per-vector WAV FNV-1a32 slices, checks
both-run identity, verifies repository provenance before writing its script
manifest, and leaves all generated evidence below ignored `build/proof`.

The final ignored manifest is the authoritative record of the post-document
commit HEAD and proof hashes. [PROOF.md](PROOF.md) records the known terminal
identities and the reproducible vector/timestamp contract without embedding
private bytes.
