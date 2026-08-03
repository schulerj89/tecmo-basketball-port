# Per-Task Documentation Contract

Each domain or boundary task must commit `docs/finish-tasks/<task-id>/` before
its Sol orchestrator can mark it accepted. At minimum the folder contains:

- `README.md` — task ID, title, round, owner Sol, base SHA, branch, worktree,
  scope, non-goals, dependencies, and writable ownership.
- `EVIDENCE.md` — ROM revision/fingerprint, ASM bank/address/table/routine,
  source-map/asset fingerprints, original-frame/audio references, confidence,
  what each source proves, what remains inferred, and legal handling.
- `IMPLEMENTATION.md` — implementation summary, changed functions/modules,
  native-C and asset-pack boundaries, migrations, and known approximations.
- `LINEAGE.md` — Sol thread ID, every Luna thread ID as reported by the Sol,
  titles/models/thinking, worktrees/branches, commits, review findings,
  revision loops, bad-request recovery, and final accepted hashes.
- `TESTS.md` — exact commands, environment/reference prerequisites, final SHA,
  pass/fail counts, logs/manifests, and Sol-owned conclusions.
- `PROOF.md` — reproducible video/frame/audio commands and manifest locations;
  base/final SHA; asset-pack fingerprint; original reference and ROM revision;
  input script; resolution/sample rate; numbered frames/timestamps; contact
  sheet/waveform/event evidence; and Sol's personal visual/listening notes.
- `MERGE.md` — accepted branch and tip, expected base/parent, ordered commits,
  dependency and merge-order notes, conflict expectations, and exact staging
  instructions for the master.

The folder must also state:

1. final fidelity classification for each affected acceptance criterion;
2. every user-visible or audible difference still present;
3. why each approximation is justified;
4. whether proof is safe/committed or private/ignored;
5. confirmation that normal runtime consumes no ROM/ASM/capture/decomp data;
6. confirmation that the Sol personally inspected source, patch, tests, and
   relevant visual/audio proof.

Empty placeholders, Luna-only sign-off, a build-only claim, or an uncommitted
chat report do not satisfy this contract.
