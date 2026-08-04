# R2C defense/contact integration QA

Status: **ACCEPT** at signed pre-documentation integration tip
`32721888d9fd37b46c0ae2f529edf72c83844e88`.

This directory is the terminal acceptance record for task
`R2C-DEFENSE-CONTACT-INTEGRATION-QA`, session
`S-SOL-R2C-DEFENSE-CONTACT-INTEGRATION-QA-001`, claim
`OWN-R2C-DEFENSE-CONTACT-INTEGRATION-QA`, and lane
`LANE-R2C-DEFENSE-CONTACT-INTEGRATION-QA`. Authority is the Good-signed
master control commit `6f32bd8704f20422f4aa07c42b4ecaeb26a278b6`, whose parent is the
Good-signed reservation `bd3160a6d52f05c89d07a2314b590366573aa10b`, plus
Good-signed new-main authority
`2894a25c20532c642cc282408b69f28997b1166c`.

The exact signed docs-only terminal commit cannot record its own object ID.
The Sol handoff supplies that commit ID, tree, signature result, and final
guarded-main observation.

## Decision

- Integration disposition: **ACCEPT**.
- Sol terminal findings: P0 `0`, P1 `0`, P2 `0`.
- Sole independent Luna terminal findings: P0 `0`, P1 `0`, P2 `0`, P3 `0`.
- The first Luna documentation audit found one P3 in a non-PowerShell
  environment-assignment command shape. It is corrected in `PROOF.md`; the
  Sol handoff records the same Luna's corrected-tip confirmation.
- Warning-clean full MSVC build: pass, zero warning/error diagnostics.
- Focused defense/contact runner: pass from repository root and from an
  independent external caller, using isolated MSVC `/std:c11 /W4 /WX`.
- Direct and enclosing ROM fingerprints, raw oracles, rollback,
  repeatability, boundary hashes, cleanup, and zero-root-artifact gates: pass.
- Relevant initial- and reconciled-main gameplay, flow/Win32, asset-pack,
  season, and audio smokes: pass.
- Ownership, overlap, signature, ancestry, merge-tree, and no-registration
  checks: pass.
- Candidate/product correction: none required and none made.

## Acceptance anchors

1. Initial current-main input and first merge parent:
   `0ef11cf247e3110b6064e79a4c496be6346f3e13`.
2. Immutable accepted candidate and second merge parent:
   `ed70d884c3c75d900df589f442816c9566eb38df`, tree
   `dc989c39a9dd5c5a52325f16a9bc0ee06c3a9416`.
3. Candidate base `edf16ca9059158452798dbe5667f5e64ef444e39`
   and linear accepted commits `d5c5fa9b84cdce404751eda1a86e5507fc014656`,
   `9d6f0227f43ad476dcc3db008f43d7c5d830bb19`, and `ed70d884...`.
4. Initial authorized branch-only non-fast-forward merge:
   `ec4c0958519f78d033f4b49936d07bdd30a2a400`, ordered parents current main
   first and immutable candidate second, tree
   `960ccd3c7d3b5861a342caa783edcf0fd09a4c2f`.
5. Accepted new main `522909264f67673bd3242ecc62b343e1238bb142`,
   tree `adb18fceb477644a0cb98d3155ea67f1f9a24482`.
6. Additional authorized branch-only non-fast-forward reconciliation
   `32721888d9fd37b46c0ae2f529edf72c83844e88`, ordered parents R2C lineage
   `ec4c095...` first and accepted new main `5229092...` second, tree
   `7b8234fc3b45919ff59657d8dbb11c22f82a68dd`.
7. The signed docs-only terminal commit reported by the Sol handoff.

All checked controls and commits verify Good for `jaystar524@gmail.com` with
RSA key `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. Both
integration trees equal their independently predicted conflict-free ordered
merge trees.

## Accepted delta and seam

The immutable candidate adds exactly eight tracked text paths:

- `docs/finish-tasks/R2-defense-contact/EVIDENCE.md`
- `docs/finish-tasks/R2-defense-contact/LINEAGE.md`
- `docs/finish-tasks/R2-defense-contact/README.md`
- `docs/finish-tasks/R2-defense-contact/SCOPE.md`
- `docs/finish-tasks/R2-defense-contact/TEST-MANIFEST.md`
- `include/tecmo_gameplay_defense_contact.h`
- `src/tecmo_gameplay_defense_contact.c`
- `tools/Run-GameplayDefenseContactTests.ps1`

At initial takeover, the candidate had eight paths and then-current main had
55 from candidate base; their normalized overlap was zero and both diff checks
passed. Later stable-main movement `0ef11cf...5229092` was exactly 23 text
paths. Its normalized overlap with the eight R2C product paths was zero, and
its overlap with the two reserved draft-doc paths was also zero. Both movement
diff checks passed, ordered merge-tree prediction was conflict-free, and no
binary/proprietary payload or generated compiler artifact was introduced.
The reconciled tree preserves candidate blobs exactly and takes all disjoint
R2B/main blobs exactly from accepted new main.

The module is intentionally a pure raw/neutral standalone foundation. It is
absent from normal CMake/build, runtime, scene, CLI, and asset-pack
registration. The normal full build therefore proves current-main regression
safety but deliberately does not compile or exercise this module. Its focused
runner is the only compilation path accepted here.

No public or internal result vocabulary upgrades the raw routines to a
player-facing steal, block, rebound, recovery, foul, possession, scoring, or
completed contact outcome. Those semantic and runtime integrations remain
outside this task and require separate authority.

## Combined QA result

Sol personally read the five accepted candidate reports, complete public
header, complete C source, complete focused runner, and the original lifted
B06 `$B081-$B365` and B05 `$9968-$999E`/`$9A24-$9A5F` provenance contracts.
The raw scan order, wrapped arithmetic, strict threshold/tie behavior,
borrow-sensitive coordinate windows, raw `0x17` plan, validation-first
rollback, and helper-request-only `$C042` boundary agree with the source
contract.

The full command/result, provenance, hash, semantic-oracle, isolation, and
cleanup ledger is in `PROOF.md`. It includes the honest first NativeFlow
attempt that stopped because the ignored canonical asset pack was absent; the
pack was rebuilt through the supported CLI and the complete NativeFlow suite
then passed without a source or runner change.

After main advanced to `5229092...`, Sol reran a warning-clean full build, the
focused defense runner from both caller contexts, TGCS, TGSR, TPTI, complete
GameplayScene, direct scene/state, TGFL, TGFT, TGAI, 55/55 asset-pack, direct
and isolated flow, Win32 launch, music, frontend/gameplay audio, and season.
Every affected reconciled gate passed. Registration and cleanup checks again
found zero normal defense-module use, zero root compiler artifacts, and zero
remaining focused scratch directories.

## Independent QA

Exactly one top-level projectless independent reviewer was used: task
`019fcaa4-615e-7a41-b919-f001132bdcb9`, model `gpt-5.6-luna`, thinking
`max`. It reviewed signed merge `ec4c095...`, tree `960ccd3c...`, read every
accepted change, independently recomputed provenance and fingerprints, ran
the focused runner from both caller contexts, verified isolation and cleanup,
and returned **PASS with zero findings**. The same reviewer found the one P3
command-shape issue in the first docs draft, then returned PASS on the
corrected draft with P0 `0`, P1 `0`, P2 `0`, P3 `0`. The Sol handoff records
its final read-only verification of reconciled signed tip `32721888...` and
the terminal docs commit; no second Luna was created.

The reviewer explicitly made no normal-runtime, visual, audio, or
player-facing semantic acceptance claim. Those broad regression gates are
Sol evidence; the Luna verdict covers the signed lineage and bounded raw
foundation.

## Non-actions and guarded handoff

This lane performed no rebase, cherry-pick, force operation, destructive
clean, main switch, main mutation, staging mutation, candidate mutation,
remote push, or normal runtime registration. Generated executables and the
canonical test asset pack remain ignored below `build/`; focused compiler
scratch was removed and no compiler artifacts were created at repository
root.

At documentation time, local main, `origin/main`, and live remote main all
resolved to guarded SHA `522909264f67673bd3242ecc62b343e1238bb142`, which is
the second parent of reconciliation `32721888...` and an ancestor of the
accepted integration lineage. Master alone owns the final fast-forward and
ordinary push. If any main observation differs from that guarded SHA before
handoff, this acceptance must be reconciled and the affected gates rerun
before integration.
