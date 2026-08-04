# R1B TIP integration QA

## Decision

This lane accepts the integrated R1 TIP candidate at signed branch merge
`564d83835258ab48b9ea2ebcc867ba41e185822f`, reconciled without conflict onto
the subsequently accepted main tip
`8a5b9928544a430efa34cbf98a248d6a8cbe7b14` by signed branch-only merge
`3aa7dfb523d6fee51785f845d023e7ea8a990074`. The accepted candidate remains
`e21f9a6621df5527544be1de4d0dc60382539c60`; the terminal report commit is
recorded in the master handoff.

The additional original-ASM/native-function traceability gate also passes.
The gate found one real guidance defect: two `AGENTS.md` statements and one
`PORTING.md` statement said equal errors choose Away even though the accepted
runtime defers equal claims. Master granted the exact guidance-only rescope at
signed control checkpoint
`9a3b4623022e3e4dc46142f5370f26c705bd9fe3`. Good-SSH-signed correction
`7ba0066ca1084e971a268d0b1b0176d065fdbd01` changes only those two files and
does not alter runtime, source-map, or test code.

## Scope

- Audit the immutable accepted R1 TIP candidate, signatures, reports, ownership,
  overlap, and precomputed merge tree.
- Make the one authorized signed no-fast-forward branch merge with current-main
  lineage first and candidate second.
- Run combined native build, focused TIP/TPTI-2, gameplay-scene, Win32,
  current-main cross-domain, deterministic proof, visual, malformed-input,
  ownership, and proprietary-artifact gates.
- Reverify the Rev1 ROM and requested Bank04/Bank05/fixed-bank ASM spans, map
  them to the exact native consumers, and preserve every evidence boundary.
- Obtain one projectless, read-only, pinned `gpt-5.6-luna` / `max`
  independent QA review and reuse that same task for the terminal records.
- Deliver only this task's reports plus the specifically authorized tie-policy
  guidance correction. Main integration and push remain master-owned.

## Result

- Candidate audit: four candidate commits and the merge verify Good SSH;
  candidate/current-main path overlap is `0`; Git-native precomputed tree
  `a08a66bb9edc858f7b87e72bff160c5cd8310186` matches the merge tree.
- Merge: `564d838...` has ordered parents `edf16ca...`, `e21f9a...` and
  exactly the precomputed tree.
- Live-main reconciliation: accepted main advanced to `8a5b992...`; its
  24-path delta and the R1B lineage had normalized overlap `0`. Signed merge
  `3aa7dfb...` has ordered parents `7ba0066...`, `8a5b992...` and exact
  precomputed tree `fb2e4cd08e5c20dfb5f4167853bd49ade6095780`.
- Build and automated QA: warning-clean full build PASS; focused TIP/TPTI-2
  PASS; broad gameplay-scene PASS; Win32 production launch PASS after building
  the required ignored root asset pack; intro, season, gameplay-audio, and
  CPU-steering cross-domain gates PASS.
- Reconciliation regression QA: warning-clean full build, direct gameplay
  state and asset-pack checks, all 55 asset-pack vectors, focused TIP,
  free-throw lineup, fatigue, broad gameplay-scene, native-flow, Win32, and
  season gates all PASS. No TIP/R2A overlap or regression was found.
- Proof: 65 contiguous 640x480 logical frames `661..725`, a second
  deterministic render, runtime logs, full-resolution contact/edge sheets, and
  away-left facing all PASS at reconciled tip `3aa7dfb...`. All 65 frames and
  all five media artifacts are byte-identical to both the pre-reconciliation
  proof and the accepted R1 proof.
- Original evidence: direct ROM rehash, lifted-source review, raw dispatcher
  table decode, TPTI-2 source-role/span audit, same-pack dependency audit, and
  ASM-to-native trace table PASS.
- Security/provenance: malformed, missing, oversized, cross-pack, stale-header,
  overlap/bounds/padding, source-mutation, false-friend, ownership, and
  proprietary-artifact gates fail closed or PASS as designed.
- Independent review: the same worker's first terminal-report pass was
  P0/P1/P2 `0/0/2` for two report-only findings; both exact corrections and
  the complete retry lineage are recorded in
  [INDEPENDENT-QA.md](INDEPENDENT-QA.md). The same-task exact-fix confirmation
  was **PASS — P0/P1/P2 `0/0/0`**. After reconciliation the same task was
  re-pinned, found and verified one report-only proof-root correction, and
  returned terminal **PASS — P0/P1/P2 `0/0/0`**. It was then unpinned.

## Fidelity boundary

The integrated behavior is source-grounded, but it is not advertised as a
cycle-exact ROM port.

- Source-pinned: Bank04 capture/error semantics and `$0761-$0764` state;
  Bank05 automatic, opposing-selected-actor, jump-commit, slot-10 dispatch and
  claim spans; fixed `$E537-$E542` ordering; fixed `$CD96-$CDAB` RNG seam;
  the exact TPTI-2 source/dependency fingerprints.
- Runtime-proven: native controlled sampling, strict automatic threshold,
  transactional commit/claim, equal-claim deferral, fail-closed winner query,
  resolved claimant handoff, deterministic production-path scene/proof, and
  away-left live orientation.
- Native-faithful/approximate: the deterministic 20/21/22 automatic calibration,
  velocity/raw-height bridge, presentation arc, and native frame-721 handoff.
  Frame 721 is not claimed as a ROM-exact handoff.
- Original-reference only: mapper-gated `$E56E` observations establish a
  recurring running-loop entry, not a native timing contract or a handoff
  counter.
- Incomplete: original single-winner tie settlement; jumper selector versus
  receiver/holder/team ownership; TTDT-to-`$7C48` mapping; complete original
  trajectory; and original close-up/pixel/timing parity.

`$8642` is a shared selected-actor function, not slot-10 TIP logic.
`$98E1-$9A5F` is later/general collision and settlement logic, not the TIP
claim routine. Neither is used as a false substitute for `$A274-$A2D5`.

## Non-goals

- No runtime, source-map, test, build, asset, or proof-runner edits.
- No claim that raw `$0380/$037F` selectors identify team, orientation,
  receiver, or holder.
- No claim that native presentation frames, close-up composition, or
  frame-721 transition are ROM-exact.
- No ROM, ASM, decompilation, emulator trace, capture, save state, or other
  proprietary artifact is committed.
- No checkout, merge, commit, push, force, rebase, or cherry-pick on
  `main` or `origin/main`.

## Records

- [COMMANDS.md](COMMANDS.md) records the personal commands, temporal results,
  and terminal gates.
- [EVIDENCE.md](EVIDENCE.md) contains the ASM/native traceability table,
  TPTI-2 provenance audit, proof hashes, and visual review.
- [INDEPENDENT-QA.md](INDEPENDENT-QA.md) records the sole Luna allocation,
  findings, retries, and terminal disposition.
- [LINEAGE.md](LINEAGE.md) records Git ancestry, master checkpoints, authorized
  scope, diagnostics, signing, and guarded fast-forward conditions.

## Personal signature

- Decision owner: `S-SOL-R1B-TIP-INTEGRATION-QA-001`
- Task/claim: `R1B-TIP-INTEGRATION-QA` /
  `OWN-R1B-TIP-INTEGRATION-QA`
- Decision: `ACCEPT`
- Date: `2026-08-03`
- Signed product merge: `564d83835258ab48b9ea2ebcc867ba41e185822f`
- Signed guidance correction: `7ba0066ca1084e971a268d0b1b0176d065fdbd01`
- Signed current-main reconciliation:
  `3aa7dfb523d6fee51785f845d023e7ea8a990074`
