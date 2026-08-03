# R4B frontend integration QA acceptance

Status: **ACCEPT** at the signed integration merge
`6b5d43546128408de8ab246d22f1b48322714183`.

This directory is the terminal acceptance record for
`OWN-R4B-INTEGRATION-QA`. Product/source inputs were treated as immutable. The
only tracked changes made by this task after the deliberate candidate merge are
the documents in this directory. The exact signed documentation commit cannot
record its own content-addressed SHA; that SHA and its `git verify-commit`
result are supplied in the signed Sol terminal handoff.

## Decision

- Integration disposition: **ACCEPT**.
- Sol findings: P0 `0`, P1 `0`, P2 `0`.
- Independent Luna findings: P0 `0`, P1 `0`, P2 `0`.
- Product rescope required: no.
- Current-main/candidate path overlap: `0` (`75` versus `19` normalized
  paths).
- Warning-clean MSVC `/W4` build: pass, `0` warnings.
- Canonical frontend suite: `29` tests, `1` intentional optional-reference
  skip, `0` failures.
- Direct asset-pack, arena-scene, and same-pack flow tests: pass.
- CPU, LIVE, season, music, frontend-audio, gameplay-audio, and Win32 launch
  integration gates: pass.
- Complete frontend proof: `3152` contiguous source frames and state rows,
  deterministic repeat video, exact accepted MP4 hash, and clean Sol visual
  review.
- Ownership/proprietary-artifact scan: pass.
- Tracked status at the pre-documentation checkpoint: clean.
- `main`, `origin/main`, and live `refs/heads/main` at the same checkpoint:
  `819b0e5eabca11683786e45474ca60329dff7f5f`; this task did not mutate any of
  them.

## Acceptance anchors

The immutable candidate commit is **unsigned**. This is deliberate and remains
explicit; it is not silently upgraded to signed provenance.

Acceptance is anchored by all of the following:

1. candidate SHA `757283edba5f87c2998b16e06bd1831e54ba04b5` and tree
   `81adcdec1559b34055406c8be4ea8d646bfb82f1`;
2. byte-exact SHA-256
   `B59B9C42A9DE8885C02C6DC6BA1545C70B47FDCA7EF2EBA42ECC09D9AE5F4725`
   for `docs/finish-tasks/R4-frontend-intro-title/recovery-sol-acceptance.md`
   as stored in that candidate;
3. cryptographically signed non-fast-forward merge
   `6b5d43546128408de8ab246d22f1b48322714183`, with ordered parents
   `819b0e5...` and `757283e...`, tree `4b9879f...`, and successful
   `git verify-commit`;
4. the cryptographically signed docs-only terminal commit reported in the Sol
   handoff, also verified with `git verify-commit`.

The good SSH signatures use identity `jaystar524@gmail.com` and RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Evidence map

- `LINEAGE.md`: exact Git graph, path accounting, immutable-input provenance,
  Luna registry, and master-only fast-forward procedure.
- `COMMANDS.md`: commands, results, proof reproduction shape, and corrected
  harness preconditions.
- `EVIDENCE.md`: hashes, negative coverage, cross-domain gates, complete
  frame/video proof, visual observations, ownership scan, and deferred limits.
- `INDEPENDENT-QA.md`: independent Luna scope, checks, severity closure, and
  pin/retirement lifecycle.

The sanitized ignored evidence record is
`build/proof/r4b-frontend-integration-6b5d435/R4B-PROOF-MANIFEST.json`, 23,191
bytes, SHA-256
`0AD3650262099E7B9DE2768DC47B921FB5F77E9DF254A4859A6E56E03691521B`.
It embeds no ROM bytes, private input paths, screenshots, or decoded
proprietary payloads.

## Scope and non-actions

- No hand edit was made to the 19 candidate product/test/document paths.
- No current-main product, build, registry, audio, season, CPU, LIVE, Win32,
  orchestration, root policy, or porting path was edited.
- Generated packs, PNGs, logs, manifests, contact sheets, WAV/MP4 proof, and
  other dynamic artifacts remain ignored below `build/`.
- No rebase, cherry-pick, force operation, destructive clean, worktree
  deletion, main merge, main push, or remote push was performed.
- No claim exceeds the accepted R4 report. Existing R4 approximations and
  residuals remain deferred exactly as described in `EVIDENCE.md`.

## Final handoff boundary

Only the master task may fast-forward and push `main`. The final Sol handoff
must first supply the terminal report SHA, verify its signature, recheck all
three main observations, confirm a clean worktree, and record the independent
Luna's exact unpin event. If main has moved from `819b0e5...`, the instructions
in `LINEAGE.md` are invalid until the master explicitly reconciles that
movement.
