# Independent terminal QA

Task `019fcb44-0f91-7632-9b25-88e51b505ce3`, title
`Tecmo R2 Rules Restarts Independent Terminal QA — Luna Max`, was the sole
terminal-QA lineage. It was created once as a top-level projectless
`gpt-5.6-luna/max` task and immediately pinned.

## Candidate verdict

`ACCEPT`. P0: none. P1: none. P2: none. P3: none.

QA independently verified:

- exact root, branch, HEAD `1dde1ef`, tree `a46d403`, sole parent `7fe2dd7`,
  Good SSH signature, clean status, and all diff checks;
- the exact seven-path ledger with no missing or unexpected path;
- production mutation wholly inside `scene_process_phase_audio` and unchanged
  audio dispatch order;
- SFX 3 at shot-clock expiry frame 0, no SFX 6 on frames 1-15, SFX 6 at frame
  16, exact-once behavior, and preserved restart/music policy;
- the four timing fixtures and eight OOB/backcourt restart fixtures;
- canonical and CMake console/GUI builds, full scene suite, six focused gates,
  console/GUI scene routes, and deterministic gameplay-state replay;
- canonical ROM identity without copying it into the worktree;
- ignored proof integrity, paired frame/video/audio hashes, and full-resolution
  contact-sheet appearance;
- final clean worktree/index and absence of ROM-like proof files.

The only QA diagnostics were invocation/environment issues recorded in
`FAULT-LEDGER.md`; none was a product or source finding.

## Classification accepted by QA

- Exact/ROM-derived: strict semantic assets and bounded source metadata.
- Native-faithful: the tested scene cue dispatch and restart integration.
- Native-approximate/incomplete: broader foul/contact ownership, original
  caller timing, blackout/fade alignment, and other excluded semantics.

QA did not widen the exactness claim or authorize any path change.

The same task remains pinned for the required review of the later Good-signed
Sol documentation tip. That later tip identity and disposition are supplied in
the authoritative external handoff to avoid a self-referential commit record.
